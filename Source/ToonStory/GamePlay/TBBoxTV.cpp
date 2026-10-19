// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBBoxTV.h"
#include "Audio/TBAudioEvents.h"
#include "Audio/TBAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "TBTVCamera.h"
#include "TBBox.h"
#include "Character/TBCharacter.h"
#include "Components/BoxComponent.h"
#include "Data/TBPresentationData.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ATBBoxTV::ATBBoxTV()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(5); // どうせBPで上書きできるかつチャンネルと電源しかRepするものがないので、低頻度で十分そう。
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(GetRootComponent());
	Frame->SetCollisionProfileName(TEXT("BlockAll"));
	Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
	Screen->SetupAttachment(GetRootComponent());
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Screen->SetCastShadow(false);
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(GetRootComponent());
	Capture->SetAbsolute(true, true, true);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	ChannelOSD = CreateDefaultSubobject<UWidgetComponent>(TEXT("ChannelOSD"));
	ChannelOSD->SetupAttachment(Screen);
	ChannelOSD->SetWidgetSpace(EWidgetSpace::World);
	ChannelOSD->SetDrawSize(FVector2D(320, 96));
	ChannelOSD->SetBlendMode(EWidgetBlendMode::Transparent);
	ChannelOSD->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChannelOSD->SetCastShadow(false);
	ChannelOSD->SetRenderInMainPass(false);
	ChannelOSD->SetRenderInDepthPass(false);
	ChannelOSD->SetTickWhenOffscreen(true);
}

void ATBBoxTV::BeginPlay()
{
	Super::BeginPlay();
	ChannelOSD->SetComponentTickEnabled(false); // 映るまで止めておく
	if (GetNetMode() != NM_DedicatedServer && ScreenMaterial)
	{
		ScreenInstance = UMaterialInstanceDynamic::Create(ScreenMaterial, this);
		Screen->SetMaterial(ScreenMaterialIndex, ScreenInstance);
		ScreenInstance->SetScalarParameterValue(TEXT("Power"), 0.f);
	}
}

void ATBBoxTV::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBBoxTV, Channel);
	DOREPLIFETIME(ATBBoxTV, bPowerOn);
}

bool ATBBoxTV::HasLocalViewer() const
{
	if (!TB::Playing(GetWorld()))
	{
		return false;
	}
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const auto* PC = It->Get();
		const auto* Player = PC ? PC->GetPlayerState<ATBPlayerState>() : nullptr;
		if (PC && PC->IsLocalController() && PC->GetPawn() && Player && Player->Team == ETBTeam::Toy &&
		    Player->ToyState == ETBToyState::Boxed)
		{
			return true;
		}
	}
	return false;
}

// 目の位置は落下エリアの床から求める。入った直後は、本人のPCに届いている自分の位置がまだ箱の外のことがある。
// 収納と運搬の解除は、クライアントに届く順もTickの順も決まっていない。運搬カメラの解除で視点が水平に戻されるので、
// それが済むまでは向けずに false を返し、次のTickでやり直す。
bool ATBBoxTV::FaceArrival() const
{
	const UBoxComponent* Area = ATBBox::FindDropArea(GetWorld());
	if (!Area)
	{
		return true;
	}
	TArray<APlayerController*, TInlineAllocator<2>> Arrivals;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		const auto* Player = PC ? PC->GetPlayerState<ATBPlayerState>() : nullptr;
		const auto* Pawn = PC ? PC->GetPawn<ATBCharacter>() : nullptr;
		if (PC && PC->IsLocalController() && Pawn && Player && Player->ToyState == ETBToyState::Boxed)
		{
			if (Pawn->IsCarryViewActive())
			{
				return false;
			}
			Arrivals.Add(PC);
		}
	}
	const FVector Floor = Area->GetComponentLocation() - Area->GetUpVector() * Area->GetScaledBoxExtent().Z;
	const FVector LookAt =
	    Screen->Bounds.Origin + FVector(0.f, 0.f, TB::PresentationData().Television.ArrivalLookAbove);
	for (APlayerController* PC : Arrivals)
	{
		const FVector Eye = Floor + FVector(0.f, 0.f, PC->GetPawn()->BaseEyeHeight);
		PC->SetControlRotation((LookAt - Eye).Rotation());
	}
	return true;
}

