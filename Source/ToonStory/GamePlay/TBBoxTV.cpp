#include "TBBoxTV.h"
#include "TBBox.h"
#include "TBTVCamera.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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
}

void ATBBoxTV::BeginPlay()
{
	Super::BeginPlay();
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
	DOREPLIFETIME(ATBBoxTV, SourceBox);
	DOREPLIFETIME(ATBBoxTV, Channel);
	DOREPLIFETIME(ATBBoxTV, bPowerOn);
}

bool ATBBoxTV::HasLocalViewer() const
{
	if (!TB::Playing(GetWorld()) || !IsValid(SourceBox))
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

// 人間はいつでも映せる。おもちゃは掴まれて運ばれている間も映し、箱にしまわれたら砂嵐。
bool ATBBoxTV::IsAvailableTarget(const ATBPlayerState* Player) const
{
	const auto* State = TB::GS(GetWorld());
	return IsValid(Player) && State && State->PlayerArray.Contains(Player) &&
	       IsValid(Player->GetPawn<ATBCharacter>()) &&
	       (Player->Team == ETBTeam::Human || (Player->Team == ETBTeam::Toy && Player->ToyState != ETBToyState::Boxed));
}

bool ATBBoxTV::CanOperate(const ATBCharacter* Viewer) const
{
	const auto* Info = IsValid(Viewer) ? Viewer->TBPS() : nullptr;
	return HasAuthority() && TB::Playing(GetWorld()) && Info && Info->Team == ETBTeam::Toy &&
	       Info->ToyState == ETBToyState::Boxed;
}

// 箱にいるおもちゃを除いた全員を、人間→おもちゃの順に並べる。捕まった人が増えると番号は詰まる。
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
	OnRep_Channel(); // リッスンサーバーのローカル視聴者にも切替演出を適用する。
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
	bPowerOn = !bPowerOn;
	ForceNetUpdate();
	return true;
}

void ATBBoxTV::OnRep_Channel()
{
	NoiseUntil = GetWorld()->GetTimeSeconds() + FMath::Clamp(NoiseSeconds, 0.f, 2.f);
	if (bViewing)
	{
		ScreenInstance->SetScalarParameterValue(TEXT("Noise"), 1.f);
	}
}

void ATBBoxTV::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !bPowerOn && !TB::Playing(GetWorld()))
	{
		bPowerOn = true; // 次の試合は電源ONから始める。
	}
	SetViewing(GetNetMode() != NM_DedicatedServer && bPowerOn && HasLocalViewer());
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
		Capture->HiddenActors.Reset();
		return;
	}
	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->RenderTargetFormat = RTF_RGBA8;
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->InitAutoFormat(FMath::Clamp(CaptureResolution.X, 64, 2048),
		                             FMath::Clamp(CaptureResolution.Y, 64, 2048));
		Capture->TextureTarget = RenderTarget;
		ScreenInstance->SetTextureParameterValue(TEXT("Feed"), RenderTarget);
	}
	NoiseUntil = GetWorld()->GetTimeSeconds() + FMath::Clamp(NoiseSeconds, 0.f, 2.f);
	ScreenInstance->SetScalarParameterValue(TEXT("Noise"), 1.f);
	FTimerManagerTimerParameters TimerParameters;
	TimerParameters.bLoop = true;
	// フレーム落ち時に、遅れた回数分を同一フレームでまとめ撮りしない。
	TimerParameters.bMaxOncePerFrame = true;
	GetWorldTimerManager().SetTimer(CaptureTimer, this, &ATBBoxTV::CaptureFrame,
	                                1.f / FMath::Clamp(CaptureRate, 1.f, 30.f), TimerParameters);
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
	// 捕まった・切断した・その番号の相手がいない場合は砂嵐。
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
	return true;
}

void ATBBoxTV::CaptureFrame()
{
	if (!bPowerOn || !HasLocalViewer())
	{
		SetViewing(false);
		return;
	}
	if (GetWorld()->GetTimeSeconds() < NoiseUntil || !SetCaptureView())
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
