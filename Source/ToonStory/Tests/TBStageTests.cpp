#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Character/TBCharacter.h"
#include "Character/TBMovement.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Camera/CameraActor.h"
#include "Core/TBPlayerState.h"
#include "Core/TBController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBResidentialStageTest, "ToonStory.Stages.RuntimeLayout",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBResidentialStageTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game)
		{
			World = Context.World();
		}
	}
	if (!TestNotNull(TEXT("Run in a game world"), World))
	{
		return false;
	}
	const bool Kids = World->GetMapName().Contains(TEXT("KidsRoom"));
	if (!TestTrue(TEXT("Residential stage loaded"), Kids || World->GetMapName().Contains(TEXT("ArchViz"))))
	{
		return false;
	}
	FCollisionQueryParams Query;
	int32 LobbyCameras = 0;
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("TBLobbyCamera")))
		{
			++LobbyCameras;
		}
	}
	TestEqual(TEXT("One authored lobby camera"), LobbyCameras, 1);
	TestTrue(TEXT("Near plane fits the small toy camera"), GNearClippingPlane > 0.f && GNearClippingPlane <= 1.f);
	if (auto* Controller = Cast<ATBController>(World->GetFirstPlayerController()))
	{
		// Late possession can restore the pawn view after the lobby camera was selected.
		Controller->SetViewTarget(Controller->GetPawn());
		Controller->PlayerTick(0.f);
		TestTrue(TEXT("Lobby camera recovers after possession changes the view"),
		         Controller->GetViewTarget() && Controller->GetViewTarget()->ActorHasTag(TEXT("TBLobbyCamera")));
	}
	else
	{
		AddError(TEXT("Local controller required to verify lobby camera"));
	}
	int32 VerifiedCharacters = 0;
	for (TActorIterator<ATBCharacter> It(World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
		if (auto* Player = It->TBPS())
		{
			++VerifiedCharacters;
			const ETBTeam OriginalTeam = Player->Team;
			Player->Team = ETBTeam::Human;
			It->ApplyState();
			TestNotNull(TEXT("Child skeletal mesh loaded"), It->GetMesh()->GetSkeletalMeshAsset());
			TestTrue(TEXT("Child uses the configured model"),
			         It->ChildMesh.Get() && It->GetMesh()->GetSkeletalMeshAsset() == It->ChildMesh.Get());
			TestNotNull(TEXT("Child animation instance initialized"), It->GetMesh()->GetAnimInstance());
			// 他クライアントでは入力加速度がなくても、受信した速度で足が動く必要がある。
			auto* ChildMovement = It->GetCharacterMovement();
			const FVector ChildVelocity = ChildMovement->Velocity;
			const EMovementMode ChildMode = ChildMovement->MovementMode;
			ChildMovement->SetMovementMode(MOVE_Walking);
			ChildMovement->Velocity = FVector(450, 0, 0);
			auto* ChildVisual = It->GetMesh();
			FName Foot;
			for (int32 Bone = 0; Bone < ChildVisual->GetNumBones(); ++Bone)
			{
				const FName Name = ChildVisual->GetBoneName(Bone);
				if (Name.ToString().Contains(TEXT("foot")) && !Name.ToString().Contains(TEXT("ik_")))
				{
					Foot = Name;
					break;
				}
			}
			TestFalse(TEXT("Child has a foot bone"), Foot.IsNone());
			FVector PreviousFoot = FVector::ZeroVector;
			float FootTravel = 0;
			for (int32 Frame = 0; Frame < 20; ++Frame)
			{
				ChildVisual->TickAnimation(.05f, false);
				ChildVisual->RefreshBoneTransforms();
				const FVector Position = ChildVisual->GetSocketLocation(Foot);
				if (Frame > 3)
				{
					FootTravel += FVector::Distance(PreviousFoot, Position);
				}
				PreviousFoot = Position;
			}
			TestTrue(TEXT("Child feet animate with replicated velocity and no acceleration"), FootTravel > 5.f);
			// 設定角度だけでなく、評価済みの足先が操作方向を向いていることを検証する。
			const FRotator ChildRotation = It->GetActorRotation();
			for (float Yaw : {0.f, 90.f, 180.f, -90.f})
			{
				It->SetActorRotation(FRotator(0, Yaw, 0));
				for (float Speed : {0.f, 450.f})
				{
					ChildMovement->Velocity = It->GetActorForwardVector() * Speed;
					for (int32 Frame = 0; Frame < 20; ++Frame)
					{
						ChildVisual->TickAnimation(.05f, false);
						ChildVisual->RefreshBoneTransforms();
					}
					const FVector Toes =
					    ChildVisual->GetSocketLocation(TEXT("ball_l")) + ChildVisual->GetSocketLocation(TEXT("ball_r"));
					const FVector Feet =
					    ChildVisual->GetSocketLocation(TEXT("foot_l")) + ChildVisual->GetSocketLocation(TEXT("foot_r"));
					const float Facing =
					    FVector::DotProduct((Toes - Feet).GetSafeNormal2D(), It->GetActorForwardVector());
					TestTrue(*FString::Printf(TEXT("Child faces view yaw %.0f at speed %.0f (dot %.2f)"), Yaw, Speed,
					                          Facing),
					         Facing > .8f);
				}
			}
			It->SetActorRotation(ChildRotation);
			TestTrue(TEXT("Network smoothing preserves the child's facing offset"),
			         It->GetBaseRotationOffset().Equals(ChildVisual->GetRelativeRotation().Quaternion(), .001f));
			ChildMovement->Velocity = ChildVelocity;
			ChildMovement->SetMovementMode(ChildMode);
			Player->Team = ETBTeam::Toy;
			It->ApplyState();
			TestTrue(TEXT("Toy uses animated robot"),
			         It->ToySkeletalMesh.Get() && It->GetMesh()->GetSkeletalMeshAsset() == It->ToySkeletalMesh.Get());
			TestNotNull(TEXT("Toy idle animation loaded"), It->ToyIdleAnimation.Get());
			TestNotNull(TEXT("Toy walk animation loaded"), It->ToyWalkAnimation.Get());
			TestNotNull(TEXT("Toy jump animation loaded"), It->ToyJumpAnimation.Get());
			TestEqual(TEXT("Toy view is 20 cm above capsule bottom"),
			          It->Camera->GetRelativeLocation().Z + It->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),
			          20.0);
			auto* Movement = It->GetCharacterMovement();
			const FVector OriginalVelocity = Movement->Velocity;
			const EMovementMode OriginalMode = Movement->MovementMode;
			const bool OriginalFrozen = Player->bFrozen;
			Player->bFrozen = false;
			Movement->SetMovementMode(MOVE_Walking);
			It->SetSprintRequested(true);
			TestEqual(TEXT("Toy sprint speed"), Movement->GetMaxSpeed(), It->ToySprintSpeed);
			auto* ToyMovement = CastChecked<UTBMovement>(Movement);
			auto* Prediction = ToyMovement->GetPredictionData_Client_Character();
			auto SavedSprint = Prediction->AllocateNewMove();
			SavedSprint->SetMoveFor(*It, .016f, FVector(1, 0, 0), *Prediction);
			TestTrue(TEXT("Sprint travels with movement packets"),
			         (SavedSprint->GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0) != 0);
			ToyMovement->UpdateFromCompressedFlags(0);
			TestEqual(TEXT("Released flag restores walking"), Movement->GetMaxSpeed(), It->ToySpeed);
			SavedSprint->PrepMoveFor(*It);
			TestEqual(TEXT("Prediction replay restores sprint"), Movement->GetMaxSpeed(), It->ToySprintSpeed);
			TestEqual(TEXT("Higher toy jump applied"), Movement->JumpZVelocity, It->ToyJumpVelocity);
			Movement->Velocity = FVector(100, 0, 0);
			It->Tick(0.f);
			auto* Animation = It->GetMesh()->GetSingleNodeInstance();
			if (TestNotNull(TEXT("Robot animation instance exists"), Animation))
			{
				TestTrue(TEXT("Moving robot plays walk"), Animation->GetCurrentAsset() == It->ToyWalkAnimation.Get());
				Player->bFrozen = true;
				It->Tick(0.f);
				TestEqual(TEXT("Sprint cannot bypass freeze"), Movement->GetMaxSpeed(), 0.f);
				const FVector FrozenPosition = It->GetActorLocation();
				ToyMovement->MoveAutonomous(0.f, .016f, SavedSprint->GetCompressedFlags(), FVector(100, 0, 0));
				TestTrue(TEXT("Held sprint survives frozen move replay"), ToyMovement->bWantsToSprint);
				It->SetActorLocation(FrozenPosition, false, nullptr, ETeleportType::TeleportPhysics);
				TestTrue(TEXT("Gaze freeze preserves current clip"),
				         Animation->GetCurrentAsset() == It->ToyWalkAnimation.Get());
				TestEqual(TEXT("Gaze freeze stops pose"), It->GetMesh()->GlobalAnimRateScale, 0.f);
				Player->bFrozen = false;
				It->Tick(0.f);
				TestEqual(TEXT("Unfreeze resumes animation"), It->GetMesh()->GlobalAnimRateScale, 1.f);
			}
			Player->bFrozen = OriginalFrozen;
			It->SetSprintRequested(false);
			Movement->SetMovementMode(OriginalMode);
			Movement->Velocity = OriginalVelocity;
			Player->Team = OriginalTeam;
			It->ApplyState();
		}
	}
	TestTrue(TEXT("At least one character appearance was checked"), VerifiedCharacters > 0);
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
	int32 Starts = 0, Boxes = 0, Pickups = 0, Doors = 0, Barriers = 0;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		++Starts;
		const FVector P = It->GetActorLocation();
		TestFalse(*FString::Printf(TEXT("Spawn %s clear"), *It->GetName()),
		          World->OverlapBlockingTestByChannel(P, FQuat::Identity, ECC_Pawn,
		                                              FCollisionShape::MakeCapsule(34, 88), Query));
		FHitResult Floor;
		TestTrue(*FString::Printf(TEXT("Spawn %s has floor"), *It->GetName()),
		         World->LineTraceSingleByChannel(Floor, P, P - FVector(0, 0, 150), ECC_Pawn, Query));
	}
	for (TActorIterator<ATBBox> It(World); It; ++It)
	{
		++Boxes;
		TestTrue(TEXT("Toy chest uses the configured mesh"),
		         It->BoxMesh.Get() && It->Visual->GetStaticMesh() == It->BoxMesh.Get());
		TestTrue(TEXT("Toy chest fits residential furniture scale"), It->Visual->Bounds.BoxExtent.GetMax() < 110.f);
	}
	for (TActorIterator<ATBPickup> It(World); It; ++It)
	{
		++Pickups;
		TestTrue(TEXT("Collectible uses the configured battery mesh"),
		         It->PickupMesh.Get() && It->Mesh->GetStaticMesh() == It->PickupMesh.Get());
		TestTrue(TEXT("Battery has visible real-world scale"),
		         FMath::IsNearlyEqual(It->Mesh->Bounds.BoxExtent.Z * 2, 12.0, .5));
	}
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		FVector Origin, Extent;
		It->GetActorBounds(false, Origin, Extent);
		if (It->ActorHasTag(TEXT("TBStructuralBarrier")))
		{
			++Barriers;
			const FVector Across = Extent.X < Extent.Y ? FVector(Extent.X + 35, 0, 0) : FVector(0, Extent.Y + 35, 0);
			for (const bool Toy : {true, false})
			{
				const float HalfHeight = Toy ? 13.f : 65.f;
				FVector Center = Origin;
				// 小型化で抜けやすい壁の下端を、実際の体格で検査する。
				Center.Z = Origin.Z - Extent.Z + HalfHeight + 2.f;
				for (const float Direction : {-1.f, 1.f})
				{
					FHitResult Hit;
					FCollisionQueryParams WallQuery;
					for (TActorIterator<AActor> Other(World); Other; ++Other)
					{
						if (*Other != *It)
						{
							WallQuery.AddIgnoredActor(*Other);
						}
					}
					TestTrue(*FString::Printf(TEXT("Wall blocks %s from either side: %s"),
					                          Toy ? TEXT("toy") : TEXT("child"), *It->GetName()),
					         World->SweepSingleByChannel(Hit, Center - Across * Direction, Center + Across * Direction,
					                                     FQuat::Identity, Toy ? ECC_GameTraceChannel1 : ECC_Pawn,
					                                     FCollisionShape::MakeCapsule(Toy ? 10.f : 26.f, HalfHeight),
					                                     WallQuery));
				}
			}
		}
		if (Extent.GetMax() * 2 < 30 &&
		    It->GetStaticMeshComponent()->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			AddError(FString::Printf(TEXT("Small prop must not block players: %s"), *It->GetName()));
		}
		if (It->ActorHasTag(TEXT("DoorFixedOpen")))
		{
			++Doors;
			TestEqual(TEXT("Door cannot move"), It->GetStaticMeshComponent()->Mobility.GetValue(),
			          EComponentMobility::Static);
		}
	}
	TestTrue(TEXT("At least eight distinct starts"), Starts >= 8);
	TestEqual(TEXT("One toy box"), Boxes, 1);
	TestTrue(TEXT("Enough collectibles"), Pickups >= 5);
	TestTrue(TEXT("Open doors in four rooms"), Doors >= 4);
	TestTrue(TEXT("Structural wall collision is authored"), Barriers >= 12);
	TArray<TPair<FVector, FVector>> Routes;
	for (int32 Index = 0; Index < (Kids ? 4 : 2); ++Index)
	{
		AActor* Start = nullptr;
		AActor* End = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(FName(*FString::Printf(TEXT("TBRouteStart%d"), Index))))
			{
				Start = *It;
			}
			if (It->ActorHasTag(FName(*FString::Printf(TEXT("TBRouteEnd%d"), Index))))
			{
				End = *It;
			}
		}
		if (TestNotNull(TEXT("Door route start"), Start) && TestNotNull(TEXT("Door route end"), End))
		{
			Routes.Add({Start->GetActorLocation(), End->GetActorLocation()});
		}
	}
	for (const auto& Route : Routes)
	{
		FHitResult Hit;
		const bool Blocked = World->SweepSingleByChannel(Hit, Route.Key, Route.Value, FQuat::Identity, ECC_Pawn,
		                                                 FCollisionShape::MakeCapsule(26, 65), Query);
		TestFalse(*FString::Printf(TEXT("Child can cross doorway; blocker=%s"), *GetNameSafe(Hit.GetActor())), Blocked);
	}

	return !HasAnyErrors();
}
#endif
