#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/TBContactReactiveComponent.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "AIController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBContactReactiveTest, "ToonStory.ContactReactive.BoundedMotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBContactReactiveTest::RunTest(const FString&)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Isolated native physics world"), World)) return false;
	ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); if (World->IsRooted()) World->RemoveFromRoot(); };
	// BeginPlay alone does not initialize actors: UE rejects overlaps with uninitialized actors.
	World->InitializeActorsForPlay(FURL());
	if (!TestTrue(TEXT("Native fixture world initializes actors before spawning participants"), World->AreActorsInitialized())) return false;
	auto* State = World->SpawnActor<ATBGameState>();
	if (!State) return false;
	World->SetGameState(State);
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto MakeCharacter = [&]()
	{
		auto* Body = World->SpawnActor<ATBCharacter>(FVector(-2000, 0, 100), FRotator::ZeroRotator, Spawn);
		auto* Controller = World->SpawnActor<AAIController>();
		auto* Info = World->SpawnActor<ATBPlayerState>();
		if (!Body || !Controller || !Info) return static_cast<ATBCharacter*>(nullptr);
		Controller->SetPlayerState(Info); Info->SetOwner(Controller);
		Controller->Possess(Body); State->AddPlayerState(Info);
		Info->Team = ETBTeam::Human; Info->ToyState = ETBToyState::Free; Info->bFrozen = false;
		Body->ApplyState();
		return Body;
	};
	auto* Body = MakeCharacter();
	auto* OtherBody = MakeCharacter();
	if (!TestNotNull(TEXT("Contact character"), Body) || !TestNotNull(TEXT("Second contact character"), OtherBody)) return false;
	auto MakeProp = [&](FVector Origin, bool Return, bool Once, float Angle, bool Replica = false)
	{
		auto* Owner = World->SpawnActor<AActor>();
		Owner->SetReplicates(true); Owner->SetReplicateMovement(false);
		if (Replica) Owner->SetRole(ROLE_SimulatedProxy);
		auto* Root = NewObject<USceneComponent>(Owner); Owner->SetRootComponent(Root); Root->RegisterComponent();
		Root->SetWorldLocation(Origin);
		auto* Proxy = NewObject<UBoxComponent>(Owner); Owner->AddInstanceComponent(Proxy);
		Proxy->SetupAttachment(Root); Proxy->SetBoxExtent(FVector(45, 3, 90));
		Proxy->SetRelativeLocation(FVector(50, 0, 100)); Proxy->SetMobility(EComponentMobility::Movable);
		Proxy->SetCollisionObjectType(ECC_WorldDynamic); Proxy->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Proxy->SetCollisionResponseToAllChannels(ECR_Block); Proxy->RegisterComponent();
		auto* Sensor = NewObject<UBoxComponent>(Owner); Owner->AddInstanceComponent(Sensor);
		Sensor->SetupAttachment(Root); Sensor->SetBoxExtent(FVector(45, 2, 90)); Sensor->SetRelativeLocation(FVector(45, -5, 100));
		Sensor->SetCollisionObjectType(ECC_WorldDynamic); Sensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Sensor->SetCollisionResponseToAllChannels(ECR_Ignore); Sensor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Sensor->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); Sensor->SetGenerateOverlapEvents(true); Sensor->RegisterComponent();
		auto* Motion = NewObject<UTBContactReactiveComponent>(Owner); Owner->AddInstanceComponent(Motion);
		Motion->MotionTarget.OverrideComponent = Proxy; Motion->ContactTrigger.OverrideComponent = Sensor;
		Motion->LocalPivot = FVector(0, 0, 100); Motion->AngleDegrees = Angle; Motion->MotionSeconds = .1f;
		Motion->bReturnAfterContact = Return; Motion->bOncePerRound = Once; Motion->RegisterComponent();
		if (!Owner->HasActorBegunPlay()) Owner->DispatchBeginPlay();
		return Motion;
	};
	auto* Motion = MakeProp(FVector::ZeroVector, false, true, 20);
	World->GetWorldSettings()->NotifyBeginPlay();
	if (!TestTrue(TEXT("Valid native proxy binds"), Motion->bReady)) { AddInfo(Motion->GetMotionProblem()); return false; }
	auto LogContactGeometry = [&](const TCHAR* Label)
	{
		const auto* Capsule = Body->GetCapsuleComponent();
		const auto* Sensor = Motion->Trigger.Get();
		AddInfo(FString::Printf(TEXT("%s world initialized=%d begun=%d; character initialized=%d begun=%d; prop initialized=%d begun=%d"),
		    Label, World->AreActorsInitialized(), World->HasBegunPlay(), Body->IsActorInitialized(), Body->HasActorBegunPlay(),
		    Motion->GetOwner()->IsActorInitialized(), Motion->GetOwner()->HasActorBegunPlay()));
		for (const auto* Part : {static_cast<const UPrimitiveComponent*>(Capsule), Sensor})
			AddInfo(FString::Printf(TEXT("%s component=%s registered=%d physics=%d query=%d events=%d object=%d Pawn=%d ToyPawn=%d WorldDynamic=%d profile=%s pos=%s bounds=%s"),
			    Label, *Part->GetName(), Part->IsRegistered(), Part->IsPhysicsStateCreated(), Part->IsQueryCollisionEnabled(), Part->GetGenerateOverlapEvents(),
			    int32(Part->GetCollisionObjectType()), int32(Part->GetCollisionResponseToChannel(ECC_Pawn)), int32(Part->GetCollisionResponseToChannel(ECC_GameTraceChannel1)),
			    int32(Part->GetCollisionResponseToChannel(ECC_WorldDynamic)), *Part->GetCollisionProfileName().ToString(),
			    *Part->GetComponentLocation().ToString(), *Part->Bounds.GetBox().ToString()));
		TArray<FOverlapResult> Raw;
		World->OverlapMultiByObjectType(Raw, Sensor->GetComponentLocation(), Sensor->GetComponentQuat(),
		    FCollisionObjectQueryParams(Capsule->GetCollisionObjectType()),
		    FCollisionShape::MakeBox(CastChecked<UBoxComponent>(Sensor)->GetScaledBoxExtent()));
		bool FoundCapsule = false;
		for (const auto& Result : Raw) FoundCapsule |= Result.GetComponent() == Capsule;
		AddInfo(FString::Printf(TEXT("%s raw shape query hits=%d actual capsule hit=%d; capsule radius=%.3f half=%.3f; sensor extent=%s"),
		    Label, Raw.Num(), FoundCapsule, Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(),
		    *CastChecked<UBoxComponent>(Sensor)->GetScaledBoxExtent().ToString()));
	};
	auto Move = [&](ATBCharacter* Character, FVector Position)
	{
		const bool Moved = Character->SetActorLocation(Position);
		Character->GetCapsuleComponent()->UpdateOverlaps();
		Motion->Trigger->UpdateOverlaps();
		return TestTrue(TEXT("Fixture character reaches its requested position"), Moved && Character->GetActorLocation().Equals(Position, .01f));
	};
	State->Phase = ETBPhase::Playing; State->HidePreparation.bValid = true; State->HidePreparation.bHiding = true; State->HidePreparation.Round = 1;
	Motion->AdvanceServer(0, 0);
	Move(Body, FVector(45, -30, 100));
	LogContactGeometry(TEXT("Initial actual contact"));
	if (!TestTrue(TEXT("Both overlap actors have completed initialization and BeginPlay"),
	    Body->IsActorInitialized() && Body->HasActorBegunPlay() && Motion->GetOwner()->IsActorInitialized() && Motion->GetOwner()->HasActorBegunPlay()) ||
	    !TestTrue(TEXT("Both overlap components have registered query physics and event generation"),
	        Body->GetCapsuleComponent()->IsRegistered() && Motion->Trigger->IsRegistered() &&
	        Body->GetCapsuleComponent()->IsPhysicsStateCreated() && Motion->Trigger->IsPhysicsStateCreated() &&
	        Body->GetCapsuleComponent()->IsQueryCollisionEnabled() && Motion->Trigger->IsQueryCollisionEnabled() &&
	        Body->GetCapsuleComponent()->GetGenerateOverlapEvents() && Motion->Trigger->GetGenerateOverlapEvents())) return false;
	TestTrue(TEXT("Actual sensor and character capsule overlap"), Motion->Trigger->IsOverlappingComponent(Body->GetCapsuleComponent()));
	TestEqual(TEXT("Preparation contact does not start motion"), Motion->Snapshot.Angle, 0.f);
	State->HidePreparation.bHiding = false;
	Motion->AdvanceServer(.1f, .1);
	TestEqual(TEXT("Release while still touching does not auto-start"), Motion->Snapshot.Status, ETBContactMotionStatus::Idle);
	Move(Body, FVector(-2000, 0, 100));
	const uint32 BeforeApproach = Motion->Snapshot.Generation;
	auto* WideSensor = CastChecked<UBoxComponent>(Motion->Trigger);
	WideSensor->SetBoxExtent(FVector(45, 300, 90));
	Move(Body, FVector(45, -200, 100));
	LogContactGeometry(TEXT("Wide sensor contact"));
	TestTrue(TEXT("Oversized sensor really overlaps the distant character"), Motion->Trigger->IsOverlappingComponent(Body->GetCapsuleComponent()));
	TestEqual(TEXT("Distant proximity is not surface contact"), Motion->Snapshot.Generation, BeforeApproach);
	Move(Body, FVector(-2000, 0, 100)); WideSensor->SetBoxExtent(FVector(45, 2, 90));
	const uint32 BeforeContact = Motion->Snapshot.Generation;
	Move(Body, FVector(45, -30, 100));
	if (!TestTrue(TEXT("Real capsule overlap starts authority reaction"), Motion->Snapshot.Generation == BeforeContact + 1 &&
	    Motion->Snapshot.Status == ETBContactMotionStatus::Moving)) return false;
	FHitResult Hit;
	Motion->ContactBegan(Motion->Trigger, nullptr, nullptr, 0, false, Hit);
	Motion->ContactBegan(Motion->Trigger, Motion->GetOwner(), Motion->Target, 0, false, Hit);
	Motion->ContactBegan(Motion->Trigger, Body, Body->GetMesh(), 0, false, Hit);
	Motion->ContactBegan(Motion->Trigger, Body, Body->GetCapsuleComponent(), 0, false, Hit);
	TestEqual(TEXT("Extra component and duplicate event never count a second contact"), Motion->Snapshot.Generation, BeforeContact + 1);
	// The touching character is not excluded from clearance, even if Pawn responses ignore it.
	Move(Body, FVector(50, 0, 100));
	Motion->AdvanceServer(.1f, .2);
	TestEqual(TEXT("Contacting pawn inside the solid blocks movement"), Motion->Snapshot.Status, ETBContactMotionStatus::Blocked);
	TestEqual(TEXT("Blocked contact retains its last approved angle"), Motion->Snapshot.Angle, 0.f);
	Move(Body, FVector(-2000, 0, 100)); Move(OtherBody, FVector(-2200, 0, 100));
	for (int32 Step = 0; Step < 30; ++Step) Motion->AdvanceServer(.025f, .3 + Step * .025);
	TestTrue(TEXT("Unoccupied bounded motion reaches its configured end"), FMath::IsNearlyEqual(Motion->Snapshot.Angle, 20.f) &&
	    Motion->Snapshot.Status == ETBContactMotionStatus::Held);
	const FTransform HeldPose = Motion->Snapshot.Pose;
	Move(Body, Motion->Target->GetComponentLocation());
	State->Phase = ETBPhase::Results; State->HidePreparation = FTBHidePreparation();
	Motion->AdvanceServer(.1f, 1.5);
	TestEqual(TEXT("Occupied round reset is pending, not completed"), Motion->Snapshot.Status, ETBContactMotionStatus::ResetPending);
	TestTrue(TEXT("Occupied reset never snaps through the character"), Motion->Snapshot.Pose.Equals(HeldPose));
	Motion->AdvanceServer(.1f, 7);
	TestEqual(TEXT("Reset deadline gives finite disabled status"), Motion->Snapshot.Status, ETBContactMotionStatus::Disabled);
	TestTrue(TEXT("Timeout retains approved safe pose"), Motion->Snapshot.Pose.Equals(HeldPose));
	Move(Body, FVector(-2000, 0, 100));
	State->Phase = ETBPhase::Playing; State->HidePreparation.bValid = true; State->HidePreparation.Round = 2;
	for (int32 Step = 0; Step < 30; ++Step) Motion->AdvanceServer(.025f, 8 + Step * .025);
	TestTrue(TEXT("New round restores safely before accepting new contact"), Motion->Snapshot.Status == ETBContactMotionStatus::Idle && FMath::IsNearlyZero(Motion->Snapshot.Angle));
	Motion->AdvanceServer(10000, 9);
	TestEqual(TEXT("No contact never causes an automatic performance"), Motion->Snapshot.Angle, 0.f);
	// Repeatable contact episodes: all actors must leave, and cooldown is checked at the next physical entry.
	auto* Clock = MakeProp(FVector(5000, 0, 0), true, false, 5);
	Clock->AdvanceServer(0, 0);
	const uint32 BeforeClockContact = Clock->Snapshot.Generation;
	Move(Body, FVector(5045, -30, 100)); Clock->Trigger->UpdateOverlaps();
	if (!TestTrue(TEXT("Repeatable prop also starts from real capsule contact"), Clock->Snapshot.Generation == BeforeClockContact + 1 &&
	    Clock->Snapshot.Status == ETBContactMotionStatus::Moving)) return false;
	const uint32 ClockGeneration = Clock->Snapshot.Generation;
	Move(OtherBody, FVector(5045, -30, 100)); Clock->Trigger->UpdateOverlaps();
	Move(Body, FVector(-2000, 0, 100)); Move(Body, FVector(5045, -30, 100)); Clock->Trigger->UpdateOverlaps();
	TestEqual(TEXT("One actor leaving while another stays does not rearm"), Clock->Snapshot.Generation, ClockGeneration);
	Move(Body, FVector(-2000, 0, 100)); Move(OtherBody, FVector(-2200, 0, 100)); Clock->Trigger->UpdateOverlaps();
	for (int32 Step = 0; Step < 20; ++Step) Clock->AdvanceServer(.025f, .1 + Step * .025);
	TestTrue(TEXT("Contact-started return completes at home"), Clock->Snapshot.Status == ETBContactMotionStatus::Idle && FMath::IsNearlyZero(Clock->Snapshot.Angle));
	Clock->NextContactTime = World->GetTimeSeconds() + 1; // Clock boundary injection, not a claim of an actual one-second wait.
	Move(Body, FVector(5045, -30, 100)); Clock->Trigger->UpdateOverlaps();
	TestEqual(TEXT("Fresh physical entry during cooldown is refused"), Clock->Snapshot.Generation, ClockGeneration);
	Move(Body, FVector(-2000, 0, 100)); Clock->Trigger->UpdateOverlaps();
	Clock->NextContactTime = World->GetTimeSeconds() - .01;
	Move(Body, FVector(5045, -30, 100)); Clock->Trigger->UpdateOverlaps();
	TestEqual(TEXT("All exits plus elapsed cooldown allow a new contact"), Clock->Snapshot.Generation, ClockGeneration + 1);
	const auto AuthoritySnapshot = Clock->Snapshot;
	Clock->GetOwner()->SetRole(ROLE_SimulatedProxy);
	Clock->ContactBegan(Clock->Trigger, Body, Body->GetCapsuleComponent(), 0, false, Hit);
	Clock->AdvanceServer(.25f, .7);
	TestEqual(TEXT("Non-authority cannot change reaction generation"), Clock->Snapshot.Generation, AuthoritySnapshot.Generation);
	TestEqual(TEXT("Non-authority cannot advance pose"), Clock->Snapshot.Angle, AuthoritySnapshot.Angle);
	Clock->GetOwner()->SetRole(ROLE_Authority);
	Move(Body, FVector(-2000, 0, 100));
	// A very offset pivot makes a middle-arc obstacle distinguishable from both endpoint poses.
	auto* Arc = MakeProp(FVector(10000, 0, 0), false, true, -60);
	Arc->Target->SetBoxExtent(FVector(1)); Arc->Target->SetRelativeLocation(FVector(2000, 0, 100));
	Arc->ExtentAtRest = FVector(1); Arc->Snapshot.Baseline = Arc->Snapshot.Pose = Arc->Target->GetRelativeTransform();
	Arc->AdvanceServer(0, 0);
	const FTransform Middle = Arc->PoseAt(.25f) * Arc->ParentAtRest;
	auto* Wall = World->SpawnActor<AActor>();
	auto* Obstacle = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Obstacle);
	Obstacle->SetBoxExtent(FVector(.5f)); Obstacle->SetCollisionProfileName(TEXT("BlockAll")); Obstacle->RegisterComponent(); Wall->SetActorLocation(Middle.GetLocation());
	TestTrue(TEXT("First endpoint alone is clear"), Arc->SegmentClear(0, 0));
	TestTrue(TEXT("Second endpoint alone is clear"), Arc->SegmentClear(.5f, .5f));
	TestFalse(TEXT("Whole angular interval catches its intermediate obstacle"), Arc->SegmentClear(0, .5f));
	TestTrue(TEXT("Negative rotation interval remains bounded and independently clear"), Arc->SegmentClear(0, -.5f));
	TestTrue(TEXT("Offset pivot and negative ninety-degree pose preserve the expected radius"),
	    Arc->PoseAt(-90).GetLocation().Equals(FVector(0, -2000, 100), .01f));
	Obstacle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Arc->GoalAngle = -60; Arc->Snapshot.Status = ETBContactMotionStatus::Moving; Arc->Deadline = 100;
	Arc->AdvanceServer(10000, 1);
	TestTrue(TEXT("Huge DeltaTime never exceeds eight half-degree approved substeps"), Arc->Snapshot.Angle < 0 && FMath::Abs(Arc->Snapshot.Angle) <= 4.001f);
	FString Error;
	Arc->FixedParent->SetRelativeScale3D(FVector(2, 1, 1));
	TestFalse(TEXT("Nonuniform parent scale is rejected"), Arc->ValidateSetup(Error));
	Arc->AdvanceServer(.1f, 2);
	TestEqual(TEXT("Runtime parent change disables motion without a forced reset"), Arc->Snapshot.Status, ETBContactMotionStatus::Disabled);
	// Snapshot application emulates a late recipient; actual network delivery is separate integration QA.
	Motion->NextContactTime = World->GetTimeSeconds() - .01; // Align the injected server clock boundary with native overlap time.
	Move(Body, FVector(45, -30, 100));
	Move(Body, FVector(-2000, 0, 100));
	for (int32 Step = 0; Step < 30; ++Step) Motion->AdvanceServer(.025f, 1 + Step * .025);
	if (!TestTrue(TEXT("Late-join source has a nonzero approved pose"), Motion->Snapshot.Status == ETBContactMotionStatus::Held &&
	    FMath::IsNearlyEqual(Motion->Snapshot.Angle, 20.f))) return false;
	auto* Replica = MakeProp(FVector::ZeroVector, false, true, 20, true);
	Replica->Snapshot = Motion->Snapshot;
	Replica->OnRep_Snapshot();
	TestTrue(TEXT("Late snapshot restores the approved pose and baseline"), Replica->Target->GetRelativeTransform().Equals(Motion->Snapshot.Pose) &&
	    Replica->Snapshot.Baseline.Equals(Motion->Snapshot.Baseline));
	const auto ReplicaPose = Replica->Target->GetRelativeTransform();
	Replica->TickComponent(10000, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Client never predicts beyond the server-approved pose"), Replica->Target->GetRelativeTransform().Equals(ReplicaPose));
	const uint32 BeforeEnd = Motion->Snapshot.Generation;
	Motion->EndPlay(EEndPlayReason::RemovedFromWorld);
	Motion->ContactBegan(Motion->Trigger, Body, Body->GetCapsuleComponent(), 0, false, Hit);
	TestTrue(TEXT("EndPlay clears contacts and prevents further reaction"), Motion->Contacts.IsEmpty() && !Motion->bReady && Motion->Snapshot.Generation == BeforeEnd);
	return !HasAnyErrors();
}
#endif
