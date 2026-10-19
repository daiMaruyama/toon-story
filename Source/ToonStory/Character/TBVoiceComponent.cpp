// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Character/TBVoiceComponent.h"
#include "Audio/TBPitchShift.h"
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Data/TBRuleData.h"
#include "GamePlay/TBBox.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/VoiceInterface.h"
#include "OnlineSubsystemUtils.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

static TAutoConsoleVariable<float> CVarForcePitch(TEXT("TB.Voice.ForcePitch"), 0.f,
                                                  TEXT("0以外なら、陣営に関係なく他人の声をこの半音数だけずらす（確認用）。"),
                                                  ECVF_Cheat);

// 声と耳を置く場所。ルートはネット更新で飛び飛びに動くので、補間されるメッシュを使う。
static USceneComponent* VoiceAnchor(const APawn* Pawn)
{
	const auto* Character = Cast<ACharacter>(Pawn);
	return Character && Character->GetMesh() ? Character->GetMesh() : Pawn->GetRootComponent();
}

static UTBAudioSubsystem* VoiceAudioSubsystem(UWorld* World)
{
	const auto* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UTBAudioSubsystem>() : nullptr;
}

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
	if (auto* Audio = VoiceAudioSubsystem(GetWorld()))
	{
		Audio->UntrackVoice(TalkingAudio.Get());
	}
	TalkingAudio.Reset();
	if (EarsController.IsValid())
	{
		EarsController->ClearAudioListenerOverride();
	}
	Super::EndPlay(EndPlayReason);
}

void UTBVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateMirror();
}

void UTBVoiceComponent::OnTalkingBegin(UAudioComponent* AudioComponent)
{
	Super::OnTalkingBegin(AudioComponent);
	if (auto* Audio = VoiceAudioSubsystem(GetWorld()))
	{
		Audio->UntrackVoice(TalkingAudio.Get());
		Audio->TrackVoice(AudioComponent);
	}
	TalkingAudio = AudioComponent;
	ApplyToTalkingAudio();
}

void UTBVoiceComponent::OnTalkingEnd()
{
	if (auto* Audio = VoiceAudioSubsystem(GetWorld()))
	{
		Audio->UntrackVoice(TalkingAudio.Get());
	}
	Super::OnTalkingEnd();
	TalkingAudio.Reset();
}

void UTBVoiceComponent::UpdateVoice()
{
	auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}
	// 自分の声は自分のPCでは鳴らさない。オンラインIDがない場合も耳はキャラへ移す。
	if (Pawn->IsLocallyControlled())
	{
		// 同じPawnでもロビーから試合へ移るとメッシュの向きが変わるため、補正を定期更新する。
		if (auto* Controller = Cast<APlayerController>(Pawn->GetController()))
		{
			USceneComponent* Ears = VoiceAnchor(Pawn);
			// メッシュは横向きに付いているので、向きはキャラ本体に戻す
			Controller->SetAudioListenerOverride(Ears, FVector::ZeroVector, Ears->GetRelativeRotation().GetInverse());
			EarsController = Controller;
		}
		return;
	}
	auto* Speaker = Pawn->GetPlayerState<ATBPlayerState>();
	if (!Speaker || !Speaker->GetUniqueId().IsValid())
	{
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
		// 常時送信だと相手の声の再生が止まらず、キャラを作り直しても古いキャラの設定が残るので、再生を作り直させる
		if (IOnlineVoicePtr Voice = Online::GetVoiceInterface(GetWorld()))
		{
			const FUniqueNetIdPtr Id = Speaker->GetUniqueId().GetUniqueNetId();
			Voice->UnregisterRemoteTalker(*Id);
			Voice->RegisterRemoteTalker(*Id);
		}
	}
	// キャラの作り直しで、古いキャラの破棄に登録を消されることがあるので付け直す
	else if (UVOIPStatics::GetVOIPTalkerForPlayer(Speaker->GetUniqueId()) != this)
	{
		RegisterWithPlayerState(Speaker);
	}
	const auto* Controller = GetWorld()->GetFirstPlayerController();
	const auto* Listener = Controller ? Controller->GetPlayerState<ATBPlayerState>() : nullptr;
	const ETBTeam ListenerTeam = Listener ? Listener->Team : ETBTeam::None;
	const auto* Match = TB::GS(GetWorld());
	const auto& Voice = TB::Rules().Voice;
	const bool bEveryone = !Voice.bProximityInLobby && Match && Match->Phase == ETBPhase::Lobby;
	const float ForcedPitch = CVarForcePitch.GetValueOnGameThread();
	const float Semitones = ForcedPitch != 0.f ? ForcedPitch : PitchFor(Speaker->Team, ListenerTeam);
	if (Semitones != AppliedSemitones || Voice.PitchDelayLength != AppliedDelayLength)
	{
		AppliedSemitones = Semitones;
		AppliedDelayLength = Voice.PitchDelayLength;
		FTBPitchShiftSettings PitchSettings;
		PitchSettings.Semitones = Semitones;
		PitchSettings.DelayLength = Voice.PitchDelayLength;
		Pitch->SetSettings(PitchSettings);
	}
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
	Settings.ComponentToAttachTo = bIntoBox ? Mirror.Get() : WallBox ? WallBox->Visual.Get() : VoiceAnchor(Pawn);
	auto& Shape = Attenuation->Attenuation;
	Shape.bAttenuate = !bEveryone;
	Shape.bSpatialize = !bEveryone;
	Shape.FalloffDistance = WallBox ? Voice.BoxedDistance : DistanceFor(Speaker->Team, ListenerTeam);
	Shape.DistanceAlgorithm = Voice.FalloffModel;
	Shape.CustomAttenuationCurve = Voice.FalloffCurve;
	Shape.bAttenuateWithLPF = WallBox != nullptr;
	Shape.LPFRadiusMin = 0.f;
	Shape.LPFRadiusMax = Voice.BoxedDistance;
	Shape.LPFFrequencyAtMin = Voice.BoxedMuffleFrequency;
	Shape.LPFFrequencyAtMax = Voice.BoxedMuffleFrequency;
	ApplyToTalkingAudio();
}

