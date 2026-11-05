// Copyright (c) 2026 Mamodou. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Components/StaticMeshComponent.h"
#include "Core/TBGameState.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GamePlay/TBPushableProps.h"
#include "Tests/AutomationCommon.h"

namespace
{
	struct FExpandedPropProbe
	{
		TWeakObjectPtr<UStaticMeshComponent> Part;
		TWeakObjectPtr<AStaticMeshActor> Floor;
		FTransform Original;
		FVector Velocity, Spin;
		bool bAwake = false;
		double Started = 0;
		~FExpandedPropProbe()
		{
			if (auto* P = Part.Get())
			{
				P->SetWorldTransform(Original, false, nullptr, ETeleportType::TeleportPhysics);
				P->SetPhysicsLinearVelocity(Velocity);
				P->SetPhysicsAngularVelocityInRadians(Spin);
				if (!bAwake) P->PutRigidBodyToSleep();
			}
			if (Floor.IsValid()) Floor->Destroy();
		}
	};
}

// Also run in a listen server and two clients on the same stage to check actual replicated physics.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBExpandedPropsTest, "ToonStory.Playroom.ExpandedProps",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBExpandedPropsTest::RunTest(const FString& Parameters)
{
	if (Parameters != TEXT("Ready"))
	{
		const double ReadyDeadline = FPlatformTime::Seconds() + 60;
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, ReadyDeadline]()
		{
			for (const auto& C : GEngine->GetWorldContexts())
				if (C.WorldType == EWorldType::Game && C.World())
					if (auto* S = C.World()->GetGameState<ATBGameState>(); S && S->HasActorBegunPlay())
					{
						RunTest(TEXT("Ready"));
						return true;
					}
			if (FPlatformTime::Seconds() < ReadyDeadline) return false;
			AddError(TEXT("Game state BeginPlay did not arrive"));
			return true;
		}));
		return true;
	}
	UWorld* World = nullptr;
	for (const auto& C : GEngine->GetWorldContexts())
		if (C.WorldType == EWorldType::Game) World = C.World();
	if (!TestNotNull(TEXT("Game world"), World)) return false;
	const bool bHouse = World->GetMapName().EndsWith(TEXT("TB_StylizedHouse"));
	if (!TestTrue(TEXT("Run on either playroom"), bHouse || World->GetMapName().EndsWith(TEXT("TB_ToyWorldDemoExpanded")))) return false;
	int32 Count = 0;
	TArray<AStaticMeshActor*> Candidates;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		auto* P = It->GetStaticMeshComponent();
		if (!P->ComponentHasTag(TEXT("TBRuntimePushable"))) continue;
		++Count;
		TestTrue(TEXT("New prop has a live physics body"), P->IsSimulatingPhysics() && P->IsGravityEnabled());
		TestTrue(TEXT("New prop remains pushable and cannot become a character floor"),
			P->ComponentHasTag(TEXT("TBPushable")) && P->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore &&
			P->GetCollisionResponseToChannel(ECC_GameTraceChannel1) == ECR_Ignore && P->CanCharacterStepUpOn == ECB_No);
		TestFalse(TEXT("Repeated configuration does not reset a moving prop"), TBPushableProps::TryEnable(**It));
		if (World->GetNetMode() != NM_Client)
			TestTrue(TEXT("Authority replicates prop movement"), It->GetIsReplicated() && It->IsReplicatingMovement());
		if (P->GetStaticMesh()->GetName() == (bHouse ? TEXT("SM_FoodCan01") : TEXT("SM_Battery_01"))) Candidates.Add(*It);
	}
	AddInfo(FString::Printf(TEXT("Expanded props in %s: %d"), *World->GetMapName(), Count));
	TestEqual(TEXT("All reviewed stage instances receive the same setup on every peer"), Count, bHouse ? 56 : 52);
	if (!TestTrue(TEXT("Deterministic physics probe exists"), !Candidates.IsEmpty())) return false;
	Candidates.Sort([](const AStaticMeshActor& A, const AStaticMeshActor& B) { return A.GetName() < B.GetName(); });
	auto* Part = Candidates[0]->GetStaticMeshComponent();
	// Reproduce a physics replication update arriving before local prop configuration.
	auto* EarlyPhysics = World->SpawnActor<AStaticMeshActor>();
	auto* EarlyPart = EarlyPhysics->GetStaticMeshComponent();
	EarlyPart->SetMobility(EComponentMobility::Movable);
	EarlyPart->SetStaticMesh(Part->GetStaticMesh());
	EarlyPart->SetWorldScale3D(Part->GetComponentScale());
	EarlyPart->SetCollisionProfileName(TEXT("PhysicsActor"));
	EarlyPart->SetSimulatePhysics(true);
	EarlyPart->SetPhysicsLinearVelocity(FVector(123, 45, 67));
	TestTrue(TEXT("Already-received physics must not skip local pushable setup"), TBPushableProps::TryEnable(*EarlyPhysics));
	TestTrue(TEXT("Late setup preserves received motion"), EarlyPart->IsAnyRigidBodyAwake() &&
		EarlyPart->GetPhysicsLinearVelocity().Equals(FVector(123, 45, 67), .1f));
	EarlyPhysics->Destroy();
	auto Probe = MakeShared<FExpandedPropProbe>();
	Probe->Part = Part;
	Probe->Original = Part->GetComponentTransform();
	Probe->Velocity = Part->GetPhysicsLinearVelocity();
	Probe->Spin = Part->GetPhysicsAngularVelocityInRadians();
	Probe->bAwake = Part->IsAnyRigidBodyAwake();
	// Each process creates the same isolated test floor. The prop itself must arrive through replication.
	auto* FloorActor = World->SpawnActor<AStaticMeshActor>(FVector(-6000, 0, 4500), FRotator::ZeroRotator);
	Probe->Floor = FloorActor;
	auto* Floor = FloorActor->GetStaticMeshComponent();
	Floor->SetMobility(EComponentMobility::Movable);
	Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->SetWorldScale3D(FVector(10, 10, .2));
	Floor->SetCollisionProfileName(TEXT("BlockAll"));
	const double Deadline = FPlatformTime::Seconds() + 60;
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, World, Probe, Deadline]()
	{
		const double Now = FPlatformTime::Seconds();
		if (Now > Deadline) { AddError(TEXT("Prop physics/replication timed out")); return true; }
		auto* P = Probe->Part.Get();
		if (!P) { AddError(TEXT("Prop disappeared")); return true; }
		if (World->GetNetMode() == NM_Client)
		{
			const FVector Position = P->Bounds.Origin;
			if (Position.Z < 4500 || Position.Z > 4550 || Position.X < -5970 || Position.X > -5700 || FMath::Abs(Position.Y) > 50) return false;
			if (!Probe->Started) Probe->Started = Now;
			if (Now - Probe->Started < 2.) return false;
			TestTrue(TEXT("Client received server physics on the existing map actor"), P->GetOwner()->GetLocalRole() == ROLE_SimulatedProxy);
			AddInfo(FString::Printf(TEXT("Replicated prop settled at %s"), *Position.ToString()));
			return true;
		}
		if (!Probe->Started)
		{
			if (World->GetNetMode() == NM_ListenServer && World->GetGameState<ATBGameState>()->PlayerArray.Num() < 3) return false;
			P->AddWorldOffset(FVector(-6000, 0, 4700) - P->Bounds.Origin, false, nullptr, ETeleportType::TeleportPhysics);
			P->SetPhysicsLinearVelocity(FVector::ZeroVector);
			P->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			P->AddImpulse(FVector(100, 0, 0), NAME_None, true);
			Probe->Started = Now;
			return false;
		}
		if (Now - Probe->Started < (World->GetNetMode() == NM_ListenServer ? 15. : 4.)) return false;
		const FVector Position = P->Bounds.Origin;
		TestTrue(TEXT("Impulse moves prop horizontally and collision catches its fall"),
			Position.X > -5970 && Position.X < -5700 && FMath::Abs(Position.Y) < 50 && Position.Z > 4500 && Position.Z < 4550);
		AddInfo(FString::Printf(TEXT("Authority prop settled at %s"), *Position.ToString()));
		return true;
	}));
	return true;
}
#endif