// 人間はいつでも映せる。おもちゃは掴まれた時点で砂嵐にし、降ろされて自由になれば再び映す。
bool ATBBoxTV::IsAvailableTarget(const ATBPlayerState* Player) const
{
	const auto* State = TB::GS(GetWorld());
	return IsValid(Player) && State && State->PlayerArray.Contains(Player) &&
	       IsValid(Player->GetPawn<ATBCharacter>()) &&
	       (Player->Team == ETBTeam::Human || (Player->Team == ETBTeam::Toy && Player->ToyState == ETBToyState::Free));
}

bool ATBBoxTV::CanOperate(const ATBCharacter* Viewer) const
{
	const auto* Info = IsValid(Viewer) ? Viewer->TBPS() : nullptr;
	return HasAuthority() && TB::Playing(GetWorld()) && Info && Info->Team == ETBTeam::Toy &&
	       Info->ToyState == ETBToyState::Boxed;
}

// 箱にいるおもちゃを除いた全員を、人間→おもちゃの順に並べる。箱に入る人が増えると番号は詰まる（掴まれただけでは詰めない）。
TArray<ATBPlayerState*> ATBBoxTV::GetPlayerChannels() const
{
	TArray<ATBPlayerState*> Players;
	for (APlayerState* Player : TB::GS(GetWorld())->PlayerArray)
	{
		auto* Info = Cast<ATBPlayerState>(Player);
		if (Info &&
		    (Info->Team == ETBTeam::Human || (Info->Team == ETBTeam::Toy && Info->ToyState != ETBToyState::Boxed)))
		{
			Players.Add(Info);
		}
	}
	Players.Sort(
	    [](const ATBPlayerState& A, const ATBPlayerState& B)
	    {
		    const bool bAHuman = A.Team == ETBTeam::Human;
		    return bAHuman != (B.Team == ETBTeam::Human) ? bAHuman : A.GetPlayerId() < B.GetPlayerId();
	    });
	return Players;
}

// 置かれている固定カメラを都度探す。置くだけで登録され、試合中に増減しても追従する。
ATBTVCamera* ATBBoxTV::FindCamera(int32 Number) const
{
	for (TActorIterator<ATBTVCamera> It(GetWorld()); It; ++It)
	{
		if (It->Channel == Number)
		{
			return *It;
		}
	}
	return nullptr;
}

bool ATBBoxTV::HasPicture(int32 Number, const TArray<ATBPlayerState*>& Players) const
{
	if (Number < CameraChannels)
	{
		return FindCamera(Number) != nullptr;
	}
	return Players.IsValidIndex(Number - CameraChannels) && IsAvailableTarget(Players[Number - CameraChannels]);
}

void ATBBoxTV::SetChannel(int32 Number)
{
	const auto Players = GetPlayerChannels();
	Channel.Number = Number;
	Channel.Target = Players.IsValidIndex(Number - CameraChannels) ? Players[Number - CameraChannels] : nullptr;
	++Channel.Revision;
	ForceNetUpdate();
	UTBAudioEvents::Emit(this, ETBSoundEvent::TVChannel, nullptr, FVector::ZeroVector);
	UTBAudioEvents::Emit(this, ETBSoundEvent::TVGlitch, nullptr, FVector::ZeroVector);
	OnRep_Channel(); // リッスンサーバーのローカル視聴者にも切替演出を適用する。
}

// 誰も見ていない時に戻すので、切替音や砂嵐は出さない。
void ATBBoxTV::ResetChannel()
{
	Channel.Number = 0;
	Channel.Target = nullptr;
	++Channel.Revision;
	ForceNetUpdate();
}

bool ATBBoxTV::SelectChannel(ATBCharacter* Viewer, int32 Number)
{
	if (!CanOperate(Viewer) || !bPowerOn || Number < 0 || Number > MaxChannel)
	{
		return false;
	}
	SetChannel(Number);
	return true;
}

