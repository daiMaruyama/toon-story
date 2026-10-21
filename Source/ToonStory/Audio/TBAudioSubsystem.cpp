#include "Audio/TBAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Core/TBGameState.h"
#include "Core/TBGameUserSettings.h"
#include "Core/TBPlayerState.h"
#include "Core/TBTitleGameMode.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"

namespace
{
	float SafeVolume(float Value) { return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : 0.f; }
}

void UTBAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bInitialized = true;
	CleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::OnWorldCleanup);
	ReloadAudioSettings();
	// プレイヤーが設定画面で選んだ音量（保存済み）で鳴らす。
	if (const auto* Settings = UTBGameUserSettings::Get())
	{
		Settings->ApplyVolumes(*this);
	}
}

void UTBAudioSubsystem::Deinitialize()
{
	bInitialized = false;
	FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
	StopOwnedAudio();
	ReleaseVoiceTracking();
	LoadedAssets.Reset();
	Super::Deinitialize();
}

void UTBAudioSubsystem::ReloadAudioSettings()
{
	StopOwnedAudio();
	MusicState = ETBMusicState::None;
	const auto* Settings = GetDefault<UTBAudioSettings>();
	MusicClips = Settings->Music;
	SoundClips = Settings->Sounds;
	HumanFootstepToyVolumeMultiplier = FMath::IsFinite(Settings->HumanFootstepToyVolumeMultiplier)
	    ? FMath::Clamp(Settings->HumanFootstepToyVolumeMultiplier, 1.f, 4.f) : 1.75f;
	HumanFootstepToyPitchMultiplier = FMath::IsFinite(Settings->HumanFootstepToyPitchMultiplier)
	    ? FMath::Clamp(Settings->HumanFootstepToyPitchMultiplier, .25f, 1.f) : .75f;
	MusicFadeSeconds = FMath::IsFinite(Settings->MusicFadeSeconds) ? FMath::Clamp(Settings->MusicFadeSeconds, 0.f, 5.f) : .5f;
	LoadedAssets.Reset();
	// A small fixed catalog is loaded up front, so gameplay events never block to load an asset.
	if (!IsRunningDedicatedServer())
	{
		auto LoadClip = [this](const FTBAudioClip& Clip)
		{
			if (auto* Asset = Clip.Sound.LoadSynchronous()) LoadedAssets.AddUnique(Asset);
			if (auto* Asset = Clip.Attenuation.LoadSynchronous()) LoadedAssets.AddUnique(Asset);
			if (auto* Asset = Clip.Concurrency.LoadSynchronous()) LoadedAssets.AddUnique(Asset);
		};
		for (const auto& Pair : MusicClips) LoadClip(Pair.Value);
		for (const auto& Pair : SoundClips) LoadClip(Pair.Value);
	}
	++SettingsRevision;
}

void UTBAudioSubsystem::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (PlaybackWorld.Get() != World) return;
	StopOwnedAudio();
	ReleaseVoiceTracking(); // Voice components are owned/stopped by their talkers.
	PlaybackWorld.Reset();
	ToyActionMasks.Reset();
	MusicState = ETBMusicState::None;
	bSawPlaying = bResultCuePlayed = false;
}

void UTBAudioSubsystem::StopOwnedAudio()
{
	for (const auto& Playing : PlayingSounds)
	{
		if (Playing.Category == ETBAudioCategory::Voice) continue;
		if (auto* Audio = Playing.Component.Get())
		{
			Audio->Stop();
			Audio->DestroyComponent();
		}
	}
	PlayingSounds.RemoveAll([](const FPlayingSound& Playing) { return Playing.Category != ETBAudioCategory::Voice; });
	OwnedComponents.Reset();
	MusicComponent.Reset();
}

void UTBAudioSubsystem::ReleaseVoiceTracking()
{
	for (const auto& Playing : PlayingSounds)
	{
		if (Playing.Category == ETBAudioCategory::Voice && Playing.Component.IsValid())
			Playing.Component->SetVolumeMultiplier(Playing.BaseVolume);
	}
	PlayingSounds.Reset();
}

void UTBAudioSubsystem::EnsurePlaybackWorld(UWorld* World)
{
	if (PlaybackWorld.Get() != World)
	{
		StopOwnedAudio();
		ReleaseVoiceTracking();
		PlaybackWorld = World;
		ToyActionMasks.Reset();
		MusicState = ETBMusicState::None;
		bSawPlaying = bResultCuePlayed = false;
		LastLobbyCount = LastRemainingSecond = -1;
		++SettingsRevision;
	}
}

void UTBAudioSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer) return;
	EnsurePlaybackWorld(World);
	const APlayerController* Local = GetGameInstance()->GetFirstLocalPlayerController(World);
	if (!Local) return;
	if (TravelSound.IsSet() && TravelFromWorld.Get() != World)
	{
		const ETBSoundEvent Event = TravelSound.GetValue();
		TravelSound.Reset();
		PlaySoundEvent(Event);
	}
	ETBMusicState Next = ETBMusicState::None;
	if (const auto* State = World->GetGameState<ATBGameState>())
	{
		switch (State->Phase)
		{
		case ETBPhase::Lobby: Next = ETBMusicState::Lobby; break;
		case ETBPhase::Playing: Next = ETBMusicState::Match; break;
		case ETBPhase::Results:
		case ETBPhase::Aborted: Next = ETBMusicState::Results; break;
		}
		if (Next == ETBMusicState::Match)
		{
			if (MusicState == ETBMusicState::Lobby) PlaySoundEvent(ETBSoundEvent::MatchStart);
			bSawPlaying = true;
			const int32 Remaining = FMath::CeilToInt(State->Remaining());
			if (LastRemainingSecond > Remaining && Remaining > 0 && Remaining <= 10)
				PlaySoundEvent(ETBSoundEvent::TimeTick);
			LastRemainingSecond = Remaining;
		}
		else LastRemainingSecond = -1;
		if (Next == ETBMusicState::Lobby)
		{
			const int32 Count = State->PlayerArray.Num();
			if (Count > 0 && Count != LastLobbyCount)
			{
				if (LastLobbyCount >= 0 || MusicState == ETBMusicState::None)
					PlaySoundEvent(LastLobbyCount > Count ? ETBSoundEvent::LobbyLeave : ETBSoundEvent::LobbyJoin);
				LastLobbyCount = Count;
			}
		}
		else LastLobbyCount = -1;
		if (Next != ETBMusicState::Results) bResultCuePlayed = false;
		// Wait for winner/team replication. Initial snapshots and rematch resets don't play old cues.
		const auto* Player = Local->GetPlayerState<ATBPlayerState>();
		if (Next == ETBMusicState::Results && bSawPlaying && !bResultCuePlayed && Player &&
		    Player->Team != ETBTeam::None && State->Winner != ETBWinner::None)
		{
			const bool Won = (Player->Team == ETBTeam::Human && State->Winner == ETBWinner::Humans) ||
			                 (Player->Team == ETBTeam::Toy && State->Winner == ETBWinner::Toys);
			PlaySoundEvent(Won ? ETBSoundEvent::Victory : ETBSoundEvent::Defeat);
			bResultCuePlayed = true;
		}
		if (Next == ETBMusicState::Lobby) bSawPlaying = false;
	}
	else if (Local->IsA<ATBTitleController>()) Next = ETBMusicState::Title;
	ChangeMusic(Next);
	PlayingSounds.RemoveAll([](const FPlayingSound& Playing) { return !Playing.Component.IsValid(); });
	OwnedComponents.RemoveAll([](const TObjectPtr<UAudioComponent>& Audio) { return !IsValid(Audio); });
	for (const auto& Playing : PlayingSounds)
	{
		// Plain SoundWaves also repeat. Author looping in the asset for gapless playback.
		if (Playing.bLoop && Playing.Component.IsValid() && !Playing.Component->IsPlaying())
		{
			Playing.Component->Play();
		}
	}
}

