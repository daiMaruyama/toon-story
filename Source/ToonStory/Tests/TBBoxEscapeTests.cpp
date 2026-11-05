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
#include "Engine/Level.h"
#include "Engine/GameInstance.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/Package.h"
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
		if (IsValid(World) && IsValid(State) && IsValid(Mode) && IsValid(Host))
		{
			if (State->Phase == ETBPhase::Playing) Mode->Finish(ETBWinner::None, TEXT("Escape fixture cleanup"), true);
			if (State->Phase == ETBPhase::Results || State->Phase == ETBPhase::Aborted) Mode->ReturnToLobby(Host);
		}
		if (IsValid(World) && IsValid(Guest)) UGameplayStatics::RemovePlayer(Guest, true);
		if (IsValid(World) && IsValid(Third)) UGameplayStatics::RemovePlayer(Third, true);
		for (auto* Actor : Actors) if (IsValid(Actor)) Actor->Destroy();
		if (IsValid(State)) State->Settings = OriginalSettings;
		if (IsValid(Host))
			if (auto* Info = Host->GetPlayerState<ATBPlayerState>(); IsValid(Info)) Info->Preference = Preference;
		if (IsValid(World) && IsValid(Mode) && IsValid(State)) Mode->SyncLobbyCounts();
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
	Course->Goal->GetCollisionComponent()->SetMobility(EComponentMobility::Movable);
	Course->Goal->SetActorLocation(Room.TransformPosition(FVector(400, 100, 132)));
	Course->Goal->GetCollisionComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CastChecked<UBoxComponent>(Course->Goal->GetCollisionComponent())->SetBoxExtent(FVector(14, 14, 15));
	Course->FinalSupportTag = TEXT("TBFixtureFinalLanding");
	Course->FinalSupportLevelPackage = FName(*UWorld::RemovePIEPrefix(World->PersistentLevel->GetOutermost()->GetName()));
	auto MakeSupport = [&]()
	{
		auto* Owner = World->SpawnActor<AActor>(); Actors.Add(Owner);
		auto* Surface = NewObject<UBoxComponent>(Owner); Owner->SetRootComponent(Surface);
		Surface->SetBoxExtent(FVector(14, 14, 4)); Surface->SetCollisionProfileName(TEXT("BlockAll"));
		Surface->ComponentTags.Add(Course->FinalSupportTag); Surface->RegisterComponent();
		Owner->SetActorLocation(Room.TransformPosition(FVector(400, 100, 116)));
		return Surface;
	};
	auto* SupportA = MakeSupport();
	auto* SupportB = MakeSupport();
	SupportB->GetOwner()->SetActorLocation(Room.TransformPosition(FVector(400, 100, 115.6)));
	auto* Exit = World->SpawnActor<ATargetPoint>(); Actors.Add(Exit);
	Exit->GetRootComponent()->SetMobility(EComponentMobility::Movable);
	Exit->SetActorLocation(FVector(-1700, -1700, 12)); Course->ExitPoints = {Exit};
	FString Error;
	if (!TestTrue(TEXT("Explicit final landing validates with two intended overlapping support components"), Course->Validate(Error)))
	{ AddInfo(Error); return false; }
	SupportB->ComponentTags.Remove(Course->FinalSupportTag);
	TestFalse(TEXT("Missing final support tag is rejected"), Course->Validate(Error));
	SupportB->ComponentTags.Add(Course->FinalSupportTag);
	auto* Unexpected = MakeSupport();
	TestFalse(TEXT("Unexpected third tagged component is rejected"), Course->Validate(Error));
	Unexpected->GetOwner()->Destroy();
	const FName SupportLevel = Course->FinalSupportLevelPackage;
	Course->FinalSupportLevelPackage = TEXT("/Game/Maps/NotTheSharedRoom");
	TestFalse(TEXT("Same-world tags in an unrelated level are not accepted"), Course->Validate(Error));
	Course->FinalSupportLevelPackage = SupportLevel;
	Course->Goal->SetActorLocation(Room.TransformPosition(FVector(700, 100, 132)));
	TestFalse(TEXT("Old remote XY marker cannot serve as a final landing"), Course->Validate(Error));
	Course->Goal->SetActorLocation(Room.TransformPosition(FVector(400, 100, 132)));
	TestTrue(TEXT("Restored explicit landing contract remains valid"), Course->Validate(Error));
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
	auto Observe = [&](const FString& Stage)
	{
		AddInfo(FString::Printf(TEXT("%s phase=%d reason='%s' round=%u valid=%d hiding=%d preparing=%d end=%.3f now=%.3f players=%d"),
		    *Stage, int32(State->Phase), *State->Reason, State->HidePreparation.Round, State->HidePreparation.bValid,
		    State->HidePreparation.bHiding, State->IsPreparing(), double(State->EndTime),
		    double(State->GetServerWorldTimeSeconds()), State->PlayerArray.Num()));
		AddInfo(FString::Printf(TEXT("%s rules human=%d toy=%d npcHuman=%d npcToy=%d duration=%d required=%d collected=%d boxed=%d winner=%d"),
		    *Stage, State->Settings.Humans, State->Settings.Toys, State->Settings.NPCHumans, State->Settings.NPCToys,
		    int32(State->Settings.Duration), State->Settings.RequiredItems, State->Collected, State->BoxedCount, int32(State->Winner)));
		for (auto* PC : {Host, Guest, Third})
		{
			const auto* Info = IsValid(PC) ? PC->GetPlayerState<ATBPlayerState>() : nullptr;
			const auto* Body = IsValid(PC) ? PC->GetPawn<ATBCharacter>() : nullptr;
			AddInfo(FString::Printf(TEXT("%s pc=%s ps=%s pawn=%s team=%d toyState=%d ready=%d possess=%d psMatch=%d"),
			    *Stage, *GetNameSafe(PC), *GetNameSafe(Info), *GetNameSafe(Body), Info ? int32(Info->Team) : -1,
			    Info ? int32(Info->ToyState) : -1, Info && Info->bReady,
			    Body && Body->GetController() == PC, Body && Body->TBPS() == Info));
		}
	};
	auto ParticipantsReady = [&](const FString& Stage, bool InRound)
	{
		bool Valid = TestEqual(Stage + TEXT(" has three participants"), State->PlayerArray.Num(), 3);
		for (auto* PC : {Host, Guest, Third})
		{
			auto* Info = IsValid(PC) ? PC->GetPlayerState<ATBPlayerState>() : nullptr;
			auto* Body = IsValid(PC) ? PC->GetPawn<ATBCharacter>() : nullptr;
			Valid &= TestTrue(Stage + TEXT(" has current possessed pawn/PS"), IsValid(Info) && IsValid(Body) &&
			    Body->GetController() == PC && Body->TBPS() == Info && State->PlayerArray.Contains(Info));
			if (!IsValid(Info)) continue;
			const ETBTeam Expected = InRound ? (PC == Host ? ETBTeam::Human : ETBTeam::Toy) : ETBTeam::None;
			Valid &= TestEqual(Stage + TEXT(" team ") + GetNameSafe(PC), Info->Team, Expected);
			Valid &= TestEqual(Stage + TEXT(" free state ") + GetNameSafe(PC), Info->ToyState, ETBToyState::Free);
		}
		return Valid;
	};
	auto Begin = [&](const TCHAR* Case) -> ATBCharacter*
	{
		const FString Stage(Case);
		const uint32 PreviousRound = State->HidePreparation.Round;
		Observe(Stage + TEXT(" before cleanup"));
		if (State->Phase == ETBPhase::Playing) Mode->Finish(ETBWinner::None, TEXT("Landing fixture next"), true);
		if (State->Phase == ETBPhase::Results || State->Phase == ETBPhase::Aborted) Mode->ReturnToLobby(Host);
		Observe(Stage + TEXT(" lobby"));
		if (!TestEqual(Stage + TEXT(" returns to Lobby"), State->Phase, ETBPhase::Lobby) ||
		    !ParticipantsReady(Stage + TEXT(" lobby"), false)) return nullptr;
		Host->GetPlayerState<ATBPlayerState>()->bReady = Guest->GetPlayerState<ATBPlayerState>()->bReady = Third->GetPlayerState<ATBPlayerState>()->bReady = true;
		// This synchronous fixture starts several rounds without any lobby movement.
		// Vacate the arena start markers so a previous participant cannot block a
		// different participant while the next round teleports them in sequence.
		int32 StagingIndex = 0;
		for (auto* PC : {Host, Guest, Third})
			PC->GetPawn()->SetActorLocation(FVector(-1000 + 200 * StagingIndex++, -1000, 120), false, nullptr, ETeleportType::TeleportPhysics);
		Mode->StartRound(Host);
		Observe(Stage + TEXT(" started"));
		if (!TestTrue(Stage + TEXT(" commits a new preparing round"), State->Phase == ETBPhase::Playing &&
		    State->HidePreparation.bValid && State->HidePreparation.bHiding && State->IsPreparing() &&
		    State->HidePreparation.Round != PreviousRound && State->EndTime == 0) ||
		    !ParticipantsReady(Stage + TEXT(" started"), true)) return nullptr;
		Mode->AdvancePreparationClockForTest(0);
		Observe(Stage + TEXT(" released"));
		if (!TestTrue(Stage + TEXT(" releases through server preparation path"), State->Phase == ETBPhase::Playing &&
		    State->HidePreparation.bValid && !State->IsPreparing() && FMath::IsFinite(State->EndTime) &&
		    State->EndTime > State->GetServerWorldTimeSeconds()) ||
		    !ParticipantsReady(Stage + TEXT(" released"), true)) return nullptr;
		return Cast<ATBCharacter>(Guest->GetPawn());
	};
	auto Store = [&](const TCHAR* Label, ATBCharacter* Body)
	{
		Observe(FString(Label) + TEXT(" before Store"));
		if (!TestTrue(FString(Label) + TEXT(" has a free current toy in released Playing"),
		    IsValid(Box) && Box == State->Box && Box->HasAuthority() && IsValid(Body) && IsValid(Body->TBPS()) &&
		    Body->GetController() && Body->GetController()->GetPawn() == Body && Body->TBPS()->Team == ETBTeam::Toy &&
		    Body->TBPS()->ToyState == ETBToyState::Free && State->Phase == ETBPhase::Playing && !State->IsPreparing())) return false;
		const bool Stored = Box->Store(Body); // One attempt; diagnostic queries below cannot retry the operation.
		if (!Stored)
		{
			Observe(FString(Label) + TEXT(" rejected"));
			const auto* Capsule = Body->GetCapsuleComponent();
			FCollisionQueryParams Query(SCENE_QUERY_STAT(TBEscapeStoreDiagnostic), false, Body);
			int32 ClearSlots = 0;
			for (int32 Slot = 0; Slot < ATBBox::StorageSlotCount; ++Slot)
				ClearSlots += !World->OverlapBlockingTestByChannel(Box->StorageRoom->GetComponentTransform().TransformPosition(ATBBox::StorageSlot(Slot)),
				    FQuat::Identity, ECC_GameTraceChannel1, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query);
			AddInfo(FString::Printf(TEXT("Store diagnostic only: radius=%.3f half=%.3f clearFallbackSlots=%d/%d dropArea=%s; private drop selection was not observed"),
			    Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), ClearSlots, ATBBox::StorageSlotCount,
			    *GetNameSafe(ATBBox::FindDropArea(World))));
		}
		return TestTrue(Label, Stored);
	};
	auto* Toy = Begin(TEXT("Initial landing"));
	if (!Toy || State->Phase != ETBPhase::Playing || State->IsPreparing()) return false;
	auto Place = [&](FVector Local, EMovementMode MovementMode = MOVE_Walking, bool Evaluate = true)
	{
		const FVector Position = Room.TransformPosition(Local);
		const bool Moved = Toy->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("Fixture relocation reaches the exact requested position"), Moved && Toy->GetActorLocation().Equals(Position, .01));
		auto* Movement = Toy->GetCharacterMovement();
		Movement->SetMovementMode(MovementMode); Movement->Velocity = FVector::ZeroVector;
		Movement->FindFloor(Toy->GetActorLocation(), Movement->CurrentFloor, false);
		if (Evaluate) Course->Tick(0);
	};
	// The fixture relocates the pawn, sets Walking/Falling, then obtains real physics floor hits.
	// This is not an input-driven climb or proof that the authored map has been migrated.
	const double LandingZ = 120 + Toy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() +
	    (UCharacterMovementComponent::MIN_FLOOR_DIST + UCharacterMovementComponent::MAX_FLOOR_DIST) * .5;
	const FVector Landing(400, 100, LandingZ);
	if (!Store(TEXT("Real capture stores the toy"), Toy)) return false;
	TestEqual(TEXT("One toy outside keeps the round playing"), State->Phase, ETBPhase::Playing);
	const uint32 FirstCapture = Box->CaptureGeneration(Toy);
	Place(Landing, MOVE_Falling);
	TestEqual(TEXT("High storage drop cannot immediately release a fresh capture"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(Landing);
	TestEqual(TEXT("Even a first landing must have descended below the goal this capture"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(FVector(200, 0, 160));
	TestEqual(TEXT("Height elsewhere in the room cannot release the toy"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(FVector(200, 0, 20));
	TestTrue(TEXT("A fall stays at the actual position, without teleporting to a start"),
	    Toy->GetActorLocation().Equals(Room.TransformPosition(FVector(200, 0, 20)), .01f));
	Place(FVector(200, 0, -100));
	TestTrue(TEXT("Even falling below the course does not reset or abort"),
	    Toy->GetActorLocation().Equals(Room.TransformPosition(FVector(200, 0, -100)), .01f) && State->Phase == ETBPhase::Playing);
	Place(FVector(2000, 0, 220));
	TestEqual(TEXT("Height outside the box cannot escape"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(FVector(400, 100, 20));
	TestEqual(TEXT("Below the finish height cannot escape"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(Landing + FVector(0, 0, -10), MOVE_Falling);
	TestEqual(TEXT("Feet below the finish height cannot escape"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Place(FVector(200, 0, 160), MOVE_Falling);
	TestEqual(TEXT("Reaching finish height anywhere in the course escapes while airborne"), Toy->TBPS()->ToyState, ETBToyState::Free);
	TestTrue(TEXT("Escape reaches the safe exit"), Toy->GetActorLocation().Equals(ExitLocation, 1.f));
	if (!Store(TEXT("Recapture succeeds"), Toy)) return false;
	Place(Landing);
	TestEqual(TEXT("Recapture must descend again; previous arming is not reused"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	TestFalse(TEXT("Old capture token cannot release a new capture"), Box->ReleasePrisoner(Toy, FirstCapture, {Exit->GetActorTransform()}));
	Place(FVector(200, 0, 20));
	SupportA->GetOwner()->SetActorLocation(Room.TransformPosition(FVector(400, 100, 115.6)));
	SupportB->GetOwner()->SetActorLocation(Room.TransformPosition(FVector(400, 100, 116)));
	auto* Blocker = World->SpawnActor<AActor>(); Actors.Add(Blocker);
	auto* Collision = NewObject<UBoxComponent>(Blocker); Blocker->SetRootComponent(Collision);
	Collision->SetBoxExtent(FVector(100)); Collision->SetCollisionProfileName(TEXT("BlockAll")); Collision->RegisterComponent();
	Blocker->SetActorLocation(ExitLocation);
	Place(Landing, MOVE_Walking, false);
	TestTrue(TEXT("Second actual walking floor resolves the other allowed support"), Toy->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent() == SupportB);
	Course->Tick(0);
	TestEqual(TEXT("A blocked exit keeps the toy boxed"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Course->Tick(0);
	TestEqual(TEXT("An unblocked exit releases on the next supported landing check"), Toy->TBPS()->ToyState, ETBToyState::Free);
	if (!Store(TEXT("Another capture succeeds"), Toy)) return false;
	Place(FVector(200, 0, 20));
	State->EndTime = State->GetServerWorldTimeSeconds() - .01;
	Mode->CheckWin(); // The game mode ticks before the course in the real world.
	Place(Landing);
	TestEqual(TEXT("Deadline takes precedence over final landing escape"), State->Winner, ETBWinner::Humans);
	TestEqual(TEXT("Expired round cannot release the toy"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Toy = Begin(TEXT("Tokenless boxed rejection"));
	if (!Toy) return false;
	Toy->SetToyState(ETBToyState::Boxed);
	Place(FVector(200, 0, 20)); Place(Landing);
	TestEqual(TEXT("Boxed state without a real capture cannot authorize escape"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	Toy = Begin(TEXT("All boxed regression"));
	if (!Toy) return false;
	if (!Store(TEXT("First toy is stored"), Toy)) return false;
	Place(FVector(200, 0, 20));
	auto* OtherToy = Cast<ATBCharacter>(Third->GetPawn());
	if (!TestNotNull(TEXT("Other toy exists"), OtherToy)) return false;
	if (!Store(TEXT("Last toy is stored"), OtherToy)) return false;
	TestEqual(TEXT("All boxed ends immediately, without an escape grace period"), State->Phase, ETBPhase::Results);
	TestEqual(TEXT("All boxed awards the human victory"), State->Winner, ETBWinner::Humans);
	Place(Landing);
	TestEqual(TEXT("Previously armed toy cannot escape after the last capture"), Toy->TBPS()->ToyState, ETBToyState::Boxed);
	return true;
}
#endif