// 何か映る番号だけを巡回する。
bool ATBBoxTV::StepChannel(ATBCharacter* Viewer, int32 Direction)
{
	if (!CanOperate(Viewer) || !bPowerOn || Direction == 0)
	{
		return false;
	}
	const auto Players = GetPlayerChannels();
	TArray<int32> Watchable;
	for (int32 Number = 0; Number <= MaxChannel; ++Number)
	{
		if (HasPicture(Number, Players))
		{
			Watchable.Add(Number);
		}
	}
	if (Watchable.IsEmpty())
	{
		return false;
	}
	int32 Next = Direction > 0 ? Watchable[0] : Watchable.Last();
	for (int32 Offset = 0; Offset < Watchable.Num(); ++Offset)
	{
		const int32 Candidate = Watchable[Direction > 0 ? Offset : Watchable.Num() - 1 - Offset];
		if (Direction > 0 ? Candidate > Channel.Number : Candidate < Channel.Number)
		{
			Next = Candidate;
			break;
		}
	}
	SetChannel(Next);
	return true;
}

bool ATBBoxTV::TogglePower(ATBCharacter* Viewer)
{
	if (!CanOperate(Viewer))
	{
		return false;
	}
	SetPowerOn(!bPowerOn);
	UTBAudioEvents::Emit(this, bPowerOn ? ETBSoundEvent::TVPowerOn : ETBSoundEvent::TVPowerOff, nullptr, FVector::ZeroVector);
	return true;
}

void ATBBoxTV::SetPowerOn(bool bOn)
{
	bPowerOn = bOn;
	ForceNetUpdate();
	OnRep_PowerOn(); // リッスンサーバーのローカルにも通知する。
}

void ATBBoxTV::OnRep_PowerOn()
{
	OnPowerChanged.Broadcast();
}

void ATBBoxTV::OnRep_Channel()
{
	NoiseUntil = GetWorld()->GetTimeSeconds() + FMath::Clamp(TB::PresentationData().Television.NoiseSeconds, 0.f, 2.f);
	if (bViewing)
	{
		ScreenInstance->SetScalarParameterValue(TEXT("Noise"), 1.f);
		UpdateChannelOSD();
	}
}

void ATBBoxTV::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// 箱に誰もいない間は、次に入った人が電源ON・0番から見始められるように戻す。
	const auto* State = TB::GS(GetWorld());
	if (HasAuthority() && (!TB::Playing(GetWorld()) || !State || State->BoxedCount == 0))
	{
		if (!bPowerOn)
		{
			SetPowerOn(true);
		}
		if (Channel.Number != 0 || Channel.Target)
		{
			ResetChannel();
		}
	}
	// 箱に入れられたら一度だけ、本人の画面をテレビの少し上へ向ける。向きは本人のPCが持っているので、ここで決める。
	const bool bLocalBoxed = GetNetMode() != NM_DedicatedServer && HasLocalViewer();
	bFacedArrival = bLocalBoxed && (bFacedArrival || FaceArrival());
	SetViewing(bPowerOn && bLocalBoxed);
	// テクスチャは最初の描画で作られるので、できるまでTickで試す。
	if (bViewing && !bOSDBound)
	{
		BindChannelOSD();
	}
}

void ATBBoxTV::SetViewing(bool bEnable)
{
	if (bEnable == bViewing || !ScreenInstance)
	{
		return;
	}
	bViewing = bEnable;
	ScreenInstance->SetScalarParameterValue(TEXT("Power"), bViewing ? 1.f : 0.f);
	GetWorldTimerManager().ClearTimer(CaptureTimer);
	if (!bViewing)
	{
		bWasStatic = false;
		Capture->HiddenActors.Reset();
		ChannelOSD->SetComponentTickEnabled(false);
		return;
	}
	const auto& Television = TB::PresentationData().Television;
	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->RenderTargetFormat = RTF_RGBA8;
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->InitAutoFormat(FMath::Clamp(Television.CaptureResolution.X, 64, 2048),
		                             FMath::Clamp(Television.CaptureResolution.Y, 64, 2048));
		Capture->TextureTarget = RenderTarget;
		ScreenInstance->SetTextureParameterValue(TEXT("Feed"), RenderTarget);
	}
	NoiseUntil = GetWorld()->GetTimeSeconds() + FMath::Clamp(Television.NoiseSeconds, 0.f, 2.f);
	ScreenInstance->SetScalarParameterValue(TEXT("Noise"), 1.f);
	FTimerManagerTimerParameters TimerParameters;
	TimerParameters.bLoop = true;
	// フレーム落ち時に、遅れた回数分を同一フレームでまとめ撮りしない。
	TimerParameters.bMaxOncePerFrame = true;
	GetWorldTimerManager().SetTimer(CaptureTimer, this, &ATBBoxTV::CaptureFrame,
	                                1.f / FMath::Clamp(Television.CaptureRate, 1.f, 30.f), TimerParameters);
	ChannelOSD->SetComponentTickEnabled(true);
	UpdateChannelOSD();
}