UAudioComponent* UTBAudioSubsystem::CreateAudio(const FTBAudioClip& Clip, ETBAudioCategory Category,
                                               ETBSoundEvent Event, FVector Location, USceneComponent* AttachTo, bool bLoop)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_DedicatedServer || !Clip.Sound.Get() ||
	    Location.ContainsNaN() || (AttachTo && AttachTo->GetWorld() != World)) return nullptr;
	EnsurePlaybackWorld(World);
	if (Category == ETBAudioCategory::Effects)
	{
		int32 Count = 0;
		for (const auto& Playing : PlayingSounds)
		{
			if (Playing.Category == Category && Playing.Event == Event && Playing.Component.IsValid() &&
			    Playing.Component->IsPlaying()) ++Count;
		}
		if (Count >= FMath::Clamp(Clip.MaxInstances, 1, 64)) return nullptr;
	}
	UObject* Outer = AttachTo && AttachTo->GetOwner() ? static_cast<UObject*>(AttachTo->GetOwner()) : static_cast<UObject*>(World);
	auto* Audio = NewObject<UAudioComponent>(Outer);
	Audio->bAutoActivate = false;
	Audio->bAutoDestroy = !bLoop;
	Audio->bShouldRemainActiveIfDropped = bLoop;
	Audio->bStopWhenOwnerDestroyed = true;
	Audio->bAllowSpatialization = Category == ETBAudioCategory::Effects && Clip.bSpatial;
	Audio->bIsUISound = !Audio->bAllowSpatialization;
	Audio->SetSound(Clip.Sound.Get());
	float Pitch = FMath::IsFinite(Clip.Pitch) ? FMath::Clamp(Clip.Pitch, .25f, 4.f) : 1.f;
	float BaseVolume = FMath::IsFinite(Clip.Volume) ? FMath::Clamp(Clip.Volume, 0.f, 4.f) : 1.f;
	if (Category == ETBAudioCategory::Effects && Event == ETBSoundEvent::HumanFootstep)
	{
		// Resolve on the hearing machine, not on the human who produced the step.
		const auto* Local = GetGameInstance()->GetFirstLocalPlayerController(World);
		const auto* Listener = Local ? Local->GetPlayerState<ATBPlayerState>() : nullptr;
		if (Listener && Listener->Team == ETBTeam::Toy)
		{
			BaseVolume *= HumanFootstepToyVolumeMultiplier;
			Pitch *= HumanFootstepToyPitchMultiplier;
		}
	}
	Audio->SetPitchMultiplier(Pitch);
	Audio->SetVolumeMultiplier(BaseVolume * GetCategoryVolume(Category) *
	    (Category == ETBAudioCategory::Effects && Clip.bToyActionSound ? GetToyActionMask() : 1.f));
	if (auto* Concurrency = Clip.Concurrency.Get()) Audio->ConcurrencySet.Add(Concurrency);
	if (Audio->bAllowSpatialization)
	{
		if (auto* Attenuation = Clip.Attenuation.Get()) Audio->AttenuationSettings = Attenuation;
		else
		{
			FSoundAttenuationSettings DefaultAttenuation;
			DefaultAttenuation.bAttenuate = true;
			DefaultAttenuation.bSpatialize = true;
			DefaultAttenuation.AttenuationShapeExtents = FVector::ZeroVector;
			if (Event == ETBSoundEvent::HumanFootstep) DefaultAttenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
			DefaultAttenuation.FalloffDistance = FMath::IsFinite(Clip.MaxDistance) ? FMath::Max(1.f, Clip.MaxDistance) : 1800.f;
			Audio->AdjustAttenuation(DefaultAttenuation);
		}
	}
	else
	{
		FSoundAttenuationSettings NoAttenuation;
		NoAttenuation.bAttenuate = false;
		NoAttenuation.bSpatialize = false;
		Audio->AdjustAttenuation(NoAttenuation);
	}
	if (AttachTo) Audio->SetupAttachment(AttachTo);
	else Audio->SetWorldLocation(Location);
	Audio->RegisterComponentWithWorld(World);
	OwnedComponents.Add(Audio);
	PlayingSounds.Add({Audio, Category, Event, BaseVolume, bLoop, Category == ETBAudioCategory::Effects && Clip.bToyActionSound});
	return Audio;
}

void UTBAudioSubsystem::PlaySoundEvent(ETBSoundEvent Event, FVector Location, USceneComponent* AttachTo)
{
	if (Event == ETBSoundEvent::UIError && GetWorld())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now >= LastErrorTime && Now - LastErrorTime < .5) return;
		LastErrorTime = Now;
	}
	if (const auto* Clip = SoundClips.Find(Event))
	{
		if (auto* Audio = CreateAudio(*Clip, ETBAudioCategory::Effects, Event, Location, AttachTo, false)) Audio->Play();
	}
}

UAudioComponent* UTBAudioSubsystem::StartLoop(ETBSoundEvent Event, USceneComponent* AttachTo)
{
	if (!AttachTo) return nullptr;
	const auto* Clip = SoundClips.Find(Event);
	auto* Audio = Clip ? CreateAudio(*Clip, ETBAudioCategory::Effects, Event, AttachTo->GetComponentLocation(), AttachTo, true) : nullptr;
	if (Audio) Audio->Play();
	return Audio;
}

void UTBAudioSubsystem::StopLoop(UAudioComponent* Audio)
{
	if (!IsValid(Audio)) return;
	const int32 Index = PlayingSounds.IndexOfByPredicate([Audio](const FPlayingSound& Playing)
	{
		return Playing.Component.Get() == Audio && Playing.Category != ETBAudioCategory::Voice;
	});
	if (Index == INDEX_NONE) return;
	PlayingSounds.RemoveAtSwap(Index);
	OwnedComponents.Remove(Audio);
	Audio->Stop();
	Audio->DestroyComponent();
}

