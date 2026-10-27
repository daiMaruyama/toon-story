#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/TBHumanEntrance.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Core/TBPlayerState.h"
#include "Core/TBLobbySettings.h"
#include "Character/TBCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBHumanEntranceTest, "ToonStory.Playroom.HumanEntrance",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBHumanEntranceTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	if (!Mode || !State || !Host || State->Phase != ETBPhase::Lobby || State->PlayerArray.Num() != 1 ||
	    World->GetGameInstance()->GetSubsystem<UTBLobbySettings>()->bDedicatedLobby || TBLobbySettings::ShouldTravelToStage(World)) return false;
	const auto Settings = State->Settings;
	const auto Starts = Mode->SpawnPoints;
	const auto Preference = Host->GetPlayerState<ATBPlayerState>()->Preference;
	const int32 OriginalNPCFailure = Mode->TestFailNPCAfter;
	auto* Guest = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!Guest) return false;
	TArray<AActor*> Actors;
	ON_SCOPE_EXIT
	{
		Mode->TestFailNPCAfter = OriginalNPCFailure;
		Mode->SpawnPoints = Starts;
		if (State->Phase == ETBPhase::Playing) Mode->Finish(ETBWinner::None, TEXT("Entrance fixture cleanup"), true);
		if (State->Phase == ETBPhase::Results || State->Phase == ETBPhase::Aborted) Mode->ReturnToLobby(Host);
		UGameplayStatics::RemovePlayer(Guest, true);
		for (auto* Actor : Actors) if (IsValid(Actor)) Actor->Destroy();
		State->Settings = Settings;
		Host->GetPlayerState<ATBPlayerState>()->Preference = Preference;
		Mode->SyncLobbyCounts();
	};
	auto* Entrance = World->SpawnActor<ATBHumanEntrance>(); Actors.Add(Entrance);
	if (!Entrance) return false;
	const FVector Origin(10000, 10000, 2000);
	Entrance->SetActorLocation(Origin);
	Entrance->DoorLeaf->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Entrance->ClosedDoorTransform = FTransform(FVector(500, -300, 70));
	Entrance->OpenDoorTransform = FTransform(FRotator(0, 90, 0), FVector(500, -300, 70));
	auto* Floor = World->SpawnActor<AActor>(); Actors.Add(Floor);
	auto* FloorBox = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(FloorBox);
	FloorBox->SetBoxExtent(FVector(500, 1000, 10));
	FloorBox->SetCollisionProfileName(TEXT("BlockAll")); FloorBox->RegisterComponent();
	Floor->SetActorLocation(Origin + FVector(0, 350, -10));
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		auto* Slot = World->SpawnActor<APlayerStart>(Origin + FVector(0, Index * 100, 67), FRotator(0, 146.321f + Index * 5, 0), Spawn);
		if (!TestNotNull(TEXT("Entrance fixture slot spawns"), Slot)) return false;
		Slot->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		Actors.Add(Slot); Entrance->DoorSlots.Add(Slot); Mode->SpawnPoints.Add(Slot);
	}
	auto MoveSlot = [this](APlayerStart* Slot, const FVector& Location)
	{
		const bool bMoved = TestTrue(TEXT("Fixture slot position change succeeds"), Slot->SetActorLocation(Location));
		const bool bAtLocation = TestTrue(TEXT("Fixture slot reaches the requested position"), Slot->GetActorLocation().Equals(Location, .01f));
		return bMoved && bAtLocation;
	};
	FString Error;
	TestTrue(TEXT("Eight supported non-overlapping slots validate"), Entrance->Validate(8, Error));
	TestFalse(TEXT("Insufficient capacity is rejected"), Entrance->Validate(9, Error));
	const FVector SlotOne = Entrance->DoorSlots[1]->GetActorLocation();
	if (!MoveSlot(Entrance->DoorSlots[1].Get(), Entrance->DoorSlots[0]->GetActorLocation())) return false;
	TestFalse(TEXT("Overlapping capsules are rejected"), Entrance->Validate(8, Error));
	if (!MoveSlot(Entrance->DoorSlots[1].Get(), SlotOne)) return false;
	FloorBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Slots without supporting floor are rejected"), Entrance->Validate(8, Error));
	FloorBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FloorBox->SetRelativeRotation(FRotator(60, 0, 0));
	TestFalse(TEXT("Steep supporting surface is rejected"), Entrance->Validate(8, Error));
	FloorBox->SetRelativeRotation(FRotator::ZeroRotator);
	if (!MoveSlot(Entrance->DoorSlots[0].Get(), Origin + FVector(0, 0, 20))) return false;
	TestFalse(TEXT("Embedded/head-clearance-invalid capsule is rejected"), Entrance->Validate(8, Error));
	if (!MoveSlot(Entrance->DoorSlots[0].Get(), Origin + FVector(0, 0, 67))) return false;
	auto* Ceiling = World->SpawnActor<AActor>(); Actors.Add(Ceiling);
	auto* CeilingBox = NewObject<UBoxComponent>(Ceiling); Ceiling->SetRootComponent(CeilingBox);
	CeilingBox->SetBoxExtent(FVector(40, 40, 5)); CeilingBox->SetCollisionProfileName(TEXT("BlockAll")); CeilingBox->RegisterComponent();
	Ceiling->SetActorLocation(Origin + FVector(0, 0, 125));
	TestFalse(TEXT("Head-height obstacle is rejected despite a supported floor"), Entrance->Validate(8, Error));
	CeilingBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Entrance->SetPreparing(true);
	TestEqual(TEXT("Preparation door blocks pawns"), Entrance->DoorLeaf->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	Entrance->SetPreparing(false);
	TestEqual(TEXT("Cancelled door is passable"), Entrance->DoorLeaf->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	auto Rules = State->Settings; Rules.Humans = Rules.Toys = 1; Rules.NPCHumans = Rules.NPCToys = 0; Rules.RequiredItems = 1;
	Mode->SetRules(Host, Rules);
	Host->GetPlayerState<ATBPlayerState>()->Preference = ETBTeam::Human;
	Guest->GetPlayerState<ATBPlayerState>()->Preference = ETBTeam::Toy;
	Host->GetPlayerState<ATBPlayerState>()->bReady = Guest->GetPlayerState<ATBPlayerState>()->bReady = true;
	const FRotator InitialHumanView(-12, -73, 4), InitialToyView(0, 38, 0);
	Host->SetControlRotation(InitialHumanView);
	Guest->SetControlRotation(InitialToyView);
	const FVector BeforeRejectedStart = Host->GetPawn()->GetActorLocation();
	CeilingBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mode->StartRound(Host);
	TestEqual(TEXT("Unsafe head clearance never starts the match"), State->Phase, ETBPhase::Lobby);
	TestEqual(TEXT("Rejected entrance never starts the timer"), State->EndTime, 0.0);
	TestTrue(TEXT("Rejected entrance leaves the pawn in place"), Host->GetPawn()->GetActorLocation().Equals(BeforeRejectedStart, 1.f));
	TestEqual(TEXT("Rejected entrance leaves no blocking gate"), Entrance->DoorLeaf->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Rejected entrance never changes human view"), Host->GetControlRotation().Equals(InitialHumanView, .01f));
	TestTrue(TEXT("Rejected entrance never changes toy view"), Guest->GetControlRotation().Equals(InitialToyView, .01f));
	CeilingBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rules.NPCToys = 1;
	Mode->SetRules(Host, Rules);
	Mode->TestFailNPCAfter = 0;
	Host->GetPlayerState<ATBPlayerState>()->bReady = Guest->GetPlayerState<ATBPlayerState>()->bReady = true;
	Mode->StartRound(Host);
	if (!TestTrue(TEXT("NPC failure is reached after player placement"), Mode->bTestNPCFailureReached) ||
	    !TestEqual(TEXT("NPC failure rolls back to Lobby"), State->Phase, ETBPhase::Lobby)) return false;
	TestTrue(TEXT("Post-placement rollback preserves human view"), Host->GetControlRotation().Equals(InitialHumanView, .01f));
	TestTrue(TEXT("Post-placement rollback preserves toy view"), Guest->GetControlRotation().Equals(InitialToyView, .01f));
	TestTrue(TEXT("NPC failure restores the human position"), Host->GetPawn()->GetActorLocation().Equals(BeforeRejectedStart, 1.f));
	Mode->TestFailNPCAfter = INDEX_NONE;
	Rules.NPCToys = 0;
	Mode->SetRules(Host, Rules);
	Host->GetPlayerState<ATBPlayerState>()->bReady = Guest->GetPlayerState<ATBPlayerState>()->bReady = true;
	Mode->SpawnCursor = Starts.Num(); // Intentionally make the normal list encounter the door markers first.
	Mode->StartRound(Host);
	if (!TestTrue(TEXT("Entrance placement commits preparation"), State->Phase == ETBPhase::Playing &&
	    State->HidePreparation.bValid && State->HidePreparation.bHiding && State->IsPreparing())) return false;
	const FRotator FirstFacing(0, Entrance->DoorSlots[0]->GetActorRotation().Yaw, 0);
	TestTrue(TEXT("Committed entrance human adopts slot yaw with zero pitch and roll"), Host->GetControlRotation().Equals(FirstFacing, .01f));
	TestTrue(TEXT("Committed entrance never changes toy view"), Guest->GetControlRotation().Equals(InitialToyView, .01f));
	TestTrue(TEXT("Human was placed at the door before the ten-second wait"), Host->GetPawn()->GetActorLocation().Equals(Entrance->DoorSlots[0]->GetActorLocation(), 1.f));
	for (const auto& Slot : Entrance->DoorSlots)
		TestFalse(TEXT("Toy never uses a human door marker"), Guest->GetPawn()->GetActorLocation().Equals(Slot->GetActorLocation(), 1.f));
	Host->PlayerTick(0);
	TestTrue(TEXT("Confirmed human uses the approach camera"), Host->IsShowingEntranceApproach());
	const FVector Locked = Host->GetPawn()->GetActorLocation();
	Mode->AdvancePreparationClockForTest(0);
	Host->PlayerTick(0);
	TestFalse(TEXT("Release leaves approach view"), Host->IsShowingEntranceApproach());
	TestTrue(TEXT("Release does not teleport the human again"), Host->GetPawn()->GetActorLocation().Equals(Locked, 1.f));
	TestEqual(TEXT("Release opens the actual gate"), Entrance->DoorLeaf->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Release retains the committed door facing"), Host->GetControlRotation().Equals(FirstFacing, .01f));
	const FRotator PlayerChosenView(0, 67, 0);
	Host->SetControlRotation(PlayerChosenView);
	Host->PlayerTick(0);
	TestTrue(TEXT("Entrance alignment is one-shot, not a per-frame view override"), Host->GetControlRotation().Equals(PlayerChosenView, .01f));
	Mode->Finish(ETBWinner::None, TEXT("Entrance facing rematch"), true);
	Mode->ReturnToLobby(Host);
	const FRotator RematchFacing(0, -118.75f, 0);
	if (!TestTrue(TEXT("Fixture door yaw changes for rematch"), Entrance->DoorSlots[0]->SetActorRotation(RematchFacing)) ||
	    !TestTrue(TEXT("Fixture rematch door yaw is exact"), Entrance->DoorSlots[0]->GetActorRotation().Equals(RematchFacing, .01f))) return false;
	Host->SetControlRotation(InitialHumanView);
	Guest->SetControlRotation(InitialToyView);
	Host->GetPlayerState<ATBPlayerState>()->bReady = Guest->GetPlayerState<ATBPlayerState>()->bReady = true;
	Mode->StartRound(Host);
	if (!TestTrue(TEXT("Rematch commits preparation"), State->Phase == ETBPhase::Playing && State->IsPreparing())) return false;
	TestTrue(TEXT("Rematch uses the new slot yaw instead of the prior round"), Host->GetControlRotation().Equals(RematchFacing, .01f));
	TestTrue(TEXT("Rematch leaves toy view unchanged"), Guest->GetControlRotation().Equals(InitialToyView, .01f));
	Host->PlayerTick(0);
	Mode->AdvancePreparationClockForTest(0);
	Host->PlayerTick(0);
	TestTrue(TEXT("Rematch release retains the new slot yaw"), Host->GetControlRotation().Equals(RematchFacing, .01f));
	Mode->Finish(ETBWinner::None, TEXT("Entrance-free fallback view"), true);
	Mode->ReturnToLobby(Host);
	Entrance->Destroy();
	Mode->SpawnPoints = Starts;
	Host->SetControlRotation(InitialHumanView);
	Guest->SetControlRotation(InitialToyView);
	Host->GetPlayerState<ATBPlayerState>()->bReady = Guest->GetPlayerState<ATBPlayerState>()->bReady = true;
	Mode->StartRound(Host);
	if (!TestTrue(TEXT("Map without entrance retains preparation fallback"), State->Phase == ETBPhase::Playing && State->IsPreparing())) return false;
	TestTrue(TEXT("No-entrance commit preserves human view"), Host->GetControlRotation().Equals(InitialHumanView, .01f));
	TestTrue(TEXT("No-entrance commit preserves toy view"), Guest->GetControlRotation().Equals(InitialToyView, .01f));
	Mode->AdvancePreparationClockForTest(0);
	Host->PlayerTick(0);
	TestTrue(TEXT("No-entrance release preserves human view"), Host->GetControlRotation().Equals(InitialHumanView, .01f));
	return true;
}
#endif
