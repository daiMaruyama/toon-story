#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBCharacter.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Audio/TBAudioEvents.h"
#include "Audio/TBAudioSettings.h"
#include "Data/TBRuleData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/InputSettings.h"
#include "Sound/SoundWave.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBToyTauntTest, "ToonStory.Audio.ToyTaunt",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBToyTauntTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World() &&
		    Context.World()->GetGameState<ATBGameState>())
		{ World = Context.World(); break; }
	if (!TestNotNull(TEXT("Standalone game"), World) || World->GetNetMode() != NM_Standalone) return false;
	auto* PC = World->GetFirstPlayerController();
	auto* Character = PC ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Player state"), Character->TBPS())) return false;
	auto* Player = Character->TBPS();
	auto* State = World->GetGameState<ATBGameState>();
	const auto OldPhase = State->Phase;
	const auto OldTeam = Player->Team;
	const auto OldToyState = Player->ToyState;
	const bool OldFrozen = Player->bFrozen;
	ON_SCOPE_EXIT
	{
		State->Phase = OldPhase;
		Player->Team = OldTeam;
		Player->ToyState = OldToyState;
		Player->bFrozen = OldFrozen;
	};
	Player->Team = ETBTeam::Toy;
	Player->ToyState = ETBToyState::Free;
	Player->bFrozen = false;
	State->Phase = ETBPhase::Lobby;
	TestFalse(TEXT("Lobby cannot taunt"), Character->TryTaunt());
	State->Phase = ETBPhase::Playing;
	Player->Team = ETBTeam::Human;
	TestFalse(TEXT("Humans cannot taunt"), Character->TryTaunt());
	TestTrue(TEXT("Humans hear taunts"), UTBAudioEvents::ShouldDeliver(ETBSoundEvent::ToyTaunt, Player, Player));
	Player->Team = ETBTeam::Toy;
	TestTrue(TEXT("Toys hear taunts"), UTBAudioEvents::ShouldDeliver(ETBSoundEvent::ToyTaunt, Player, Player));
	Player->bFrozen = true;
	TestFalse(TEXT("Frozen toy cannot taunt"), Character->TryTaunt());
	Player->bFrozen = false;
	for (auto ToyState : {ETBToyState::Grabbed, ETBToyState::Carried, ETBToyState::Storing, ETBToyState::Boxed})
	{
		Player->ToyState = ToyState;
		TestFalse(TEXT("Captured toy cannot taunt"), Character->TryTaunt());
	}
	Player->ToyState = ETBToyState::Free;
	TestTrue(TEXT("Free toy emits confirmed sound"), Character->TryTaunt());
	TestFalse(TEXT("Repeated request is rate limited"), Character->TryTaunt());
	State->Phase = ETBPhase::Results;
	TestFalse(TEXT("Results reject taunt"), Character->TryTaunt());
	const auto* Clip = GetDefault<UTBAudioSettings>()->Sounds.Find(ETBSoundEvent::ToyTaunt);
	if (TestNotNull(TEXT("Taunt catalog entry"), Clip))
	{
		TestTrue(TEXT("Spatial toy action follows mixer and masking"), Clip->bSpatial && Clip->bToyActionSound);
		auto* Sound = Cast<USoundWave>(Clip->Sound.LoadSynchronous());
		if (TestNotNull(TEXT("Placeholder asset loads"), Sound))
		{
			TestEqual(TEXT("Spatial sound is mono"), Sound->NumChannels, 1);
			TestTrue(TEXT("Short non-looping horn"), !Sound->bLooping && Sound->Duration > .1f && Sound->Duration < 1.f);
		}
	}
	TArray<FInputActionKeyMapping> Keys;
	GetDefault<UInputSettings>()->GetActionMappingByName(TEXT("Taunt"), Keys);
	TestTrue(TEXT("F is mapped to Taunt"), Keys.ContainsByPredicate([](const FInputActionKeyMapping& Key) { return Key.Key == EKeys::F; }));
	TestTrue(TEXT("Cooldown comes from rule data"), TB::Rules().Toys.TauntCooldownSeconds > 0);
	return true;
}
#endif
