#include "Character/TBVoiceComponent.h"
#include "Audio/TBPitchShift.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/TBBox.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

static TAutoConsoleVariable<float> CVarForcePitch(TEXT("TB.Voice.ForcePitch"), 0.f,
                                                  TEXT("0以外なら、陣営に関係なく他人の声をこの半音数だけずらす（確認用）。"),
                                                  ECVF_Cheat);

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
		Pitch = NewObject<UTBPitchShiftPreset>(this);
		PitchChain = NewObject<USoundEffectSourcePresetChain>(this);
		FSourceEffectChainEntry Entry;
		Entry.Preset = Pitch;
		PitchChain->Chain.Add(Entry);
		Settings.SourceEffectChain = PitchChain;
		RegisterWithPlayerState(Speaker);
	}
	const auto* Controller = GetWorld()->GetFirstPlayerController();
	const auto* Listener = Controller ? Controller->GetPlayerState<ATBPlayerState>() : nullptr;
	const ETBTeam ListenerTeam = Listener ? Listener->Team : ETBTeam::None;
	FTBPitchShiftSettings PitchSettings;
	const float ForcedPitch = CVarForcePitch.GetValueOnGameThread();
	PitchSettings.Semitones = ForcedPitch != 0.f ? ForcedPitch : PitchFor(Speaker->Team, ListenerTeam);
	PitchSettings.DelayLength = PitchDelayLength;
	if (PitchSettings.Semitones != Pitch->Settings.Semitones || PitchSettings.DelayLength != Pitch->Settings.DelayLength)
	{
		Pitch->Settings = PitchSettings;
		Pitch->SetSettings(PitchSettings);
	}
	PitchChain->Chain[0].bBypass = FMath::IsNearlyZero(PitchSettings.Semitones);
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
	Shape.FalloffDistance = WallBox ? BoxedDistance : DistanceFor(Speaker->Team, ListenerTeam);
	Shape.DistanceAlgorithm = FalloffModel;
	Shape.CustomAttenuationCurve = FalloffCurve;
	Shape.bAttenuateWithLPF = WallBox != nullptr;
	Shape.LPFRadiusMin = 0.f;
	Shape.LPFRadiusMax = BoxedDistance;
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

float UTBVoiceComponent::DistanceFor(ETBTeam SpeakerTeam, ETBTeam ListenerTeam) const
{
	const bool bSpeakerToy = SpeakerTeam == ETBTeam::Toy;
	if (ListenerTeam == ETBTeam::Toy)
	{
		return bSpeakerToy ? ToyToToyDistance : SpeakerTeam == ETBTeam::Human ? HumanToToyDistance : HumanToHumanDistance;
	}
	return bSpeakerToy && ListenerTeam == ETBTeam::Human ? ToyToHumanDistance : HumanToHumanDistance;
}

float UTBVoiceComponent::PitchFor(ETBTeam SpeakerTeam, ETBTeam ListenerTeam) const
{
	if (SpeakerTeam == ETBTeam::Human && ListenerTeam == ETBTeam::Toy)
	{
		return HumanPitchForToys;
	}
	return SpeakerTeam == ETBTeam::Toy && ListenerTeam == ETBTeam::Human ? ToyPitchForHumans : 0.f;
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
