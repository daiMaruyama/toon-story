#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/TBBoxEscapeCourse.h"
#include "GamePlay/TBBox.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Core/TBPlayerState.h"
#include "Core/TBLobbySettings.h"
#include "Character/TBCharacter.h"
#include "Character/TBNPCController.h"
#include "Data/TBRuleData.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TriggerBox.h"
#include "Engine/TargetPoint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBBoxEscapeTest, "ToonStory.Playroom.BoxEscape",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBBoxEscapeTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	if (!Mode || !State || !Host || !State->Box || State->Phase != ETBPhase::Lobby || State->PlayerArray.Num() != 1 ||
	    World->GetGameInstance()->GetSubsystem<UTBLobbySettings>()->bDedicatedLobby || TBLobbySettings::ShouldTravelToStage(World)) return false;
	const auto OriginalSettings = State->Settings;
	const auto Preference = Host->GetPlayerState<ATBPlayerState>()->Preference;
	auto* Guest = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!Guest) return false;
	ATBController* Third = nullptr;
	auto* Box = State->Box.Get();
	TArray<AActor*> Actors;
	ON_SCOPE_EXIT
	{
		if (State->Phase == ETBPhase::Playing) Mode->Finish(ETBWinner::None, TEXT("Escape fixture cleanup"), true);
		if (State->Phase == ETBPhase::Results || State->Phase == ETBPhase::Aborted) Mode->ReturnToLobby(Host);
		if (IsValid(Guest)) UGameplayStatics::RemovePlayer(Guest, true);
		if (IsValid(Third)) UGameplayStatics::RemovePlayer(Third, true);
		for (auto* Actor : Actors) if (IsValid(Actor)) Actor->Destroy();
		State->Settings = OriginalSettings;
		Host->GetPlayerState<ATBPlayerState>()->Preference = Preference;
		Mode->SyncLobbyCounts();
	};
	const FBox OriginalRoomBounds = Box->StorageRoomBounds();
	TestTrue(TEXT("Storage bounds are available from the box collision geometry"), OriginalRoomBounds.IsValid != 0);
	UStaticMeshComponent* RoomWall = nullptr;
	for (const auto& Child : Box->StorageRoom->GetAttachChildren())
		if (auto* Wall = Cast<UStaticMeshComponent>(Child.Get()); Wall && Wall->IsQueryCollisionEnabled())
		{ RoomWall = Wall; break; }
	if (!TestNotNull(TEXT("Room collision is present"), RoomWall)) return false;
	const FVector WallLocation = RoomWall->GetRelativeLocation();
	const auto WallMobility = RoomWall->Mobility;
	RoomWall->SetMobility(EComponentMobility::Movable);
	RoomWall->SetRelativeLocation(WallLocation + FVector(10000, 0, 0));
	TestTrue(TEXT("Bounds follow edited room geometry instead of a copied box size"),
	    Box->StorageRoomBounds().Max.X > OriginalRoomBounds.Max.X);
	RoomWall->SetRelativeLocation(WallLocation);
	RoomWall->SetMobility(WallMobility);
	auto* Course = World->SpawnActor<ATBBoxEscapeCourse>(); Actors.Add(Course);
	Course->Box = Box;
	Course->SetActorTransform(Box->StorageRoom->GetComponentTransform());
	Course->CourseBounds->SetRelativeLocation(FVector(400, 0, 160));
	Course->CourseBounds->SetBoxExtent(FVector(390, 290, 159));
	const FTransform Room = Box->StorageRoom->GetComponentTransform();
	Course->Goal = World->SpawnActor<ATriggerBox>(); Actors.Add(Course->Goal.Get());
	Course->Goal->SetActorLocation(Room.TransformPosition(FVector(400, 100, 120)));
	Course->Goal->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	auto* Exit = World->SpawnActor<ATargetPoint>(); Actors.Add(Exit);
	Exit->SetActorLocation(FVector(-1700, -1700, 12)); Course->ExitPoints = {Exit};
	FString Error;
	if (!TestTrue(TEXT("Height escape validates without checkpoints or a reset point"), Course->Validate(Error)))
	{ AddInfo(Error); return false; }
	const FVector ExitLocation = Exit->GetActorLocation();
	Exit->SetActorLocation(Room.TransformPosition(FVector(600, 100, 12)));
	TestFalse(TEXT("Exit inside the storage room is rejected"), Course->Validate(Error));
	Exit->SetActorLocation(FVector(-1700, -1700, 500));
	TestFalse(TEXT("Unsupported exit is rejected"), Course->Validate(Error));
	Exit->SetActorLocation(ExitLocation);
	Third = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!TestNotNull(TEXT("Third player keeps one toy outside"), Third)) return false;
	Third->GetPlayerState<ATBPlayerState>()->Preference = ETBTeam::Toy;
	auto Rules = State->Settings;
	Rules.Humans = 1; Rules.Toys = 2; Rules.NPCHumans = Rules.NPCToys = 0; Rules.Duration = 120; Rules.RequiredItems = 1;
	Mode->SetRules(Host, Rules);
	Host->GetPlayerState<ATBPlayerState>()->Preference = ETBTeam::Human;
	Guest->GetPlayerState<ATBPlayerState>()->Preference = ETBTeam::Toy;
	auto Begin = [&]() -> ATBCharacter*
	{
		if (State->Phase == ETBPhase::Playing) Mode->Finish(ETBWinner::None, TEXT("Height fixture next"), true);
		if (State->Phase == ETBPhase::Results || State->Phase == ETBPhase::Aborted) Mode->ReturnToLobby(Host);
		Host->GetPlayerState<ATBPlayerState>()->bReady = Guest->GetPlayerState<ATBPlayerState>()->bReady = Third->GetPlayerState<ATBPlayerState>()->bReady = true;
		Mode->StartRound(Host); Mode->AdvancePreparationClockForTest(0);
		return Cast<ATBCharacter>(Guest->GetPawn());
	};
	auto* Toy = Begin();
	if (!Toy || State->Phase != ETBPhase::Playing || State->IsPreparing()) return false;
	auto Place = [&](FVector Local)
	{
		const FVector Position = Room.TransformPosition(Local);
		Toy->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
		Course->Tick(0);
	};
	TestTrue(TEXT("Real capture stores the toy"), Box->Store(Toy));
	TestEqual(TEXT("One toy outside keeps the round playing"), State->Phase, ETBPhase::Playing);
	const uint32 FirstCapture = Box->CaptureGeneration(Toy);
	Place(FVector(200, 0, 220));
	TestEqual(TEXT("High storage drop cannot immediately release a fresh capture"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(FVector(200, 0, 160));
	TestEqual(TEXT("Still descending above the line remains boxed"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(FVector(200, 0, 120));
	Place(FVector(200, 0, 121));
	TestEqual(TEXT("Merely touching the line does not arm an escape"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(FVector(200, 0, 20));
	TestTrue(TEXT("A fall stays at the actual position, without teleporting to a start"),
	    Toy->GetActorLocation().Equals(Room.TransformPosition(FVector(200, 0, 20)), .01f));
	Place(FVector(200, 0, -100));
	TestTrue(TEXT("Even falling below the course does not reset or abort"),
	    Toy->GetActorLocation().Equals(Room.TransformPosition(FVector(200, 0, -100)), .01f) && State->Phase == ETBPhase::Playing);
	Place(FVector(2000, 0, 220));
	TestEqual(TEXT("Height outside the box cannot escape"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(FVector(200, 0, 120));
	TestEqual(TEXT("At the line is not above it"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(FVector(200, 0, 121));
	TestEqual(TEXT("Ascending after the drop escapes without visiting the marker XY"), Toy->TBPS()->ToyState, ETBToyState::Free);
	TestTrue(TEXT("Escape reaches the safe exit"), Toy->GetActorLocation().Equals(ExitLocation, 1.f));
	TestTrue(TEXT("Recapture succeeds"), Box->Store(Toy));
	Place(FVector(200, 0, 220));
	TestEqual(TEXT("Recapture must descend again; previous arming is not reused"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	TestFalse(TEXT("Old capture token cannot release a new capture"), Box->ReleasePrisoner(Toy, FirstCapture, {Exit->GetActorTransform()}));
	Place(FVector(200, 0, 20));
	auto* Blocker = World->SpawnActor<AActor>(); Actors.Add(Blocker);
	auto* Collision = NewObject<UBoxComponent>(Blocker); Blocker->SetRootComponent(Collision);
	Collision->SetBoxExtent(FVector(100)); Collision->SetCollisionProfileName(TEXT("BlockAll")); Collision->RegisterComponent();
	Blocker->SetActorLocation(ExitLocation);
	Place(FVector(200, 0, 121));
	TestEqual(TEXT("A blocked exit keeps the toy boxed"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Course->Tick(0);
	TestEqual(TEXT("An unblocked exit releases on the next check"), Toy->TBPS()->ToyState, ETBToyState::Free);
	TestTrue(TEXT("Another capture succeeds"), Box->Store(Toy));
	Place(FVector(200, 0, 20));
	State->EndTime = State->GetServerWorldTimeSeconds() - .01;
	Mode->CheckWin(); // The game mode ticks before the course in the real world.
	Place(FVector(200, 0, 121));
	TestEqual(TEXT("Deadline takes precedence over height escape"), State->Winner, ETBWinner::Humans);
	TestEqual(TEXT("Expired round cannot release the toy"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Toy = Begin();
	if (!Toy) return false;
	Toy->SetToyState(ETBToyState::Boxed);
	Place(FVector(200, 0, 20)); Place(FVector(200, 0, 121));
	TestEqual(TEXT("Boxed state without a real capture cannot authorize escape"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Toy = Begin();
	if (!Toy) return false;
	TestTrue(TEXT("First toy is stored"), Box->Store(Toy));
	Place(FVector(200, 0, 20));
	auto* OtherToy = Cast<ATBCharacter>(Third->GetPawn());
	if (!TestNotNull(TEXT("Other toy exists"), OtherToy)) return false;
	TestTrue(TEXT("Last toy is stored"), Box->Store(OtherToy));
	TestEqual(TEXT("All boxed ends immediately, without an escape grace period"), State->Phase, ETBPhase::Results);
	TestEqual(TEXT("All boxed awards the human victory"), State->Winner, ETBWinner::Humans);
	Place(FVector(200, 0, 121));
	TestEqual(TEXT("Previously armed toy cannot escape after the last capture"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	return true;
}
#endif
