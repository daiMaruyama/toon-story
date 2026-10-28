#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/TBTestMatch.h"
#include "GamePlay/TBHumanEntrance.h"
#include "GamePlay/TBBoxEscapeCourse.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBPickup.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBLobbySettings.h"
#include "Character/TBNPCController.h"
#include "Character/TBCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "Engine/TriggerBox.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBPlayroomStageTest, "ToonStory.Playroom.AuthoredContract",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBPlayroomStageTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
	if (!World || !TestEqual(TEXT("Stylized House is stage ID 3"), TBLobbySettings::StageIndex(World), 3) ||
	    !TestTrue(TEXT("Authored contract runs on the new house map"), World->GetMapName().EndsWith(TEXT("TB_StylizedHouse")))) return false;
	TestEqual(TEXT("Demo ID remains stable"), FString(TBLobbySettings::StagePath(1)), FString(TEXT("/Game/Maps/TB_ToyWorldDemoExpanded")));
	TestEqual(TEXT("Stage ID 3 selects the new house path"), FString(TBLobbySettings::StagePath(3)), FString(TEXT("/Game/Maps/TB_StylizedHouse")));
	int32 Batteries = 0, Boxes = 0, Entrances = 0, Courses = 0;
	ATBBox* Box = nullptr;
	for (TActorIterator<ATBPickup> It(World); It; ++It) ++Batteries;
	for (TActorIterator<ATBBox> It(World); It; ++It) { Box = *It; ++Boxes; }
	TestEqual(TEXT("Eight authored batteries, not an assumed measurement"), Batteries, 8);
	TestEqual(TEXT("Exactly one match box"), Boxes, 1);
	TSet<const APlayerStart*> DoorStarts;
	for (TActorIterator<ATBHumanEntrance> It(World); It; ++It)
	{
		++Entrances; FString Error;
		TestTrue(FString(TEXT("Eight safe entrance slots: ")) + It->GetName(), It->Validate(8, Error));
		if (!Error.IsEmpty()) AddInfo(Error);
		TestNotNull(TEXT("Visual and physical door leaf is configured"), It->DoorLeaf->GetStaticMesh().Get());
		TestEqual(TEXT("House entrance has exactly eight door slots"), It->DoorSlots.Num(), 8);
		for (const auto& Slot : It->DoorSlots)
		{
			if (!TestTrue(TEXT("Door slot is live in the house world"), IsValid(Slot.Get()) && Slot->GetWorld() == World)) continue;
			TestFalse(TEXT("Door slot references are distinct"), DoorStarts.Contains(Slot.Get()));
			DoorStarts.Add(Slot.Get());
		}
	}
	TSet<const APlayerStart*> ToyStarts;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
		if (!DoorStarts.Contains(*It)) ToyStarts.Add(*It);
	TestEqual(TEXT("Eight distinct human door starts"), DoorStarts.Num(), 8);
	TestEqual(TEXT("Eight ordinary toy starts outside the door-start set"), ToyStarts.Num(), 8);
	for (const auto* Start : ToyStarts)
	{
		TestTrue(TEXT("Toy start belongs to the house world"), Start->GetWorld() == World);
		for (const auto* Door : DoorStarts)
			TestFalse(TEXT("Toy and human start positions do not coincide"), Start->GetActorLocation().Equals(Door->GetActorLocation(), 1.f));
	}
	for (TActorIterator<ATBBoxEscapeCourse> It(World); It; ++It)
	{
		++Courses; FString Error;
		TestTrue(TEXT("Course belongs to the match box"), It->Box == Box);
		TestTrue(TEXT("Course/reset/exits have valid supported geometry"), It->Validate(Error));
		if (!Error.IsEmpty()) AddInfo(Error);
		auto CheckMarker = [&](const ATriggerBox* Marker)
		{
			const auto* Volume = IsValid(Marker) ? Cast<UBoxComponent>(Marker->GetCollisionComponent()) : nullptr;
			if (!TestNotNull(TEXT("Course checkpoint/goal has a box volume"), Volume)) return;
			TestFalse(TEXT("Pure checkpoint/goal volume cannot block ToyPawn queries"),
			    Volume->IsQueryCollisionEnabled() && Volume->GetCollisionResponseToChannel(ECC_GameTraceChannel1) == ECR_Block);
		};

		CheckMarker(It->Goal.Get());
	}
	TestEqual(TEXT("Exactly one entrance"), Entrances, 1);
	TestEqual(TEXT("Exactly one registered escape course"), Courses, 1);
	int32 DropAreas = 0;
	for (TActorIterator<AActor> It(World); It; ++It) DropAreas += It->ActorHasTag(ATBBox::DropAreaTag) ? 1 : 0;
	TestEqual(TEXT("TV/storage drop-area tag is unique"), DropAreas, 1);
	// Walking reachability, camera occlusion, timed parkour completion and TV sightlines require separate runtime QA.
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBPlayroomVisionTest, "ToonStory.Playroom.NPCVision",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBPlayroomVisionTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	auto* State = World->GetGameState<ATBGameState>();
	if (!State || State->Phase != ETBPhase::Lobby) return false;
	const auto Hide = State->HidePreparation;
	const auto Phase = State->Phase;
	const double End = State->EndTime;
	TArray<AActor*> Actors;
	ON_SCOPE_EXIT
	{
		State->HidePreparation = Hide; State->Phase = Phase; State->EndTime = End;
		for (auto* Actor : Actors) if (IsValid(Actor)) Actor->Destroy();
	};
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Body = World->SpawnActor<ATBCharacter>(FVector(0, 0, 10000), FRotator::ZeroRotator, Spawn); Actors.Add(Body);
	auto* AI = World->SpawnActor<ATBNPCController>(); Actors.Add(AI);
	auto* Target = World->SpawnActor<ATBCharacter>(FVector(0, 0, 11000), FRotator::ZeroRotator, Spawn); Actors.Add(Target);
	if (!Body || !AI || !Target) return false;
	AI->Possess(Body);
	auto* Info = AI->GetPlayerState<ATBPlayerState>();
	if (!Info) return false;
	Info->Team = ETBTeam::Human; Body->ApplyState();
	Target->GetCapsuleComponent()->SetCapsuleSize(7.5f, 9.75f);
	const FVector Eye = Body->GetPawnViewLocation();
	AI->SetControlRotation(FRotator::ZeroRotator);
	Target->SetActorLocation(Eye + FVector(1800, 0, 400));
	TestTrue(TEXT("Human NPC sees an elevated small toy beyond the old fourteen metres"), AI->CanSee(Target));
	Target->SetActorLocation(Eye + FVector(2500, 0, 400));
	TestFalse(TEXT("Sight range is still bounded"), AI->CanSee(Target));
	Target->SetActorLocation(Eye + FVector(-500, 0, 100));
	TestFalse(TEXT("No sight behind the three-dimensional cone"), AI->CanSee(Target));
	Target->SetActorLocation(Eye + FVector(1800, 0, 400));
	auto* Wall = World->SpawnActor<AActor>(); Actors.Add(Wall);
	auto* Collision = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Collision);
	Collision->SetBoxExtent(FVector(20, 200, 400)); Collision->SetCollisionProfileName(TEXT("BlockAll")); Collision->RegisterComponent();
	Wall->SetActorLocation(Eye + FVector(900, 0, 200));
	TestFalse(TEXT("Wall occlusion still rejects every body sample"), AI->CanSee(Target));
	TBTest::FPlayingScope Playing(*State, State->GetServerWorldTimeSeconds() + 60);
	AI->UpdateControlRotation(0, false);
	const float Scan = AI->GetControlRotation().Pitch;
	Target->SetActorLocation(Eye + FVector(-1000, 1000, 1000));
	AI->UpdateControlRotation(0, false);
	TestTrue(TEXT("Unseen target position cannot change time-driven pitch scan"), FMath::IsNearlyEqual(Scan, AI->GetControlRotation().Pitch, .01f));
	return true;
}
#endif
