#include "Character/TBVoiceComponent.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/TBBox.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

UTBVoiceComponent::UTBVoiceComponent(const FObjectInitializer& Init) : Super(Init)
{
	// 必要時のみTickする。
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTBVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// PlayerStateの到着待ちと、ラウンドごとの陣営の変化に追従するため定期チェック
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &UTBVoiceComponent::UpdateVoice, .5f, true, 0.f);
}

void UTBVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	Super::EndPlay(EndPlayReason);
}

void UTBVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateMirror();
}

void UTBVoiceComponent::UpdateVoice()
{
	auto* Pawn = Cast<APawn>(GetOwner());
	auto* Speaker = Pawn ? Pawn->GetPlayerState<ATBPlayerState>() : nullptr;
	if (!Speaker || !Speaker->GetUniqueId().IsValid())
	{
		return;
	}
	// 自分の声は自分のPCでは鳴らさない。
	if (Pawn->IsLocallyControlled())
	{
		GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
		return;
	}
	if (!Attenuation)
	{
		Attenuation = NewObject<USoundAttenuation>(this);
		Attenuation->Attenuation.AttenuationShapeExtents = FVector::ZeroVector; // 既定の全音量域（半径400）を消す。
		Settings.AttenuationSettings = Attenuation;
		RegisterWithPlayerState(Speaker);
	}
	const auto* Controller = GetWorld()->GetFirstPlayerController();
	const auto* Listener = Controller ? Controller->GetPlayerState<ATBPlayerState>() : nullptr;
	const bool bToyToHuman = Speaker->Team == ETBTeam::Toy && Listener && Listener->Team == ETBTeam::Human;
	// 収納部屋は箱から離れた別室なので、中と外の間の声は箱越しに聞かせる。
	const bool bSpeakerBoxed = Speaker->ToyState == ETBToyState::Boxed;
	const bool bListenerBoxed = Listener && Listener->ToyState == ETBToyState::Boxed;
	const ATBBox* WallBox = bSpeakerBoxed != bListenerBoxed ? FindBox() : nullptr;
	const bool bIntoBox = WallBox && bListenerBoxed;
	if (bIntoBox && !Mirror)
	{
		Mirror = NewObject<USceneComponent>(Pawn, TEXT("VoiceMirror"));
		Mirror->SetupAttachment(Pawn->GetRootComponent());
		Mirror->SetUsingAbsoluteLocation(true);
		Mirror->RegisterComponent();
	}
	SetComponentTickEnabled(bIntoBox);
	UpdateMirror();
	Settings.ComponentToAttachTo = bIntoBox ? Mirror.Get() : WallBox ? WallBox->Visual.Get() : Pawn->GetRootComponent();
	auto& Shape = Attenuation->Attenuation;
	Shape.FalloffDistance = WallBox ? BoxedHearingDistance : bToyToHuman ? ToyToHumanHearingDistance : HearingDistance;
	Shape.bAttenuateWithLPF = WallBox != nullptr;
	Shape.LPFRadiusMin = 0.f;
	Shape.LPFRadiusMax = BoxedHearingDistance;
	Shape.LPFFrequencyAtMin = BoxedMuffleFrequency;
	Shape.LPFFrequencyAtMax = BoxedMuffleFrequency;
}

void UTBVoiceComponent::UpdateMirror()
{
	const auto* Controller = GetWorld()->GetFirstPlayerController();
	const APawn* ListenerPawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Mirror || !Box.IsValid() || !ListenerPawn)
	{
		return;
	}
	// 箱から見た話し手の位置を、聞き手の周りに写す。聞き手との距離が箱との距離と一致する。
	const FVector FromBox = GetOwner()->GetActorLocation() - Box->Visual->GetComponentLocation();
	Mirror->SetWorldLocation(ListenerPawn->GetActorLocation() + FromBox);
}

ATBBox* UTBVoiceComponent::FindBox()
{
	// 箱はマップに1つの前提（ATBBox::HasPrisonersと同じ）。
	if (!Box.IsValid())
	{
		TActorIterator<ATBBox> It(GetWorld());
		Box = It ? *It : nullptr;
	}
	return Box.Get();
}
