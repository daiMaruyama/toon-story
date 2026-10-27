// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBController.h"
#include "Core/TBLobbySettings.h"
#include "Character/TBCharacter.h"
#include "Character/TBNPCController.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRoundStartTest, "ToonStory.Match.RoundStartPlacement",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBRoundStartTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World() &&
		    Context.World()->GetAuthGameMode<ATBGameMode>()) { World = Context.World(); break; }
	if (!TestNotNull(TEXT("Standalone Arena world required"), World)) return false;
	if (!TestEqual(TEXT("No network players are modified"), World->GetNetMode(), NM_Standalone)) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	if (!State || !Host || State->Phase != ETBPhase::Lobby || State->PlayerArray.Num() != 1) return false;
	auto* Pending = World->GetGameInstance()->GetSubsystem<UTBLobbySettings>();
	if (Pending->bDedicatedLobby || TBLobbySettings::ShouldTravelToStage(World))
	{
		AddError(TEXT("Run this fixture directly in Arena, without stage travel."));
		return false;
	}
	const auto OriginalSettings = State->Settings;
	const auto OriginalStarts = Mode->SpawnPoints;
	const int32 OriginalCursor = Mode->SpawnCursor;
	const ETBTeam OriginalPreference = Host->GetPlayerState<ATBPlayerState>()->Preference;
	auto* Second = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!TestNotNull(TEXT("Second player"), Second)) return false;
	TArray<AActor*> FixtureActors;
	ON_SCOPE_EXIT
	{
		Mode->TestFailNPCAfter = INDEX_NONE;
		Mode->bTestFailRollback = false;
		Mode->SpawnPoints = OriginalStarts;
		if (State->Phase == ETBPhase::Playing) Mode->Finish(ETBWinner::Humans, TEXT("Round start fixture cleanup"));
		if (State->Phase == ETBPhase::Aborted || State->Phase == ETBPhase::Results) Mode->ReturnToLobby(Host);
		Mode->RemoveNPCs();
		UGameplayStatics::RemovePlayer(Second, true);
		for (auto* Actor : FixtureActors) if (IsValid(Actor)) Actor->Destroy();
		State->Settings = OriginalSettings;
		State->Collected = 0; State->BoxedCount = 0; State->Winner = ETBWinner::None;
		State->EndTime = 0; State->Reason.Reset();
		Host->GetPlayerState<ATBPlayerState>()->Preference = OriginalPreference;
		Host->GetPlayerState<ATBPlayerState>()->bReady = false;
		Mode->SpawnCursor = OriginalCursor;
	};
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* FirstStart = World->SpawnActor<APlayerStart>(FVector(0, 0, 20000), FRotator::ZeroRotator, Spawn);
	auto* SecondStart = World->SpawnActor<APlayerStart>(FVector(2000, 0, 20000), FRotator::ZeroRotator, Spawn);
	if (!FirstStart || !SecondStart) return false;
	FixtureActors.Add(FirstStart); FixtureActors.Add(SecondStart);
	auto* Wall = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Collision fixture"), Wall)) return false;
	FixtureActors.Add(Wall);
	auto* Blocker = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Blocker);
	Blocker->SetBoxExtent(FVector(600));
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));
	Blocker->RegisterComponent();
	Wall->SetActorLocation(SecondStart->GetActorLocation());
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FTBSettings Rules = State->Settings;
	Rules.Humans = 1; Rules.Toys = 1; Rules.NPCHumans = 0; Rules.NPCToys = 0;
	Rules.Duration = 60; Rules.RequiredItems = 1;
	Mode->SetRules(Host, Rules);
	auto* Human = Cast<ATBCharacter>(Host->GetPawn());
	auto* Toy = Cast<ATBCharacter>(Second->GetPawn());
	if (!Human || !Toy) return false;
	auto* HumanInfo = Human->TBPS();
	auto* ToyInfo = Toy->TBPS();
	HumanInfo->Preference = ETBTeam::Human;
	ToyInfo->Preference = ETBTeam::Toy;
	ToyInfo->Items = 3;
	ToyInfo->bFrozen = true;
	Toy->ApplyState();
	Human->TeleportTo(FVector(0, 2000, 20000), FRotator(0, 15, 0));
	Toy->TeleportTo(FVector(2000, 2000, 20000), FRotator(0, 30, 0));
	const FTransform HumanOriginal = Human->GetActorTransform(), ToyOriginal = Toy->GetActorTransform();
	const FVector SavedVelocity(12, 23, -1);
	Human->GetCharacterMovement()->Velocity = SavedVelocity;
	auto Ready = [&]()
	{
		HumanInfo->bReady = true; ToyInfo->bReady = true;
		Mode->SpawnCursor = 0;
		State->Collected = 2; State->BoxedCount = 1; State->Winner = ETBWinner::Toys;
		State->EndTime = 123;
	};
	auto CheckFailure = [&](bool Recovered)
	{
		TestEqual(TEXT("Failed preparation never enters Playing"), State->Phase,
		    Recovered ? ETBPhase::Lobby : ETBPhase::Aborted);
		TestEqual(TEXT("No running deadline after failure"), State->EndTime, 0.0);
		TestEqual(TEXT("Collection not committed"), State->Collected, 2);
		TestEqual(TEXT("Boxed count not committed"), State->BoxedCount, 1);
		TestEqual(TEXT("Cursor restored"), Mode->SpawnCursor, 0);
		TestFalse(TEXT("All readiness revoked"), HumanInfo->bReady || ToyInfo->bReady);
		TestEqual(TEXT("Human role restored"), HumanInfo->Team, ETBTeam::None);
		TestEqual(TEXT("Toy role restored"), ToyInfo->Team, ETBTeam::None);
		TestEqual(TEXT("Toy state restored"), ToyInfo->ToyState, ETBToyState::Free);
		TestTrue(TEXT("Frozen state restored"), ToyInfo->bFrozen);
		TestEqual(TEXT("Personal item count restored"), ToyInfo->Items, 3);
		TestEqual(TEXT("Explicit preference unchanged"), ToyInfo->Preference, ETBTeam::Toy);
		TestTrue(TEXT("Failure reason sent to host"), !State->Reason.IsEmpty() && Host->Notice == State->Reason);
		TestTrue(TEXT("Failure reason sent to other player"), Second->Notice == State->Reason);
		TestFalse(TEXT("No partial NPCs remain"), bool(TActorIterator<ATBNPCController>(World)));
		if (Recovered)
		{
			TestTrue(TEXT("First player returned to original position"), Human->GetActorTransform().Equals(HumanOriginal, 1.f));
			TestTrue(TEXT("Second player returned to original position"), Toy->GetActorTransform().Equals(ToyOriginal, 1.f));
			TestTrue(TEXT("Velocity restored after placement rollback"), Human->GetCharacterMovement()->Velocity.Equals(SavedVelocity));
			TestEqual(TEXT("Winner was not committed"), State->Winner, ETBWinner::Toys);
		}
	};

	Mode->SpawnPoints = {FirstStart, nullptr};
	Ready(); Mode->StartRound(Host); CheckFailure(true);
	Mode->SpawnPoints = {FirstStart, SecondStart};
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Ready(); Mode->StartRound(Host); CheckFailure(true);
	TestTrue(TEXT("Collision failure has a placement diagnostic"), State->Reason.Contains(TEXT("塞がれ")));
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Create one real NPC, then inject failure before the next one. Both must be cleaned up.
	State->Settings.NPCHumans = 1; State->Settings.NPCToys = 1;
	Mode->TestFailNPCAfter = 1;
	Ready(); Mode->StartRound(Host); CheckFailure(true);
	TestTrue(TEXT("NPC failure path reached"), State->Reason.Contains(TEXT("NPC")));
	TestEqual(TEXT("One real NPC character was created before failure"), Mode->TestSuccessfulNPCs, 1);
	TestTrue(TEXT("Failure occurred at the requested seam"), Mode->bTestNPCFailureReached);
	Mode->TestFailNPCAfter = INDEX_NONE;
	State->Settings.NPCHumans = 0; State->Settings.NPCToys = 0;
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mode->bTestFailRollback = true;
	Ready(); Mode->StartRound(Host); CheckFailure(false);
	TestEqual(TEXT("Unsafe recovery has no winner"), State->Winner, ETBWinner::None);
	HumanInfo->bReady = true; ToyInfo->bReady = true;
	Mode->StartRound(Host);
	TestEqual(TEXT("Aborted match cannot start again"), State->Phase, ETBPhase::Aborted);
	Mode->bTestFailRollback = false;
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mode->ReturnToLobby(Host);
	// ReturnToLobby is the existing explicit user recovery path; it replaces both pawns.
	Human = Cast<ATBCharacter>(Host->GetPawn()); Toy = Cast<ATBCharacter>(Second->GetPawn());
	if (!Human || !Toy) return false;
	HumanInfo = Human->TBPS(); ToyInfo = Toy->TBPS();
	Mode->SpawnPoints = {FirstStart, SecondStart};
	HumanInfo->bReady = true; ToyInfo->bReady = true;
	Mode->SpawnCursor = 0;
	Mode->StartRound(Host);
	TestEqual(TEXT("Explicit lobby recovery and re-ready can start"), State->Phase, ETBPhase::Playing);
	TestTrue(TEXT("Successful commit begins the hiding preparation"), State->IsPreparing());
	TestEqual(TEXT("Normal duration has not started at commit"), State->EndTime, 0.0);
	TestEqual(TEXT("Preparation preserves the full match duration"), State->Remaining(), State->Settings.Duration);
	TestEqual(TEXT("Successful start clears counters"), State->Collected, 0);
	TestTrue(TEXT("Successful start clears failure reason"), State->Reason.IsEmpty());
	TestTrue(TEXT("Real first teleport reaches the start"), Human->GetActorLocation().Equals(FirstStart->GetActorLocation(), 1.f));
	TestTrue(TEXT("Real second teleport reaches the start"), Toy->GetActorLocation().Equals(SecondStart->GetActorLocation(), 1.f));
	Mode->Finish(ETBWinner::Humans, TEXT("Round start normal path"));
	Mode->ReturnToLobby(Host);
	State->Settings.NPCHumans = 1; State->Settings.NPCToys = 1;
	Host->GetPlayerState<ATBPlayerState>()->bReady = true;
	Second->GetPlayerState<ATBPlayerState>()->bReady = true;
	Mode->SpawnCursor = 0;
	Mode->StartRound(Host);
	TestEqual(TEXT("Rematch with real NPC spawning starts"), State->Phase, ETBPhase::Playing);
	int32 NPCCount = 0;
	for (TActorIterator<ATBNPCController> It(World); It; ++It)
	{
		++NPCCount;
		TestTrue(TEXT("Every committed NPC has a character"), IsValid(Cast<ATBCharacter>(It->GetPawn())));
	}
	TestEqual(TEXT("Both requested NPCs committed"), NPCCount, 2);
	TestEqual(TEXT("Both real NPC character creations observed"), Mode->TestSuccessfulNPCs, 2);
	TestFalse(TEXT("Successful rematch never reached failure seam"), Mode->bTestNPCFailureReached);
	return true;
}
#endif
