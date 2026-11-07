// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AIController.h"
#include "Character/TBToyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Character/TBHumanPose.h"
#include "Data/TBDataSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBHumanAppearanceGCTest, "ToonStory.Appearance.HumanAssetsSurviveGC",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBHumanAppearanceGCTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetNetMode() == NM_Standalone)
			World = Context.World();
	if (!TestNotNull(TEXT("Standalone game world"), World)) return false;

	const FTransform SpawnTransform(FVector(-10000, -10000, 500));
	auto* Character = World->SpawnActorDeferred<ATBToyCharacter>(ATBToyCharacter::StaticClass(), SpawnTransform,
	    nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Deferred character"), Character)) return false;
	AAIController* Controller = nullptr;
	ATBPlayerState* Info = nullptr;
	ON_SCOPE_EXIT
	{
		if (Controller) Controller->Destroy();
		if (Info) Info->Destroy();
		Character->Destroy();
	};

	USkeletalMesh* SourceMesh = Character->ChildMesh.LoadSynchronous();
	if (!TestNotNull(TEXT("Configured child mesh is available"), SourceMesh)) return false;
	// A unique transient copy prevents other players or editor assets from accidentally keeping it alive.
	TWeakObjectPtr<USkeletalMesh> WeakMesh = DuplicateObject<USkeletalMesh>(SourceMesh, GetTransientPackage());
	TWeakObjectPtr<USkeletalMesh> UnretainedMesh = DuplicateObject<USkeletalMesh>(SourceMesh, GetTransientPackage());
	Character->ChildMesh = WeakMesh.Get();
	Character->FinishSpawning(SpawnTransform);
	TestNull(TEXT("No player state has arrived yet"), Character->GetPlayerState());
	TestNull(TEXT("Child mesh is not yet held by the display component"), Character->GetMesh()->GetSkeletalMeshAsset());
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("Control mesh without a strong reference is collected"), UnretainedMesh.IsValid());
	if (!TestTrue(TEXT("Loaded child mesh survives GC before team assignment"), WeakMesh.IsValid())) return false;
	TestTrue(TEXT("Loaded animation class survives GC before team assignment"), Character->ChildAnimation.IsValid());

	Controller = World->SpawnActor<AAIController>();
	Info = World->SpawnActor<ATBPlayerState>();
	if (!TestNotNull(TEXT("Controller"), Controller) || !TestNotNull(TEXT("Player state"), Info)) return false;
	Controller->SetPlayerState(Info);
	Info->SetOwner(Controller);
	Info->Team = ETBTeam::Human;
	Controller->Possess(Character);
	Character->ApplyState();
	TestTrue(TEXT("Late human assignment displays the retained mesh"), Character->GetMesh()->GetSkeletalMeshAsset() == WeakMesh.Get());
	TestFalse(TEXT("Human does not use the fallback cube"), Character->Body->IsVisible());
	TestNotNull(TEXT("Human animation is initialized"), Character->GetMesh()->GetAnimInstance());

	Info->Team = ETBTeam::Toy;
	Character->ApplyState();
	TestNull(TEXT("Toy appearance releases the displayed child mesh"), Character->GetMesh()->GetSkeletalMeshAsset());
	CollectGarbage(RF_NoFlags);
	if (!TestTrue(TEXT("Child mesh survives GC while displaying a toy"), WeakMesh.IsValid())) return false;
	Info->Team = ETBTeam::Human;
	Character->ApplyState();
	TestTrue(TEXT("Returning to human restores the same mesh"), Character->GetMesh()->GetSkeletalMeshAsset() == WeakMesh.Get());
	TestTrue(TEXT("Returning human mesh is visible"), Character->GetMesh()->IsVisible());
	TestFalse(TEXT("Returning human does not use the fallback cube"), Character->Body->IsVisible());
	for (const auto& Look : GetDefault<UTBDataSettings>()->HumanAppearances)
	{
		Info->HumanAppearance = Look;
		Character->ApplyState();
		auto* Assembly = Character->HumanVisual->GetChildActor();
		if (!TestNotNull(TEXT("Configured human assembly spawns"), Assembly)) return false;
		USkeletalMeshComponent* BodyMesh = nullptr;
		USkeletalMeshComponent* FaceMesh = nullptr;
		for (auto* Part : TInlineComponentArray<USkeletalMeshComponent*>(Assembly))
		{
			if (Cast<UTBHumanBodyPose>(Part->GetAnimInstance())) BodyMesh = Part;
			if (Part->GetName() == TEXT("Face")) FaceMesh = Part;
		}
		if (!TestNotNull(TEXT("Retargeted body"), BodyMesh) || !TestNotNull(TEXT("Face"), FaceMesh)) return false;
		// A rear-facing entrance shot can occlude the face while its bound groom
		// is visible. Exercise the engine's visibility gate, not a forced pose tick.
		FaceMesh->bRecentlyRendered = false;
		TestTrue(TEXT("Offscreen face continues driving the groom binding pose"), FaceMesh->ShouldTickPose());
		TestTrue(TEXT("Offscreen face continues refreshing groom binding bones"), FaceMesh->ShouldUpdateTransform(false));
		for (auto* Part : {Character->GetMesh(), BodyMesh, FaceMesh})
		{
			Part->TickAnimation(1.f / 30.f, false);
			Part->RefreshBoneTransforms();
		}
		TestTrue(*FString::Printf(TEXT("%s head stays on body"), *Assembly->GetName()),
		    FaceMesh->DoesSocketExist(TEXT("head")) && BodyMesh->DoesSocketExist(TEXT("head")) &&
		    FaceMesh->GetSocketLocation(TEXT("head")).Equals(BodyMesh->GetSocketLocation(TEXT("head")), 1.0));
		TestFalse(TEXT("Original child is hidden under the selected assembly"), Character->GetMesh()->IsVisible());
	}
	return true;
}
#endif
