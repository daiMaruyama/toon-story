// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Character/TBCharacter.h"
#include "Character/TBToyCharacter.h"
#include "Components/ChildActorComponent.h"
#include "Character/TBMovement.h"
#include "Character/TBToyPresentation.h"
#include "Data/TBCharacterData.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Camera/CameraActor.h"
#include "Core/TBPlayerState.h"
#include "Core/TBController.h"
#include "Core/TBLobbySettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "GameFramework/CharacterMovementComponent.h"

// Shared presentation coverage survives retirement of the old residential layouts.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRetainedStagePresentationTest, "ToonStory.Stages.RetainedPresentation",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBRetainedStagePresentationTest::RunTest(const FString& Parameters)
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
	const int32 StageId = TBLobbySettings::StageIndex(World);
	if (!TestTrue(TEXT("Retained Demo or Stylized House loaded"), StageId == 1 || StageId == 3))
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
			auto* ToyCharacter = Cast<ATBToyCharacter>(*It);
			const int32 OriginalSelection = ToyCharacter ? ToyCharacter->SelectedToyIndex : INDEX_NONE;
			// 選択済みの試合で実行した場合も、テスト前の状態へ戻す。
			ON_SCOPE_EXIT
			{
				Player->Team = OriginalTeam;
				if (ToyCharacter) ToyCharacter->SelectedToyIndex = OriginalSelection;
				It->ApplyState();
			};
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
			//キャラクター本体ではなく表示用Actorのメッシュを検証する。
			USkeletalMeshComponent* ToyMesh = It->GetMesh();
			UStaticMeshComponent* StaticToyMesh = nullptr;
			if (auto* Toy = Cast<ATBToyCharacter>(*It))
			{
				Toy->ChooseRandomToy();
				Toy->RefreshToyPresentation();
				if (auto* Visual = Toy->ToyVisual->GetChildActor())
				{
					ToyMesh = Visual->FindComponentByClass<USkeletalMeshComponent>();
					StaticToyMesh = Visual->FindComponentByClass<UStaticMeshComponent>();
				}
			}
			// The car uses a static mesh; the other toys use animated skeletal meshes.
			TestTrue(TEXT("Toy geometry is loaded"),
			         (ToyMesh && ToyMesh->GetSkeletalMeshAsset()) ||
			         (StaticToyMesh && StaticToyMesh->GetStaticMesh()));
			if (ToyMesh) TestNotNull(TEXT("Toy animation exists"), ToyMesh->GetAnimInstance());
			TestEqual(TEXT("Toy view scales with the body"),
			          It->Camera->GetRelativeLocation().Z + It->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),
			          20.0 * TBToyPresentation::SizeScale);
			auto* Movement = It->GetCharacterMovement();
			const FVector OriginalVelocity = Movement->Velocity;
			const EMovementMode OriginalMode = Movement->MovementMode;
			const bool OriginalFrozen = Player->bFrozen;
			Player->bFrozen = false;
			Movement->SetMovementMode(MOVE_Walking);
			It->SetSprintRequested(true);
			TestEqual(TEXT("Toy sprint speed"), Movement->GetMaxSpeed(), TB::CharacterData().Toy.SprintSpeed);
			auto* ToyMovement = CastChecked<UTBMovement>(Movement);
			auto* Prediction = ToyMovement->GetPredictionData_Client_Character();
			auto SavedSprint = Prediction->AllocateNewMove();
			SavedSprint->SetMoveFor(*It, .016f, FVector(1, 0, 0), *Prediction);
			TestTrue(TEXT("Sprint travels with movement packets"),
			         (SavedSprint->GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0) != 0);
			ToyMovement->UpdateFromCompressedFlags(0);
			TestEqual(TEXT("Released flag restores walking"), Movement->GetMaxSpeed(), TB::CharacterData().Toy.WalkSpeed);
			SavedSprint->PrepMoveFor(*It);
			TestEqual(TEXT("Prediction replay restores sprint"), Movement->GetMaxSpeed(), TB::CharacterData().Toy.SprintSpeed);
			TestEqual(TEXT("Higher toy jump applied"), Movement->JumpZVelocity, TB::CharacterData().Toy.JumpVelocity);
			Movement->Velocity = FVector(100, 0, 0);
			It->Tick(0.f);
			Player->bFrozen = true;
			It->Tick(0.f);
			TestEqual(TEXT("Sprint cannot bypass freeze"), Movement->GetMaxSpeed(), 0.f);
			const FVector FrozenPosition = It->GetActorLocation();
			ToyMovement->MoveAutonomous(0.f, .016f, SavedSprint->GetCompressedFlags(), FVector(100, 0, 0));
			TestTrue(TEXT("Held sprint survives frozen move replay"), ToyMovement->bWantsToSprint);
			It->SetActorLocation(FrozenPosition, false, nullptr, ETeleportType::TeleportPhysics);
			// 移動ロック中はポーズが停止すること。
			if (ToyMesh) TestTrue(TEXT("Gaze freeze stops pose"), ToyMesh->bPauseAnims || ToyMesh->GlobalAnimRateScale == 0.f);
			Player->bFrozen = false;
			It->Tick(0.f);
			if (ToyMesh) TestTrue(TEXT("Unfreeze resumes animation"), !ToyMesh->bPauseAnims && ToyMesh->GlobalAnimRateScale > 0.f);
			Player->bFrozen = OriginalFrozen;
			It->SetSprintRequested(false);
			Movement->SetMovementMode(OriginalMode);
			Movement->Velocity = OriginalVelocity;
			Player->Team = OriginalTeam;
			It->ApplyState();
			if (auto* Toy = Cast<ATBToyCharacter>(*It)) Toy->RefreshToyPresentation();
		}
	}
	TestTrue(TEXT("At least one character appearance was checked"), VerifiedCharacters > 0);
	// These mesh and population checks are shared by the retained stages, unlike the retired doorway grid.
	int32 Starts = 0, Boxes = 0, Pickups = 0;
	for (TActorIterator<APlayerStart> It(World); It; ++It) ++Starts;
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
	TestTrue(TEXT("At least eight distinct starts"), Starts >= 8);
	TestEqual(TEXT("One toy box"), Boxes, 1);
	TestEqual(TEXT("Retained stage authored collectible count"), Pickups, StageId == 3 ? 8 : 10);

	return !HasAnyErrors();
}
#endif