void UTBVoiceComponent::ApplyToTalkingAudio()
{
	UAudioComponent* Audio = TalkingAudio.Get();
	USceneComponent* Target = Settings.ComponentToAttachTo;
	if (!Audio || !Attenuation || !Target)
	{
		return;
	}
	if (Audio->GetAttachParent() != Target)
	{
		Audio->AttachToComponent(Target, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	Audio->AdjustAttenuation(Attenuation->Attenuation);
}

void UTBVoiceComponent::UpdateMirror()
{
	const auto* Controller = GetWorld()->GetFirstPlayerController();
	const APawn* ListenerPawn = Controller ? Controller->GetPawn() : nullptr;
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Mirror || !Box.IsValid() || !ListenerPawn || !Pawn)
	{
		return;
	}
	// 箱から見た話し手の位置を、聞き手の耳の周りに写す。耳との距離が箱との距離と一致する。
	const FVector FromBox = VoiceAnchor(Pawn)->GetComponentLocation() - Box->Visual->GetComponentLocation();
	Mirror->SetWorldLocation(VoiceAnchor(ListenerPawn)->GetComponentLocation() + FromBox);
}

float UTBVoiceComponent::DistanceFor(ETBTeam SpeakerTeam, ETBTeam ListenerTeam) const
{
	const auto& Voice = TB::Rules().Voice;
	const bool bSpeakerToy = SpeakerTeam == ETBTeam::Toy;
	if (ListenerTeam == ETBTeam::Toy)
	{
		if (bSpeakerToy)
		{
			return Voice.ToyToToyDistance;
		}
		return SpeakerTeam == ETBTeam::Human ? Voice.HumanToToyDistance : Voice.HumanToHumanDistance;
	}
	return bSpeakerToy && ListenerTeam == ETBTeam::Human ? Voice.ToyToHumanDistance : Voice.HumanToHumanDistance;
}

float UTBVoiceComponent::PitchFor(ETBTeam SpeakerTeam, ETBTeam ListenerTeam) const
{
	const auto& Voice = TB::Rules().Voice;
	if (SpeakerTeam == ETBTeam::Human && ListenerTeam == ETBTeam::Toy)
	{
		return Voice.HumanPitchForToys;
	}
	return SpeakerTeam == ETBTeam::Toy && ListenerTeam == ETBTeam::Human ? Voice.ToyPitchForHumans : 0.f;
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
