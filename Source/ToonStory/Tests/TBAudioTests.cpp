#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Audio/TBAudioEvents.h"
#include "Audio/TBAudioSettings.h"
#include "Audio/TBAudioSubsystem.h"
#include "Audio/TBCharacterAudioComponent.h"
#include "Character/TBCharacter.h"
#include "GamePlay/TBDummyToy.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundWave.h"
#include "UObject/UObjectIterator.h"
#include "UObject/GarbageCollection.h"

namespace
{
	UWorld* FindAudioTestWorld()
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType == EWorldType::Game && World && World->GetNetMode() == NM_Standalone &&
			    World->GetAuthGameMode<ATBGameMode>())
			{
				return World;
			}
		}
		return nullptr;
	}

	// Keep these tests silent even after production assets have been registered.
	// Restore both editor config objects and the current session's preferences.
	class FScopedEmptyAudio
	{
	public:
		explicit FScopedEmptyAudio(UTBAudioSubsystem* InAudio)
		    : Audio(InAudio), Settings(GetMutableDefault<UTBAudioSettings>()), Music(Settings->Music),
		      Sounds(Settings->Sounds), Master(Audio->GetMasterVolume()), MusicVolume(Audio->GetMusicVolume()),
		      Effects(Audio->GetEffectsVolume()), Voice(Audio->GetVoiceVolume()),
		      HumanFootstepToyVolume(Settings->HumanFootstepToyVolumeMultiplier),
		      HumanFootstepToyPitch(Settings->HumanFootstepToyPitchMultiplier)
		{
			for (auto& Pair : Settings->Music)
			{
				Pair.Value.Sound.Reset();
			}
			for (auto& Pair : Settings->Sounds)
			{
				Pair.Value.Sound.Reset();
			}
			Audio->ReloadAudioSettings();
		}

		~FScopedEmptyAudio()
		{
			Settings->Music = Music;
			Settings->Sounds = Sounds;
			Settings->HumanFootstepToyVolumeMultiplier = HumanFootstepToyVolume;
			Settings->HumanFootstepToyPitchMultiplier = HumanFootstepToyPitch;
			Audio->SetMasterVolume(Master);
			Audio->SetMusicVolume(MusicVolume);
			Audio->SetEffectsVolume(Effects);
			Audio->SetVoiceVolume(Voice);
			Audio->ReloadAudioSettings();
		}

	private:
		UTBAudioSubsystem* Audio;
		UTBAudioSettings* Settings;
		TMap<ETBMusicState, FTBAudioClip> Music;
		TMap<ETBSoundEvent, FTBAudioClip> Sounds;
		float Master, MusicVolume, Effects, Voice;
		float HumanFootstepToyVolume, HumanFootstepToyPitch;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBAudioSelectedCatalogTest, "ToonStory.Audio.SelectedCatalog",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBAudioSelectedCatalogTest::RunTest(const FString& Parameters)
{
	const auto* Settings = GetDefault<UTBAudioSettings>();
	TestEqual(TEXT("Four music states registered"), Settings->Music.Num(), 4);
	TestEqual(TEXT("All selected SE registered"), Settings->Sounds.Num(), int32(ETBSoundEvent::ToyTaunt) + 1);
	for (const auto& Pair : Settings->Music)
	{
		auto* Wave = Cast<USoundWave>(Pair.Value.Sound.LoadSynchronous());
		if (TestNotNull(TEXT("Registered BGM loads"), Wave))
		{
			TestTrue(TEXT("BGM loops"), Wave->bLooping);
			TestTrue(TEXT("BGM has samples"), Wave->Duration > 0);
		}
		TestFalse(TEXT("BGM is non-spatial"), Pair.Value.bSpatial);
	}
	for (const auto& Pair : Settings->Sounds)
	{
		auto* Wave = Cast<USoundWave>(Pair.Value.Sound.LoadSynchronous());
		if (TestNotNull(TEXT("Registered SE loads"), Wave))
		{
			const bool bLoop = Pair.Key == ETBSoundEvent::CollectLoop || Pair.Key == ETBSoundEvent::StoreLoop ||
			                   Pair.Key == ETBSoundEvent::RescueLoop;
			TestEqual(TEXT("Only progress sounds loop"), bool(Wave->bLooping), bLoop);
			TestTrue(TEXT("SE has samples"), Wave->Duration > 0);
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBAudioEmptyAssetsTest, "ToonStory.Audio.EmptyAssetsAndVolumes",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBAudioEmptyAssetsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindAudioTestWorld();
	if (!TestNotNull(TEXT("Run in a standalone stage with -game"), World))
	{
		return false;
	}
	auto* Audio = World->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>();
	if (!TestNotNull(TEXT("Audio subsystem is created automatically"), Audio))
	{
		return false;
	}
	FScopedEmptyAudio Empty(Audio);
	auto* Controller = World->GetFirstPlayerController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	USceneComponent* Source = Pawn ? Pawn->GetRootComponent() : nullptr;
	if (!TestNotNull(TEXT("A real scene source is present"), Source))
	{
		return false;
	}
	// Both an empty entry and a missing entry must be harmless.
	Audio->PlaySoundEvent(ETBSoundEvent::UIClick);
	Audio->PlaySoundEvent(ETBSoundEvent::Capture, Source->GetComponentLocation(), Source);
	TestNull(TEXT("An empty loop does not create an audio component"), Audio->StartLoop(ETBSoundEvent::RescueLoop, Source));
	GetMutableDefault<UTBAudioSettings>()->Sounds.Remove(ETBSoundEvent::StoreLoop);
	Audio->ReloadAudioSettings();
	TestNull(TEXT("A missing loop entry is safe"), Audio->StartLoop(ETBSoundEvent::StoreLoop, Source));
	TestNull(TEXT("A missing attachment is safe"), Audio->StartLoop(ETBSoundEvent::RescueLoop, nullptr));
	Audio->StopLoop(nullptr);

	Audio->SetMasterVolume(.5f);
	Audio->SetMusicVolume(.5f);
	Audio->SetEffectsVolume(.25f);
	Audio->SetVoiceVolume(.75f);
	TestEqual(TEXT("Music combines master and music preference"), Audio->GetCategoryVolume(ETBAudioCategory::Music), .25f);
	TestEqual(TEXT("Effects have an independent preference"), Audio->GetCategoryVolume(ETBAudioCategory::Effects), .125f);
	TestEqual(TEXT("Voice has an independent preference"), Audio->GetCategoryVolume(ETBAudioCategory::Voice), .375f);
	Audio->SetMusicVolume(-1.f);
	Audio->SetEffectsVolume(2.f);
	Audio->SetVoiceVolume(-.1f);
	TestEqual(TEXT("Negative music volume clamps to zero"), Audio->GetMusicVolume(), 0.f);
	TestEqual(TEXT("Effects volume clamps to one"), Audio->GetEffectsVolume(), 1.f);
	TestEqual(TEXT("Negative voice volume clamps to zero"), Audio->GetVoiceVolume(), 0.f);
	Audio->SetMasterVolume(-1.f);
	TestEqual(TEXT("Master mute silences effects"), Audio->GetCategoryVolume(ETBAudioCategory::Effects), 0.f);
	TestEqual(TEXT("Master mute preserves effects preference"), Audio->GetEffectsVolume(), 1.f);
	Audio->SetMasterVolume(3.f);
	TestEqual(TEXT("Master volume clamps to one"), Audio->GetMasterVolume(), 1.f);
	TestEqual(TEXT("Unmuting restores effects preference"), Audio->GetCategoryVolume(ETBAudioCategory::Effects), 1.f);

	// A silent component checks live volume application without microphone/device/assets.
	auto* VoiceComponent = NewObject<UAudioComponent>(World);
	VoiceComponent->SetVolumeMultiplier(.8f);
	Audio->SetMasterVolume(.5f);
	Audio->SetVoiceVolume(.25f);
	Audio->TrackVoice(VoiceComponent);
	TestEqual(TEXT("New voice uses the current preference"), VoiceComponent->VolumeMultiplier, .1f);
	Audio->TrackVoice(VoiceComponent);
	TestEqual(TEXT("Repeated voice registration does not compound volume"), VoiceComponent->VolumeMultiplier, .1f);
	Audio->SetVoiceVolume(.5f);
	TestEqual(TEXT("Changing voice volume updates an existing component"), VoiceComponent->VolumeMultiplier, .2f);
	Audio->SetMasterVolume(0.f);
	TestEqual(TEXT("Master mute updates an existing component"), VoiceComponent->VolumeMultiplier, 0.f);
	Audio->UntrackVoice(VoiceComponent);
	TestEqual(TEXT("Removing the voice restores its original gain"), VoiceComponent->VolumeMultiplier, .8f);
	Audio->SetVoiceVolume(1.f);
	TestEqual(TEXT("Removed voice is no longer controlled"), VoiceComponent->VolumeMultiplier, .8f);
	VoiceComponent->DestroyComponent();
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBAudioPhaseTest, "ToonStory.Audio.PhaseLifecycle",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBAudioPhaseTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindAudioTestWorld();
	if (!TestNotNull(TEXT("Run in a standalone stage with -game"), World))
	{
		return false;
	}
	auto* Audio = World->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>();
	auto* State = World->GetGameState<ATBGameState>();
	if (!TestNotNull(TEXT("Audio subsystem"), Audio) || !TestNotNull(TEXT("Match state"), State))
	{
		return false;
	}
	FScopedEmptyAudio Empty(Audio);
	const ETBPhase OriginalPhase = State->Phase;
	const ETBWinner OriginalWinner = State->Winner;
	State->Winner = ETBWinner::None;
	const ETBPhase Phases[] = {ETBPhase::Lobby, ETBPhase::Playing, ETBPhase::Results, ETBPhase::Lobby,
	                          ETBPhase::Playing, ETBPhase::Aborted, ETBPhase::Lobby};
	const ETBMusicState Expected[] = {ETBMusicState::Lobby, ETBMusicState::Match, ETBMusicState::Results,
	                                ETBMusicState::Lobby, ETBMusicState::Match, ETBMusicState::Results,
	                                ETBMusicState::Lobby};
	Audio->SetMusicVolume(.25f);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Phases); ++Index)
	{
		State->Phase = Phases[Index];
		Audio->Tick(0.f);
		TestEqual(TEXT("Music follows phase even without assets"), Audio->GetMusicState(), Expected[Index]);
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Audio->Tick(1.f / 60.f);
		}
		TestEqual(TEXT("Unchanged phase remains stable"), Audio->GetMusicState(), Expected[Index]);
		TestEqual(TEXT("Round changes preserve user preference"), Audio->GetMusicVolume(), .25f);
	}
	State->Phase = OriginalPhase;
	State->Winner = OriginalWinner;
	Audio->Tick(0.f);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBAudioAudienceTest, "ToonStory.Audio.AudienceIsolation",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBAudioAudienceTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindAudioTestWorld();
	if (!TestNotNull(TEXT("Run in a standalone stage with -game"), World))
	{
		return false;
	}
	auto* Human = World->SpawnActor<ATBPlayerState>();
	auto* Toy = World->SpawnActor<ATBPlayerState>();
	auto* Unassigned = World->SpawnActor<ATBPlayerState>();
	if (!Human || !Toy || !Unassigned)
	{
		AddError(TEXT("Could not create audience fixtures"));
		for (auto* Player : {Human, Toy, Unassigned})
		{
			if (Player)
			{
				Player->Destroy();
			}
		}
		return false;
	}
	Human->Team = ETBTeam::Human;
	Toy->Team = ETBTeam::Toy;
	Unassigned->Team = ETBTeam::None;
	TestTrue(TEXT("Toys receive rescue notification"), UTBAudioEvents::ShouldDeliver(ETBSoundEvent::RescueComplete, Toy, nullptr));
	TestFalse(TEXT("Children cannot hear rescue notification"), UTBAudioEvents::ShouldDeliver(ETBSoundEvent::RescueComplete, Human, nullptr));
	TestFalse(TEXT("Unassigned players cannot hear rescue notification"), UTBAudioEvents::ShouldDeliver(ETBSoundEvent::RescueComplete, Unassigned, nullptr));
	for (const ETBSoundEvent Event : {ETBSoundEvent::Unready, ETBSoundEvent::CarryLift, ETBSoundEvent::CarryDrop, ETBSoundEvent::StoreCancel, ETBSoundEvent::BoxedTransition, ETBSoundEvent::Frozen, ETBSoundEvent::Thawed})
	{
		TestTrue(TEXT("Personal event reaches its subject"), UTBAudioEvents::ShouldDeliver(Event, Toy, Toy));
		TestFalse(TEXT("Personal event excludes other players"), UTBAudioEvents::ShouldDeliver(Event, Human, Toy));
		TestFalse(TEXT("Personal event requires a subject"), UTBAudioEvents::ShouldDeliver(Event, Toy, nullptr));
	}
	TestTrue(TEXT("Ready is heard across lobby"), UTBAudioEvents::ShouldDeliver(ETBSoundEvent::Ready, Human, Toy));
	for (auto Event : {ETBSoundEvent::Pickup, ETBSoundEvent::BatteryLast, ETBSoundEvent::RescueCancel})
	{
		TestTrue(TEXT("Toy team receives team event"), UTBAudioEvents::ShouldDeliver(Event, Toy, nullptr));
		TestFalse(TEXT("Human cannot hear team event"), UTBAudioEvents::ShouldDeliver(Event, Human, Toy));
	}
	TestFalse(TEXT("Free toy cannot hear TV"), UTBAudioEvents::ShouldDeliver(ETBSoundEvent::TVChannel, Toy, nullptr));
	Toy->ToyState = ETBToyState::Boxed;
	for (auto Event : {ETBSoundEvent::TVButton, ETBSoundEvent::TVChannel, ETBSoundEvent::TVPowerOn,
	                  ETBSoundEvent::TVPowerOff, ETBSoundEvent::TVGlitch, ETBSoundEvent::TVStatic})
	{
		TestTrue(TEXT("Boxed toy receives TV event"), UTBAudioEvents::ShouldDeliver(Event, Toy, nullptr));
		TestFalse(TEXT("Human cannot hear TV"), UTBAudioEvents::ShouldDeliver(Event, Human, Toy));
	}
	TestFalse(TEXT("Team notice excludes captured subject"), UTBAudioEvents::ShouldDeliver(ETBSoundEvent::TeamNotice, Toy, Toy));
	for (const ETBSoundEvent Event : {ETBSoundEvent::Capture, ETBSoundEvent::Store})
	{
		TestTrue(TEXT("World event reaches Children"), UTBAudioEvents::ShouldDeliver(Event, Human, Toy));
		TestTrue(TEXT("World event reaches Toys"), UTBAudioEvents::ShouldDeliver(Event, Toy, Toy));
		TestFalse(TEXT("Missing recipient is rejected"), UTBAudioEvents::ShouldDeliver(Event, nullptr, Toy));
	}
	for (const ETBSoundEvent Event : {ETBSoundEvent::UIClick, ETBSoundEvent::Footstep, ETBSoundEvent::HumanFootstep, ETBSoundEvent::RescueLoop,
	                                ETBSoundEvent::StoreLoop, ETBSoundEvent::MatchStart, ETBSoundEvent::Victory})
	{
		TestFalse(TEXT("Locally observed events cannot also broadcast through the server route"),
		          UTBAudioEvents::ShouldDeliver(Event, Toy, Toy));
	}
	for (auto* Player : {Human, Toy, Unassigned})
	{
		Player->Destroy();
	}
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBAudioRegisteredClipTest, "ToonStory.Audio.RegisteredClipLifecycle",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBAudioRegisteredClipTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindAudioTestWorld();
	if (!TestNotNull(TEXT("Standalone stage"), World)) return false;
	auto* Audio = World->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>();
	auto* State = World->GetGameState<ATBGameState>();
	APawn* Pawn = World->GetFirstPlayerController()->GetPawn();
	if (!Audio || !State || !Pawn) return false;
	FScopedEmptyAudio Empty(Audio);
	// Synthetic silent source: verify registration/playback ownership without adding production assets.
	auto* Wave = NewObject<USoundWaveProcedural>();
	Wave->NumChannels = 1;
	Wave->SetSampleRate(22050);
	TArray<uint8> Silence;
	Silence.SetNumZeroed(4410);
	Wave->QueueAudio(Silence.GetData(), Silence.Num());
	auto* Settings = GetMutableDefault<UTBAudioSettings>();
	FTBAudioClip& LoopClip = Settings->Sounds.FindOrAdd(ETBSoundEvent::RescueLoop);
	LoopClip.Sound = Wave;
	LoopClip.Volume = .8f;
	LoopClip.bSpatial = true;
	Settings->Music.FindOrAdd(ETBMusicState::Lobby).Sound = Wave;
	Audio->ReloadAudioSettings();
	Audio->SetMasterVolume(.5f);
	Audio->SetEffectsVolume(.5f);
	auto* Loop = Audio->StartLoop(ETBSoundEvent::RescueLoop, Pawn->GetRootComponent());
	if (!TestNotNull(TEXT("Registering a sound creates a loop component"), Loop)) return false;
	TestTrue(TEXT("The configured sound is used"), Loop->GetSound() == Wave);
	TestTrue(TEXT("Spatial loop attaches to its source"), Loop->GetAttachParent() == Pawn->GetRootComponent());
	TestEqual(TEXT("Clip gain combines with master and effect gains"), Loop->VolumeMultiplier, .2f);
	Audio->SetEffectsVolume(1.f);
	TestEqual(TEXT("Already-created effects respond to volume changes"), Loop->VolumeMultiplier, .4f);
	TWeakObjectPtr<UAudioComponent> WeakLoop = Loop;
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("A registered loop survives garbage collection"), WeakLoop.IsValid());
	Audio->StopLoop(WeakLoop.Get());
	TestFalse(TEXT("Stopping a loop unregisters it"), WeakLoop.IsValid() && WeakLoop->IsRegistered());

	const ETBPhase OriginalPhase = State->Phase;
	State->Phase = ETBPhase::Lobby;
	Audio->Tick(0.f);
	auto FindMusic = [&]() -> UAudioComponent*
	{
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsRegistered() && It->GetSound() == Wave && !It->GetAttachParent()) return *It;
		}
		return nullptr;
	};
	TWeakObjectPtr<UAudioComponent> Music = FindMusic();
	TestTrue(TEXT("Registering a lobby track creates BGM"), Music.IsValid());
	Audio->Tick(0.f);
	TestTrue(TEXT("Same phase reuses the BGM component"), FindMusic() == Music.Get());
	Audio->ReloadAudioSettings();
	TestFalse(TEXT("Reload stops old BGM"), Music.IsValid() && Music->IsRegistered());
	State->Phase = OriginalPhase;
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBHumanFootstepTest, "ToonStory.Audio.HumanFootstepPerspective",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBHumanFootstepTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindAudioTestWorld();
	if (!TestNotNull(TEXT("Standalone stage"), World)) return false;
	auto* Audio = World->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>();
	auto* Controller = World->GetGameInstance()->GetFirstLocalPlayerController(World);
	auto* Listener = Controller ? Controller->GetPlayerState<ATBPlayerState>() : nullptr;
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!TestNotNull(TEXT("Audio subsystem"), Audio) || !TestNotNull(TEXT("Local listener"), Listener) ||
	    !TestNotNull(TEXT("Footstep source"), Pawn)) return false;
	FScopedEmptyAudio Empty(Audio);
	TGuardValue<ETBTeam> RestoreTeam(Listener->Team, ETBTeam::Human);
	// Keep a silent source alive while checking the common playback path. Gameplay
	// emits single steps; StartLoop here only prevents immediate teardown with -nosound.
	auto* Wave = NewObject<USoundWaveProcedural>();
	Wave->NumChannels = 1;
	Wave->SetSampleRate(22050);
	TArray<uint8> Silence;
	Silence.SetNumZeroed(4410);
	Wave->QueueAudio(Silence.GetData(), Silence.Num());
	auto* Settings = GetMutableDefault<UTBAudioSettings>();
	FTBAudioClip HumanClip;
	HumanClip.Sound = Wave;
	HumanClip.Volume = .8f;
	HumanClip.Pitch = 1.2f;
	HumanClip.bSpatial = true;
	HumanClip.MaxDistance = 2400.f;
	HumanClip.MaxInstances = 16;
	Settings->Sounds.Add(ETBSoundEvent::HumanFootstep, HumanClip);
	Settings->Sounds.Add(ETBSoundEvent::Footstep, HumanClip);
	Settings->HumanFootstepToyVolumeMultiplier = 1.75f;
	Settings->HumanFootstepToyPitchMultiplier = .75f;
	Audio->ReloadAudioSettings();
	Audio->SetMasterVolume(1.f);
	Audio->SetEffectsVolume(1.f);
	USceneComponent* Source = Pawn->GetRootComponent();
	auto* Human = Audio->StartLoop(ETBSoundEvent::HumanFootstep, Source);
	Listener->Team = ETBTeam::Toy;
	auto* Toy = Audio->StartLoop(ETBSoundEvent::HumanFootstep, Source);
	auto* ToyFootstep = Audio->StartLoop(ETBSoundEvent::Footstep, Source);
	if (!TestNotNull(TEXT("Child listener gets the human footstep"), Human) ||
	    !TestNotNull(TEXT("Toy listener gets the human footstep"), Toy) ||
	    !TestNotNull(TEXT("Toy footsteps remain available"), ToyFootstep)) return false;
	TestTrue(TEXT("Both listener teams use the registered human sound"), Human->GetSound() == Wave && Toy->GetSound() == Wave);
	TestEqual(TEXT("Children hear the configured gain"), Human->VolumeMultiplier, .8f);
	TestEqual(TEXT("Children hear the configured pitch"), Human->PitchMultiplier, 1.2f);
	TestTrue(TEXT("Toys hear human footsteps 1.75 times louder"), FMath::IsNearlyEqual(Toy->VolumeMultiplier, 1.4f));
	TestTrue(TEXT("Toys hear human footsteps at 0.75 times pitch"), FMath::IsNearlyEqual(Toy->PitchMultiplier, .9f, 1.e-6f));
	TestEqual(TEXT("Toy footstep gain is unaffected"), ToyFootstep->VolumeMultiplier, .8f);
	TestEqual(TEXT("Toy footstep pitch is unaffected"), ToyFootstep->PitchMultiplier, 1.2f);
	TestTrue(TEXT("Human footsteps are positioned at their source"), Toy->bAllowSpatialization && Toy->GetAttachParent() == Source);

	const FSoundAttenuationSettings* Attenuation = Toy->GetAttenuationSettingsToApply();
	if (TestNotNull(TEXT("Human footstep has distance attenuation without an asset"), Attenuation))
	{
		TestTrue(TEXT("Distance attenuation is active"), Attenuation->bAttenuate && Attenuation->bSpatialize);
		TestEqual(TEXT("Default human attenuation is linear"), Attenuation->DistanceAlgorithm, EAttenuationDistanceModel::Linear);
		TestEqual(TEXT("Human footstep range follows its setting"), Attenuation->FalloffDistance, 2400.f);
		const FTransform Origin = Source->GetComponentTransform();
		auto GainAt = [&](float Distance)
		{
			return Attenuation->Evaluate(Origin, Origin.GetLocation() + FVector(Distance, 0.f, 0.f));
		};
		const float AtSource = GainAt(0.f);
		const float Near = GainAt(600.f);
		const float Far = GainAt(1800.f);
		TestTrue(TEXT("Approaching a human continuously raises footstep volume"), AtSource > Near && Near > Far && Far > 0.f);
		TestTrue(TEXT("Human footsteps are silent outside the configured range"), FMath::IsNearlyZero(GainAt(2401.f)));
	}
	Audio->SetMasterVolume(.5f);
	Audio->SetEffectsVolume(.5f);
	TestEqual(TEXT("Category controls scale the original human gain"), Human->VolumeMultiplier, .2f);
	TestTrue(TEXT("Category controls preserve the toy listener gain multiplier"), FMath::IsNearlyEqual(Toy->VolumeMultiplier, .35f));
	TestTrue(TEXT("Volume controls do not change pitch"), FMath::IsNearlyEqual(Toy->PitchMultiplier, .9f, 1.e-6f));
	Audio->SetEffectsVolume(0.f);
	TestEqual(TEXT("Effects mute silences amplified footsteps"), Toy->VolumeMultiplier, 0.f);
	Audio->SetEffectsVolume(1.f);
	TestTrue(TEXT("Unmuting restores amplified footsteps"), FMath::IsNearlyEqual(Toy->VolumeMultiplier, .7f));

	Listener->Team = ETBTeam::None;
	auto* Unassigned = Audio->StartLoop(ETBSoundEvent::HumanFootstep, Source);
	if (TestNotNull(TEXT("Unassigned listener uses the base human footstep"), Unassigned))
	{
		TestEqual(TEXT("Unassigned listener gets no toy gain multiplier"), Unassigned->VolumeMultiplier, .4f);
		TestEqual(TEXT("Unassigned listener gets no toy pitch multiplier"), Unassigned->PitchMultiplier, 1.2f);
	}

	// Explicitly authored distance settings still take precedence over the default.
	auto* CustomAttenuation = NewObject<USoundAttenuation>();
	CustomAttenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
	CustomAttenuation->Attenuation.FalloffDistance = 700.f;
	Settings->Sounds.FindChecked(ETBSoundEvent::HumanFootstep).Attenuation = CustomAttenuation;
	Audio->ReloadAudioSettings();
	auto* Custom = Audio->StartLoop(ETBSoundEvent::HumanFootstep, Source);
	if (TestNotNull(TEXT("Custom human attenuation can be registered"), Custom))
	{
		TestTrue(TEXT("Custom attenuation asset is preserved"), Custom->AttenuationSettings == CustomAttenuation);
		const auto* Applied = Custom->GetAttenuationSettingsToApply();
		if (TestNotNull(TEXT("Custom attenuation is applied"), Applied))
		{
			TestEqual(TEXT("Custom falloff is not replaced"), Applied->FalloffDistance, 700.f);
			TestEqual(TEXT("Custom distance model is not replaced"), Applied->DistanceAlgorithm, EAttenuationDistanceModel::Logarithmic);
		}
	}

	Settings->Sounds.FindChecked(ETBSoundEvent::HumanFootstep).Sound.Reset();
	Audio->ReloadAudioSettings();
	TestNull(TEXT("Empty human sound does not fall back to the toy footstep"), Audio->StartLoop(ETBSoundEvent::HumanFootstep, Source));
	Settings->Sounds.Remove(ETBSoundEvent::HumanFootstep);
	Audio->ReloadAudioSettings();
	TestNull(TEXT("Missing human sound slot is also safe"), Audio->StartLoop(ETBSoundEvent::HumanFootstep, Source));
	return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBDummyStoreAudioTest, "ToonStory.Audio.DummyStoreParity",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBDummyStoreAudioTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindAudioTestWorld();
	if (!TestNotNull(TEXT("Standalone stage"), World)) return false;
	auto* Human = Cast<ATBCharacter>(World->GetFirstPlayerController()->GetPawn());
	auto* State = World->GetGameState<ATBGameState>();
	auto* Audio = World->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>();
	if (!Human || !Human->TBPS() || !State || !Audio) return false;
	auto* Component = Human->FindComponentByClass<UTBCharacterAudioComponent>();
	if (!TestNotNull(TEXT("Character audio component"), Component)) return false;
	FScopedEmptyAudio Empty(Audio);
	const auto OldPhase = State->Phase;
	const auto OldTeam = Human->TBPS()->Team;
	State->Phase = ETBPhase::Playing;
	Human->TBPS()->Team = ETBTeam::Human;
	auto* Dummy = World->SpawnActor<ATBDummyToy>();
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Toy = World->SpawnActor<ATBCharacter>(FVector(0, 0, 1000), FRotator::ZeroRotator, Spawn);
	auto* Wave = NewObject<USoundWaveProcedural>();
	Wave->NumChannels = 1;
	Wave->SetSampleRate(22050);
	GetMutableDefault<UTBAudioSettings>()->Sounds.FindOrAdd(ETBSoundEvent::StoreLoop).Sound = Wave;
	Audio->ReloadAudioSettings();
	auto FindLoop = [&]() -> UAudioComponent*
	{
		for (TObjectIterator<UAudioComponent> It; It; ++It)
			if (It->GetWorld() == World && It->IsRegistered() && It->GetSound() == Wave) return *It;
		return nullptr;
	};
	for (bool bDummy : {false, true})
	{
		Human->CarriedToy = bDummy ? nullptr : Toy;
		Human->CarriedDummy = bDummy ? Dummy : nullptr;
		Human->StoreStarted = World->GetTimeSeconds();
		Component->TickComponent(0.f, LEVELTICK_All, nullptr);
		TestNotNull(bDummy ? TEXT("Dummy has the same storage loop") : TEXT("Real toy has storage loop"), FindLoop());
		Human->ClearHeldInteraction();
		Component->TickComponent(0.f, LEVELTICK_All, nullptr);
		TestNull(TEXT("Releasing interaction stops storage loop"), FindLoop());
	}
	Human->CarriedToy = nullptr;
	Human->CarriedDummy = nullptr;
	if (Toy) Toy->Destroy();
	if (Dummy) Dummy->Destroy();
	Human->TBPS()->Team = OldTeam;
	State->Phase = OldPhase;
	return !HasAnyErrors();
}
#endif