void UTBAudioSubsystem::ChangeMusic(ETBMusicState Next)
{
	if (MusicState == Next) return;
	if (auto* Audio = MusicComponent.Get())
	{
		for (auto& Playing : PlayingSounds)
		{
			if (Playing.Component.Get() == Audio) Playing.bLoop = false;
		}
		Audio->bAutoDestroy = true;
		Audio->FadeOut(MusicFadeSeconds, 0.f);
		if (!Audio->IsPlaying()) Audio->DestroyComponent();
	}
	MusicComponent.Reset();
	MusicState = Next;
	if (const auto* Clip = MusicClips.Find(Next))
	{
		auto* Audio = CreateAudio(*Clip, ETBAudioCategory::Music, ETBSoundEvent::UIClick, FVector::ZeroVector, nullptr, true);
		MusicComponent = Audio;
		if (Audio) Audio->FadeIn(MusicFadeSeconds, 1.f);
	}
}

float UTBAudioSubsystem::GetCategoryVolume(ETBAudioCategory Category) const
{
	const float CategoryVolume = Category == ETBAudioCategory::Music ? MusicVolume :
	                             Category == ETBAudioCategory::Voice ? VoiceVolume : EffectsVolume;
	return MasterVolume * CategoryVolume;
}

void UTBAudioSubsystem::ApplyVolumes()
{
	for (const auto& Playing : PlayingSounds)
	{
		if (auto* Audio = Playing.Component.Get()) Audio->SetVolumeMultiplier(Playing.BaseVolume * GetCategoryVolume(Playing.Category) *
		    (Playing.bToyActionSound ? GetToyActionMask() : 1.f));
	}
}

void UTBAudioSubsystem::SetToyActionMask(AActor* Source, float Multiplier)
{
	if (!IsValid(Source) || Source->GetWorld() != GetWorld()) return;
	EnsurePlaybackWorld(GetWorld());
	const float Value = SafeVolume(Multiplier);
	const float* Previous = ToyActionMasks.Find(Source);
	if (Previous && FMath::IsNearlyEqual(*Previous, Value)) return;
	ToyActionMasks.Add(Source, Value);
	ApplyVolumes();
}

void UTBAudioSubsystem::RemoveToyActionMask(AActor* Source)
{
	ToyActionMasks.Remove(Source);
	ApplyVolumes();
}

float UTBAudioSubsystem::GetToyActionMask() const
{
	float Result = 1.f;
	for (const auto& Pair : ToyActionMasks)
	{
		if (Pair.Key.IsValid()) Result = FMath::Min(Result, Pair.Value);
	}
	return Result;
}

void UTBAudioSubsystem::SetMasterVolume(float Value) { MasterVolume = SafeVolume(Value); ApplyVolumes(); }
void UTBAudioSubsystem::SetMusicVolume(float Value) { MusicVolume = SafeVolume(Value); ApplyVolumes(); }
void UTBAudioSubsystem::SetEffectsVolume(float Value) { EffectsVolume = SafeVolume(Value); ApplyVolumes(); }
void UTBAudioSubsystem::SetVoiceVolume(float Value) { VoiceVolume = SafeVolume(Value); ApplyVolumes(); }

void UTBAudioSubsystem::TrackVoice(UAudioComponent* Audio)
{
	if (!IsValid(Audio)) return;
	EnsurePlaybackWorld(GetWorld());
	for (const auto& Playing : PlayingSounds) if (Playing.Component.Get() == Audio) return;
	PlayingSounds.Add({Audio, ETBAudioCategory::Voice, ETBSoundEvent::UIClick, Audio->VolumeMultiplier, false});
	ApplyVolumes();
}

void UTBAudioSubsystem::UntrackVoice(UAudioComponent* Audio)
{
	for (int32 Index = PlayingSounds.Num() - 1; Index >= 0; --Index)
	{
		const auto& Playing = PlayingSounds[Index];
		if (Playing.Category != ETBAudioCategory::Voice || Playing.Component.Get() != Audio) continue;
		if (IsValid(Audio)) Audio->SetVolumeMultiplier(Playing.BaseVolume);
		PlayingSounds.RemoveAtSwap(Index);
	}
}

void UTBAudioSubsystem::QueueTravelSound(ETBSoundEvent Event)
{
    TravelSound = Event;
    TravelFromWorld = GetWorld();
}
