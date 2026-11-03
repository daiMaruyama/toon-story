#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Items/TBWalkieTalkie.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Tests/TBTestMatch.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBWalkieTalkieTest, "ToonStory.Items.WalkieTalkie",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBWalkieTalkieTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Entry : GEngine->GetWorldContexts())
		if (Entry.WorldType == EWorldType::Game && Entry.World() && Entry.World()->GetGameState<ATBGameState>()) { World = Entry.World(); break; }
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	auto* Controller = World->GetFirstPlayerController();
	auto* Character = Controller ? Cast<ATBCharacter>(Controller->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Player state"), Character->TBPS())) return false;
	auto* Player = Character->TBPS();
	const auto OldTeam = Player->Team;
	const auto OldToyState = Player->ToyState;
	const bool OldFrozen = Player->bFrozen;
	ON_SCOPE_EXIT { Player->Team = OldTeam; Player->ToyState = OldToyState; Player->bFrozen = OldFrozen; };
	TBTest::FPlayingScope Playing(*World->GetGameState<ATBGameState>());
	Player->Team = ETBTeam::Toy; Player->ToyState = ETBToyState::Free; Player->bFrozen = false;
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* A = World->SpawnActor<ATBWalkieTalkie>(Character->GetPawnViewLocation() + FVector(100, 0, -11), FRotator::ZeroRotator, Params);
	auto* B = World->SpawnActor<ATBWalkieTalkie>(Character->GetPawnViewLocation() + FVector(-100, 0, -11), FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("First"), A) || !TestNotNull(TEXT("Second"), B)) return false;
	ON_SCOPE_EXIT { A->Destroy(); B->Destroy(); };
	A->Actions[0].bRequiresLineOfSight = false;
	FTBInteractionContext Use; Use.InstigatorCharacter = Character;
	TestFalse(TEXT("Unpaired device cannot transmit"), A->TryInteract(Use));
	TestFalse(TEXT("Cannot pair with itself"), A->PairWith(A));
	TestTrue(TEXT("Devices pair"), A->PairWith(B));
	TestTrue(TEXT("Pair is reciprocal"), B->Partner == A);
	TestTrue(TEXT("Press starts transmitting"), A->TryInteract(Use));
	TestTrue(TEXT("Transmitter is recorded"), A->Transmitter == Character);
	TestFalse(TEXT("Busy device rejects another transmission"), A->TryInteract(Use));
	A->StopTransmitting(Character);
	TestNull(TEXT("Release stops transmission"), A->Transmitter.Get());
	B->Destroy();
	TestNull(TEXT("Destroying partner clears pair"), A->Partner.Get());
	return true;
}
#endif
