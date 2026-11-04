#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Character/TBCharacter.h"
#include "Character/TBMovement.h"
#include "Core/TBPlayerState.h"
#include "Tests/AutomationCommon.h"
#include "GamePlay/TBBoxEscapeCourse.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBHouseInteractionTest, "ToonStory.Playroom.HouseInteractions",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBHouseInteractionTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
	if (!World || !World->GetMapName().EndsWith(TEXT("TB_StylizedHouse"))) return false;
	int32 Furniture = 0, Chairs = 0, Fridges = 0, Props = 0;
	UStaticMeshComponent* Probe = nullptr;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		auto* Part = It->GetStaticMeshComponent();
		const UStaticMesh* Mesh = Part->GetStaticMesh();
		if (!Mesh) continue;
		if (Mesh->GetPathName().StartsWith(TEXT("/Game/HouseCollision/")))
		{
			++Furniture;
			TestTrue(TEXT("Furniture uses its visible surface for collision"), Mesh->GetBodySetup()->CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple);
			if (Mesh->GetName().Contains(TEXT("SM_Fridge")))
			{
				++Fridges;
				const FBox Bounds = Part->Bounds.GetBox();
				FVector Top = Bounds.GetCenter(); Top.Z = Bounds.Max.Z;
				FHitResult Hit;
				FCollisionQueryParams Query(SCENE_QUERY_STAT(TBFridgeRoof), false);
				TestTrue(TEXT("Fridge has a blocking roof for ordinary character traces"),
				    Part->LineTraceComponent(Hit, Top + FVector(0, 0, 50), Top - FVector(0, 0, 50), Query));
				TestTrue(TEXT("Fridge roof is walkable"), Hit.ImpactNormal.Z > .7f);
			}
			if (Mesh->GetName().Contains(TEXT("SM_Vintage_Chair")))
			{
				++Chairs;
				const FBox Local = Mesh->GetBoundingBox();
				FVector Centre = Local.GetCenter();
				Centre.Z = Local.Min.Z + 13 / Part->GetComponentScale().Z;
				FCollisionQueryParams Query(SCENE_QUERY_STAT(TBChairPassage), false);
				// Ignore surrounding objects: this tests the formerly solid chair hull.
				for (TActorIterator<AActor> Other(World); Other; ++Other) if (*Other != *It) Query.AddIgnoredActor(*Other);
				bool Open = false;
				for (const FVector Axis : {FVector::ForwardVector, FVector::RightVector})
				{
					const float Distance = Local.GetExtent().GetMax() + 30;
					FHitResult Hit;
					Open |= !World->SweepSingleByChannel(Hit,
					    Part->GetComponentTransform().TransformPosition(Centre - Axis * Distance),
					    Part->GetComponentTransform().TransformPosition(Centre + Axis * Distance),
					    FQuat::Identity, ECC_GameTraceChannel1, FCollisionShape::MakeCapsule(7.5f, 9.75f), Query);
				}
				TestTrue(FString(TEXT("Toy capsule fits underneath chair: ")) + It->GetName(), Open);
			}
		}
		if (Part->ComponentHasTag(TEXT("TBPushable")))
		{
			++Props;
			TestEqual(TEXT("Human capsule passes through small prop"), Part->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
			TestEqual(TEXT("Toy capsule cannot stand on small prop"), Part->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Ignore);
			TestEqual(TEXT("Small prop does not retract camera"), Part->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
			TestTrue(TEXT("Small prop actually simulates physics"), Part->IsSimulatingPhysics());
			TestTrue(TEXT("Small prop movement replicates"), It->GetIsReplicated() && It->IsReplicatingMovement());
			TestTrue(TEXT("Small prop has simple physics collision"), Mesh->GetBodySetup() && Mesh->GetBodySetup()->AggGeom.GetElementCount() > 0);
			if (Mesh->GetName().Contains(TEXT("SM_Radio"))) Probe = Part;
		}
	}
	TestEqual(TEXT("All selected furniture components are configured"), Furniture, 27);
	TestEqual(TEXT("All dining chairs checked"), Chairs, 7);
	TestEqual(TEXT("Fridge checked"), Fridges, 1);
	TestEqual(TEXT("All small props configured"), Props, 55);
	if (TestNotNull(TEXT("Radio physics probe exists"), Probe))
	{
		auto* Body = Cast<ATBCharacter>(World->GetFirstPlayerController()->GetPawn());
		auto* Movement = Body ? Cast<UTBMovement>(Body->GetCharacterMovement()) : nullptr;
		if (!TestNotNull(TEXT("Character contact force implementation"), Movement)) return false;
		// Isolate the force check from shelving/walls that may legitimately stop a prop.
		const FTransform Original = Probe->GetComponentTransform();
		Probe->SetWorldLocation(FVector(15000, 0, 5000), false, nullptr, ETeleportType::TeleportPhysics);
		Probe->SetEnableGravity(false);
		Probe->SetPhysicsLinearVelocity(FVector::ZeroVector);
		FHitResult Impact(Probe->GetOwner(), Probe, Probe->Bounds.Origin, -FVector::ForwardVector);
		if (!TestNotNull(TEXT("Character team"), Body->TBPS())) return false;
		const auto OriginalTeam = Body->TBPS()->Team;
		Body->TBPS()->Team = ETBTeam::Human;
		Movement->ApplyImpactPhysicsForces(Impact, FVector(500, 0, 0), FVector(200, 0, 0));
		Body->TBPS()->Team = OriginalTeam;
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.2f));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Probe, Original]()
		{
			TestTrue(TEXT("Human contact moves the actual physics prop"), Probe->GetPhysicsLinearVelocity().Size() > .1f);
			Probe->SetWorldTransform(Original, false, nullptr, ETeleportType::TeleportPhysics);
			Probe->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Probe->SetEnableGravity(true);
			return true;
		}));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBDishStabilityTest, "ToonStory.Environment.DishStability",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBDishStabilityTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World) return false;
	TArray<TPair<TWeakObjectPtr<UStaticMeshComponent>, FVector>> Dishes;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		auto* Part = It->GetStaticMeshComponent();
		if (!Part || !Part->IsSimulatingPhysics() || !Part->GetStaticMesh()) continue;
		const FString Name = Part->GetStaticMesh()->GetName();
		if (Name.Contains(TEXT("Bowl")) || Name.Contains(TEXT("Plate")) || Name.Contains(TEXT("Cup")) || Name.Contains(TEXT("Mug")))
			Dishes.Emplace(Part, Part->Bounds.Origin);
	}
	TestTrue(TEXT("Stage contains dynamic dishes"), !Dishes.IsEmpty());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Dishes]()
	{
		for (const auto& Pair : Dishes)
			if (const auto* Part = Pair.Key.Get())
			{
				const double Drop = Pair.Value.Z - Part->Bounds.Origin.Z;
				AddInfo(FString::Printf(TEXT("Dish %s initial=%s final=%s drop=%.2f"), *Part->GetOwner()->GetName(), *Pair.Value.ToString(), *Part->Bounds.Origin.ToString(), Drop));
				TestTrue(FString(TEXT("Dish remains on support: ")) + Part->GetOwner()->GetName(), Drop < 10);
				// These authored dishes previously fell before automation even began.
				// Check their shelf/table height as well as stability over time.
				const FString Name = Part->GetOwner()->GetName();
				if (Name == TEXT("Wood_Bowl_A10")) TestTrue(TEXT("Coffee-table bowl stays above the tabletop"), Part->Bounds.Origin.Z > 69);
				if (Name == TEXT("Kitchen_Props_B_Bowl_A4_25")) TestTrue(TEXT("Kitchen bowl stays on the counter"), Part->Bounds.Origin.Z > 120);
				if (Name == TEXT("Wooden_Bowl_B3")) TestTrue(TEXT("Stacked bowl stays on the cabinet shelf"), Part->Bounds.Origin.Z > 320);
			}
		return true;
	}));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBSharedToyBoxTest, "ToonStory.Environment.SharedToyBox",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBSharedToyBoxTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World) return false;
	int32 Decorations = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
		if (It->ActorHasTag(TEXT("ToyBoxPlayDecor")))
		{
			++Decorations;
			TInlineComponentArray<UPrimitiveComponent*> Parts(*It);
			for (auto* Part : Parts) TestTrue(TEXT("Decoration cannot obstruct toys or the escape course"), Part->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		}
	TestTrue(TEXT("Shared toy-box decoration is loaded in the stage"), Decorations > 100);
	for (TActorIterator<ATBBoxEscapeCourse> It(World); It; ++It)
	{
		FString Error;
		TestTrue(*FString::Printf(TEXT("Authored escape course remains valid: %s"), *It->GetName()), It->Validate(Error));
		if (!Error.IsEmpty()) AddInfo(Error);
	}
	return !HasAnyErrors();
}
#endif