bool ATBBoxTV::SetCaptureView()
{
	Capture->HiddenActors.Reset();
	Capture->HiddenActors.Add(this); // 自分の画面を撮り込まない。
	if (Channel.Number < CameraChannels)
	{
		const auto* CameraActor = FindCamera(Channel.Number);
		if (!CameraActor)
		{
			return false;
		}
		const auto* Camera = CameraActor->GetCameraComponent();
		Capture->SetWorldLocationAndRotation(Camera->GetComponentLocation(), Camera->GetComponentRotation());
		Capture->FOVAngle = Camera->FieldOfView;
		return true;
	}
	// 掴まれた・箱にいる・切断した・その番号の相手がいない場合は砂嵐。
	if (!IsAvailableTarget(Channel.Target))
	{
		return false;
	}
	const auto* Character = Channel.Target->GetPawn<ATBCharacter>();
	// ルートのネット補正による段差を避け、表示メッシュの補間分だけ目の位置も補正する。
	const FVector Smoothing = Character->GetMesh()->GetComponentLocation() -
	                          Character->GetActorTransform().TransformPosition(Character->GetBaseTranslationOffset());
	Capture->SetWorldLocationAndRotation(Character->GetPawnViewLocation() + Smoothing, Character->GetBaseAimRotation());
	Capture->FOVAngle = Character->Camera->FieldOfView;
	Capture->HiddenActors.Add(const_cast<ATBCharacter*>(Character));
	// 掴まれたおもちゃの子アクターは、掴んだ人の手に付いているので、TVに映らないように隠す。
	TInlineComponentArray<UChildActorComponent*> Visuals(Character);
	for (const auto* Visual : Visuals)
	{
		if (auto* Child = Visual->GetChildActor())
		{
			Capture->HiddenActors.Add(Child);
		}
	}
	return true;
}

void ATBBoxTV::CaptureFrame()
{
	if (!bPowerOn || !HasLocalViewer())
	{
		SetViewing(false);
		return;
	}
	const bool bSwitching = GetWorld()->GetTimeSeconds() < NoiseUntil;
	const bool bStatic = !bSwitching && !SetCaptureView();
	if (bStatic && !bWasStatic)
	{
		if (auto* Audio = GetGameInstance()->GetSubsystem<UTBAudioSubsystem>()) Audio->PlaySoundEvent(ETBSoundEvent::TVStatic);
	}
	bWasStatic = bStatic;
	if (bSwitching || bStatic)
	{
		ScreenInstance->SetScalarParameterValue(TEXT("Noise"), 1.f);
		return;
	}
	Capture->CaptureScene();
	ScreenInstance->SetScalarParameterValue(TEXT("Noise"), 0.f);
}

void ATBBoxTV::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CaptureTimer);
	Capture->TextureTarget = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ATBBoxTV::UpdateChannelOSD()
{
	// WBPのTextBlockは「Label」という名前にしておく。
	const auto* Widget = ChannelOSD->GetWidget();
	if (!Widget)
	{
		return;
	}
	auto* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Label")));
	if (!Label)
	{
		UE_LOG(LogTemp, Warning, TEXT("ChannelOSD に Label という TextBlock が見つからない"));
		return;
	}
	Label->SetText(FText::Format(NSLOCTEXT("TBBoxTV", "ChannelOSD", "CH {0}"), Channel.Number));
}

void ATBBoxTV::BindChannelOSD()
{
	auto* Target = ChannelOSD->GetRenderTarget();
	if (!Target || !ScreenInstance)
	{
		return;
	}
	ScreenInstance->SetTextureParameterValue(TEXT("OSD"), Target);
	ScreenInstance->SetScalarParameterValue(TEXT("OSDOpacity"), 1.f);
	bOSDBound = true;
}
