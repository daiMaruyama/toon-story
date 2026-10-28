#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/TBTestMatch.h"
#include "GamePlay/Items/TBNoiseRadio.h"
#include "Audio/TBAudioSubsystem.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundWaveProcedural.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNoiseRadioTest, "ToonStory.Items.NoiseRadio",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBNoiseRadioTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Entry : GEngine->GetWorldContexts())
	{
		if (Entry.WorldType == EWorldType::Game && Entry.World() && Entry.World()->GetGameState<ATBGameState>())
		{ World = Entry.World(); break; }
	}
	if (!TestNotNull(TEXT("Standalone game world"), World) || World->GetNetMode() != NM_Standalone) return false;
	auto* PC = World->GetFirstPlayerController();
	auto* Character = PC ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Player state"), Character->TBPS())) return false;
	auto* Player = Character->TBPS();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Audio = World->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>();
	if (!TestNotNull(TEXT("Audio"), Audio)) return false;
	const ETBPhase OldPhase = State->Phase;
	const ETBTeam OldTeam = Player->Team;
	const ETBToyState OldToyState = Player->ToyState;
	const bool bOldFrozen = Player->bFrozen;
	const float OldEffects = Audio->GetEffectsVolume();
	const auto OldSounds = GetMutableDefault<UTBAudioSettings>()->Sounds;
	for (ETBSoundEvent Feedback : {ETBSoundEvent::CollectLoop, ETBSoundEvent::RescueLoop})
	{
		const auto* Clip = OldSounds.Find(Feedback);
		TestTrue(TEXT("Production catalog excludes progress feedback from masking"), Clip && !Clip->bToyActionSound);
	}
	TBTest::FPlayingScope Playing(*State);
	Player->Team = ETBTeam::Toy;
	Player->ToyState = ETBToyState::Free;
	Player->bFrozen = false;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Radio = World->SpawnActor<ATBNoiseRadio>(Character->GetPawnViewLocation() + FVector(100, 0, -35), FRotator::ZeroRotator, Spawn);
	auto* Second = World->SpawnActor<ATBNoiseRadio>(Character->GetPawnViewLocation() + FVector(-100, 0, -35), FRotator::ZeroRotator, Spawn);
	if (TestNotNull(TEXT("Radio"), Radio) && TestNotNull(TEXT("Second radio"), Second))
	{
		// State-machine tests isolate sight; base Conditions covers obstructed interaction.
		for (auto& Action : Radio->Actions) Action.bRequiresLineOfSight = false;
		for (auto& Action : Second->Actions) Action.bRequiresLineOfSight = false;
		FTBInteractionContext Use;
		Use.InstigatorCharacter = Character;
		Use.ActionId = TEXT("Start");
		TestFalse(TEXT("Initially off"), Radio->IsRadioOn());
		TestTrue(TEXT("Toy starts radio"), Radio->TryInteract(Use));
		TestTrue(TEXT("State is on"), Radio->IsRadioOn());
		TestFalse(TEXT("Repeated start is rejected"), Radio->TryInteract(Use));
		TestNotNull(TEXT("Missing asset generates placeholder sound"), Cast<USoundWaveProcedural>(Radio->RadioAudio->Sound));
		TestEqual(TEXT("Near listener gets strongest mask"), Radio->GetMaskAtLocation(Radio->GetActorLocation()), .25f);
		TestEqual(TEXT("Outside radius is unaffected"), Radio->GetMaskAtLocation(Radio->GetActorLocation() + FVector(1201, 0, 0)), 1.f);
		TestEqual(TEXT("Half distance interpolates"), Radio->GetMaskAtLocation(Radio->GetActorLocation() + FVector(600, 0, 0)), .625f);
		TestTrue(TEXT("Second radio starts"), Second->TryInteract(Use));
		Use.ActionId = TEXT("Stop");
		TestFalse(TEXT("Toy cannot stop"), Radio->TryInteract(Use));

		// Test the actual audio gain path using an in-memory silent source, independent of assets/devices.
		FTBAudioClip Clip;
		Clip.Sound = Radio->RadioAudio->Sound;
		Clip.bToyActionSound = true;
		GetMutableDefault<UTBAudioSettings>()->Sounds.Add(ETBSoundEvent::Footstep, Clip);
		GetMutableDefault<UTBAudioSettings>()->Sounds.Add(ETBSoundEvent::Jump, Clip);
		GetMutableDefault<UTBAudioSettings>()->Sounds.Add(ETBSoundEvent::Land, Clip);
		// An unrelated event can opt in without adding event-specific logic to playback.
		GetMutableDefault<UTBAudioSettings>()->Sounds.Add(ETBSoundEvent::JingleAlternate, Clip);
		Clip.bToyActionSound = false;
		GetMutableDefault<UTBAudioSettings>()->Sounds.Add(ETBSoundEvent::CollectLoop, Clip);
		GetMutableDefault<UTBAudioSettings>()->Sounds.Add(ETBSoundEvent::RescueLoop, Clip);
		GetMutableDefault<UTBAudioSettings>()->Sounds.Add(ETBSoundEvent::HumanFootstep, Clip);
		Audio->ReloadAudioSettings();
		Radio->Tick(0.f);
		Second->Tick(0.f);
		auto* ToyStep = Audio->StartLoop(ETBSoundEvent::Footstep, Character->GetRootComponent());
		auto* HumanStep = Audio->StartLoop(ETBSoundEvent::HumanFootstep, Character->GetRootComponent());
		if (TestNotNull(TEXT("Toy footstep component"), ToyStep) && TestNotNull(TEXT("Human footstep component"), HumanStep))
		{
			const float HumanVolume = HumanStep->VolumeMultiplier;
			TestTrue(TEXT("New toy steps are masked"), ToyStep->VolumeMultiplier < Audio->GetCategoryVolume(ETBAudioCategory::Effects));
			Audio->SetToyActionMask(Radio, .1f);
			TestTrue(TEXT("Existing toy steps update"), FMath::IsNearlyEqual(ToyStep->VolumeMultiplier, .1f * Audio->GetCategoryVolume(ETBAudioCategory::Effects)));
			TestEqual(TEXT("Human steps are not masked"), HumanStep->VolumeMultiplier, HumanVolume);
			Audio->SetEffectsVolume(.4f);
			TestTrue(TEXT("Volume preference preserves mask"), FMath::IsNearlyEqual(ToyStep->VolumeMultiplier, .1f * Audio->GetCategoryVolume(ETBAudioCategory::Effects)));
		}
		for (ETBSoundEvent Feedback : {ETBSoundEvent::CollectLoop, ETBSoundEvent::RescueLoop})
		{
			auto* FeedbackAudio = Audio->StartLoop(Feedback, Character->GetRootComponent());
			if (TestNotNull(TEXT("Progress feedback audio"), FeedbackAudio))
			{
				TestEqual(TEXT("Radio leaves progress feedback at normal volume"), FeedbackAudio->VolumeMultiplier,
				    Audio->GetCategoryVolume(ETBAudioCategory::Effects));
				Audio->SetToyActionMask(Radio, .05f);
				TestEqual(TEXT("Playing progress feedback remains unmasked"), FeedbackAudio->VolumeMultiplier,
				    Audio->GetCategoryVolume(ETBAudioCategory::Effects));
				Audio->StopLoop(FeedbackAudio);
			}
		}
		Audio->StopLoop(ToyStep);
		Audio->StopLoop(HumanStep);
		for (ETBSoundEvent Event : {ETBSoundEvent::Jump, ETBSoundEvent::Land, ETBSoundEvent::JingleAlternate})
		{
			auto* ActionAudio = Audio->StartLoop(Event, Character->GetRootComponent());
			if (TestNotNull(TEXT("Toy action creates audio"), ActionAudio))
			{
				TestTrue(TEXT("New toy action inherits current mask"), FMath::IsNearlyEqual(ActionAudio->VolumeMultiplier,
				    Audio->GetToyActionMask() * Audio->GetCategoryVolume(ETBAudioCategory::Effects)));
				Audio->SetToyActionMask(Radio, Radio->GetMaskAtLocation(Radio->GetActorLocation() + FVector(600, 0, 0)));
				Audio->RemoveToyActionMask(Second);
				TestTrue(TEXT("Playing action follows listener distance"), FMath::IsNearlyEqual(ActionAudio->VolumeMultiplier,
				    .625f * Audio->GetCategoryVolume(ETBAudioCategory::Effects)));
				Audio->SetToyActionMask(Radio, Radio->GetMaskAtLocation(Radio->GetActorLocation() + FVector(1201, 0, 0)));
				TestTrue(TEXT("Leaving radius restores playing action"), FMath::IsNearlyEqual(ActionAudio->VolumeMultiplier,
				    Audio->GetCategoryVolume(ETBAudioCategory::Effects)));
				Audio->StopLoop(ActionAudio);
			}
		}
		Radio->Tick(0.f);
		Second->Tick(0.f);
		Player->Team = ETBTeam::Human;
		TestTrue(TEXT("Human stops radio"), Radio->TryInteract(Use));
		TestFalse(TEXT("State is off"), Radio->IsRadioOn());
		TestTrue(TEXT("Other radio keeps mask active"), Audio->GetToyActionMask() < 1.f);
		Second->Destroy();
		TestEqual(TEXT("Destroying final radio clears mask"), Audio->GetToyActionMask(), 1.f);
		Use.ActionId = TEXT("Start");
		TestFalse(TEXT("Human cannot restart"), Radio->TryInteract(Use));
		Player->Team = ETBTeam::Toy;
		TestTrue(TEXT("Toy restarts stopped radio"), Radio->TryInteract(Use));
		State->Phase = ETBPhase::Results;
		Radio->Tick(.016f);
		TestFalse(TEXT("Round end stops radio"), Radio->IsRadioOn());
		TestEqual(TEXT("Round end removes mask"), Audio->GetToyActionMask(), 1.f);
		TestFalse(TEXT("Cannot start outside match"), Radio->TryInteract(Use));
	}
	if (IsValid(Radio)) Radio->Destroy();
	if (IsValid(Second)) Second->Destroy();
	GetMutableDefault<UTBAudioSettings>()->Sounds = OldSounds;
	Audio->ReloadAudioSettings();
	Audio->SetEffectsVolume(OldEffects);
	State->Phase = OldPhase;
	Player->Team = OldTeam;
	Player->ToyState = OldToyState;
	Player->bFrozen = bOldFrozen;
	return !HasAnyErrors();
}
#endif
