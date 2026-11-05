// Copyright (c) 2026 Mamodou. All Rights Reserved.
#include "TBPushableProps.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "PhysicsEngine/BodySetup.h"

bool TBPushableProps::TryEnable(AStaticMeshActor& Prop)
{
	auto* Part = Prop.GetStaticMeshComponent();
	if (!Part || Part->ComponentHasTag(TEXT("TBPushable")) || Part->ComponentHasTag(TEXT("TBFixedProp")) ||
	    Prop.ActorHasTag(TEXT("TBFixedProp")) || Prop.GetAttachParentActor() ||
	    Prop.GetClass() != AStaticMeshActor::StaticClass()) return false;
	// Shared storage-room displays and gameplay Blueprints remain authored scenery.
	const FString Level = Prop.GetLevel()->GetOutermost()->GetName();
	if (!Level.EndsWith(TEXT("TB_StylizedHouse")) && !Level.EndsWith(TEXT("TB_ToyWorldDemoExpanded"))) return false;
	const UStaticMesh* Mesh = Part->GetStaticMesh();
	if (!Mesh) return false;
	static const TSet<FString> KitchenNames = {
		TEXT("SM_FoodCan01"), TEXT("SM_FoodCan02"), TEXT("SM_FoodCan03"), TEXT("SM_FoodCan04"),
		TEXT("SM_FoodCan05"), TEXT("SM_FoodCan06"), TEXT("SM_FoodCan07"), TEXT("SM_FoodCan08"), TEXT("SM_FoodCan09"),
		TEXT("SM_Cereal_Box"), TEXT("SM_Cereal_Box_B"), TEXT("SM_Cereal_Box_C"), TEXT("SM_Timer"),
		TEXT("SM_Kitchen_Props_B_Cookie_Jar"), TEXT("SM_Glass_Jar_A"), TEXT("SM_Bathroom_Water_Glass")
	};
	static const TSet<FString> ToyNames = {
		TEXT("SM_Plastic_Truck_01"), TEXT("SM_ToyCar_01"), TEXT("SM_Wooden_Typewriter"),
		TEXT("SM_Battery_01"), TEXT("SM_Battery_02"), TEXT("SM_Gamepad02"), TEXT("SM_Remote_Control_01"),
		TEXT("SM_Train_01"), TEXT("SM_Train_02"), TEXT("SM_Train_03"), TEXT("SM_Tractor_01"), TEXT("SM_Tractor_02"),
		TEXT("SM_Cassette01"), TEXT("SM_Cassette02"), TEXT("SM_Cassette03"), TEXT("SM_Paint_01"),
		TEXT("SM_Apple01"), TEXT("SM_Banana01"), TEXT("SM_Orange01"), TEXT("SM_Thermos")
	};
	const FString Package = Mesh->GetOutermost()->GetName();
	if (!(Package.StartsWith(TEXT("/Game/StylizedKitchen/Meshes/")) && KitchenNames.Contains(Mesh->GetName())) &&
	    !(Package.StartsWith(TEXT("/Game/ToyWorld/Meshes/")) && ToyNames.Contains(Mesh->GetName()))) return false;
	const auto* Body = Mesh->GetBodySetup();
	if (!Body || Body->AggGeom.GetElementCount() == 0 || Body->GetCollisionTraceFlag() == CTF_UseComplexAsSimple ||
	    Part->Bounds.BoxExtent.GetMax() > 35.) return false;

	// A client's first replicated physics state can arrive before GameState::BeginPlay.
	// Still apply tags/collision in that order, without putting a moving replicated body to sleep.
	const bool bWasSimulating = Part->IsSimulatingPhysics();
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetCanEverAffectNavigation(false);
	Part->ComponentTags.AddUnique(TEXT("TBPushable"));
	Part->ComponentTags.AddUnique(TEXT("TBRuntimePushable"));
	Part->SetCollisionProfileName(TEXT("PhysicsActor"));
	Part->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Part->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	Part->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Part->CanCharacterStepUpOn = ECB_No;
	Part->SetMassOverrideInKg(NAME_None, 1.f);
	Part->SetLinearDamping(.6f);
	Part->SetAngularDamping(2.f);
	Part->SetUseCCD(true);
	Part->SetEnableGravity(true);
	Part->SetSimulatePhysics(true);
	// Keep the authored arrangement and avoid spending physics time on untouched props.
	if (!bWasSimulating) Part->PutRigidBodyToSleep();
	Prop.bStaticMeshReplicateMovement = true;
	if (Prop.GetNetMode() != NM_Client)
	{
		Prop.SetReplicates(true);
		Prop.SetReplicateMovement(true);
	}
	return true;
}
