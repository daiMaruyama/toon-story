#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Character/TBCharacter.h"
#include "Character/TBHumanPose.h"
#include "Character/TBToyCarVisual.h"
#include "Character/TBToyCharacter.h"
#include "Character/TBToyPresentation.h"
#include "Character/TBToyFacingPose.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Input/TBInputSettings.h"
#include "Input/TBKeyLabels.h"
#include "Input/TBKeyRemap.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "AIController.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GamePlay/TBDummyToy.h"

namespace
{
	UWorld* AnimationTestWorld()
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World()->GetNetMode() == NM_Standalone)
				return Context.World();
		}
		return nullptr;
	}

	// Deterministic pose evaluation, not gameplay/capture validation. Freeze only the fixture's base idle clock;
	// the real post-process PreUpdate/Evaluate still receive Delta and must implement the blend themselves.
	void PoseFrames(USkeletalMeshComponent* Mesh, int32 Frames = 1)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Mesh->TickAnimation(1.f / 60.f, false);
			Mesh->RefreshBoneTransforms();
		}
	}

	using FHands = TArray<FTransform>;
	FHands Hands(USkeletalMeshComponent* Mesh)
	{
		return {Mesh->GetSocketTransform(TEXT("hand_l"), RTS_Component),
		        Mesh->GetSocketTransform(TEXT("hand_r"), RTS_Component)};
	}

	double PoseDistance(const FHands& A, const FHands& B)
	{
		double Result = 0;
		for (int32 Hand = 0; Hand < A.Num(); ++Hand)
		{
			Result += FVector::Distance(A[Hand].GetLocation(), B[Hand].GetLocation());
			Result += 10. * A[Hand].GetRotation().AngularDistance(B[Hand].GetRotation());
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNormalCarryPoseTest, "ToonStory.NormalAnimation.CarryRelease",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNormalCarryPoseTest::RunTest(const FString&)
{
	auto* World = AnimationTestWorld();
	if (!TestNotNull(TEXT("Standalone game world"), World)) return false;
	TArray<AActor*> Fixtures;
	ON_SCOPE_EXIT { for (auto* Actor : Fixtures) if (IsValid(Actor)) Actor->Destroy(); };
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Human = World->SpawnActor<ATBCharacter>(ATBCharacter::StaticClass(), FVector(-10000, 0, 2000),
	                                          FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Pose fixture human"), Human)) return false;
	Fixtures.Add(Human);
	Human->SetActorTickEnabled(false);
	Human->SetActorEnableCollision(false);
	Human->GetCharacterMovement()->SetComponentTickEnabled(false);
	auto* Info = World->SpawnActor<ATBPlayerState>();
	if (!TestNotNull(TEXT("Pose fixture state"), Info)) return false;
	Fixtures.Add(Info);
	Info->Team = ETBTeam::Human;
	Human->SetPlayerState(Info);
	Human->ApplyState();
	auto* Mesh = Human->GetMesh();
	auto* Child = Mesh->GetSkeletalMeshAsset();
	auto* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/ChildAnimation/Child_MM_Idle"));
	auto* Post = LoadClass<UAnimInstance>(nullptr,
	    TEXT("/Game/Characters/ChildAnimation/ABP_TBHumanPose.ABP_TBHumanPose_C"));
	if (!TestNotNull(TEXT("Configured child mesh"), Child) || !TestNotNull(TEXT("Existing child idle"), Idle) ||
	    !TestNotNull(TEXT("Existing HumanPose class"), Post)) return false;
	if (!TestTrue(TEXT("Human hand bones"), Mesh->DoesSocketExist(TEXT("hand_l")) &&
	              Mesh->DoesSocketExist(TEXT("hand_r")))) return false;
	auto ConfigureIdle = [&]
	{
		// The previous fixture may have a native post-process on another skeleton. Changing the override
		// alone does not replace that instance; PlayAnimation also does not reinitialize an existing
		// SingleNode mode. Reinitialize first, then restore the deterministic idle playback.
		Mesh->SetOverridePostProcessAnimBP(Post, true);
		Mesh->PlayAnimation(Idle, false);
		Mesh->GetSingleNodeInstance()->SetPosition(0, false);
		Mesh->GetSingleNodeInstance()->SetPlaying(false);
		Mesh->bPauseAnims = false;
	};
	ConfigureIdle();
	if (!TestTrue(TEXT("Native human post-process active"), IsValid(Cast<UTBHumanPose>(Mesh->GetPostProcessInstance()))))
		return false;
	PoseFrames(Mesh, 2);
	const FHands Rest = Hands(Mesh);
	ATBDummyToy* Toys[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Toys[Index] = World->SpawnActor<ATBDummyToy>();
		if (!TestNotNull(TEXT("Carry presentation fixture"), Toys[Index])) return false;
		Fixtures.Add(Toys[Index]);
		Toys[Index]->SetActorTickEnabled(false);
		Toys[Index]->SetActorEnableCollision(false);
		Toys[Index]->Visual->SetChildActorClass(ATBToyCarVisual::StaticClass());
		auto* Car = Cast<ATBToyCarVisual>(Toys[Index]->Visual->GetChildActor());
		if (!TestNotNull(TEXT("Existing car presentation"), Car) ||
		    !TestNotNull(TEXT("Existing car mesh"), Car->Car->GetStaticMesh().Get())) return false;
		Car->Car->SetRelativeScale3D(FVector(Index == 0 ? .85f : 1.5f));
	}
	FVector Left, Right;
	Human->CarriedDummy = Toys[0];
	if (!TestTrue(TEXT("Fixture has real carry contact geometry"), TBToyPresentation::CarryContacts(Human, Left, Right)))
		return false;
	PoseFrames(Mesh);
	const FHands FreshA = Hands(Mesh);
	PoseFrames(Mesh, 50);
	const FHands Held = Hands(Mesh);
	const double HeldDistance = PoseDistance(Held, Rest);
	TestTrue(TEXT("Real carrying IK differs from idle"), HeldDistance > 1.);
	Human->CarriedDummy = nullptr;
	Toys[0]->SetActorLocation(FVector(5000, 5000, 5000));
	PoseFrames(Mesh);
	const FHands Releasing = Hands(Mesh);
	TestTrue(TEXT("Release retains a decaying pose instead of snapping to idle"), PoseDistance(Releasing, Rest) > .1);
	TestTrue(TEXT("First release step is smaller than the whole return to idle"),
	         PoseDistance(Releasing, Held) < HeldDistance * .75);
	Human->CarriedDummy = Toys[0];
	PoseFrames(Mesh);
	TestTrue(TEXT("Reacquiring the same toy starts fresh rather than reusing release alpha"),
	         PoseDistance(Hands(Mesh), FreshA) < .05);
	Human->CarriedDummy = nullptr;
	PoseFrames(Mesh, 90);
	TestTrue(TEXT("Release converges back to the unchanged base idle"), PoseDistance(Hands(Mesh), Rest) < .05);
	Human->CarriedDummy = Toys[1];
	PoseFrames(Mesh);
	const FHands FreshB = Hands(Mesh);
	Human->CarriedDummy = Toys[0];
	PoseFrames(Mesh, 50);
	Human->CarriedDummy = Toys[1];
	PoseFrames(Mesh);
	TestTrue(TEXT("Changing carried object discards the old target and blend alpha"),
	         PoseDistance(Hands(Mesh), FreshB) < .05);
	Toys[1]->Visual->SetChildActorClass(nullptr);
	PoseFrames(Mesh);
	TestTrue(TEXT("Missing visual contacts cannot reuse old IK"), PoseDistance(Hands(Mesh), Rest) < .05);
	Human->CarriedDummy = Toys[0];
	PoseFrames(Mesh, 50);
	Info->Team = ETBTeam::Toy;
	PoseFrames(Mesh);
	TestTrue(TEXT("Role change clears human IK immediately"), PoseDistance(Hands(Mesh), Rest) < .05);
	Info->Team = ETBTeam::Human;
	Human->CarriedDummy = nullptr;
	PoseFrames(Mesh);
	TestTrue(TEXT("Returning to human without a toy has no stale grip"), PoseDistance(Hands(Mesh), Rest) < .05);
	Human->CarriedDummy = Toys[0];
	PoseFrames(Mesh, 50);
	// A different skeleton with no humanoid hands must be safe even with a native post-process attached.
	// This is a transient fixture configuration, not a retarget or a Content edit.
	auto* Duck = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/ToyCollectionRealistic/Duck/SK_Duck"));
	auto* DuckIdle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/ToyCollectionRealistic/Duck/AN_Idle"));
	if (!TestNotNull(TEXT("Existing alternate skeleton"), Duck) || !TestNotNull(TEXT("Matching alternate idle"), DuckIdle))
		return false;
	Mesh->SetOverridePostProcessAnimBP(UTBHumanPose::StaticClass(), false);
	Mesh->SetSkeletalMesh(Duck);
	Mesh->PlayAnimation(DuckIdle, false);
	Mesh->GetSingleNodeInstance()->SetPlaying(false);
	TestFalse(TEXT("Alternate skeleton lacks required human hands"), Mesh->DoesSocketExist(TEXT("hand_l")));
	TestNotNull(TEXT("Native post-process on alternate skeleton"), Mesh->GetPostProcessInstance());
	PoseFrames(Mesh, 3);
	for (const auto& Bone : Mesh->GetComponentSpaceTransforms())
		TestFalse(TEXT("Missing humanoid bones produce finite fallback transforms"), Bone.ContainsNaN());
	Mesh->SetSkeletalMesh(nullptr);
	Human->CarriedDummy = nullptr;
	Mesh->SetSkeletalMesh(Child);
	auto* BeforeRestore = Mesh->GetPostProcessInstance();
	AddInfo(FString::Printf(TEXT("Restore before ConfigureIdle: post=%s expected=%s, pose distance=%.6f"),
	                       *GetPathNameSafe(BeforeRestore ? BeforeRestore->GetClass() : nullptr),
	                       *GetPathNameSafe(Post), PoseDistance(Hands(Mesh), Rest)));
	ConfigureIdle();
	auto* RestoredPost = Mesh->GetPostProcessInstance();
	if (!TestTrue(TEXT("Restored compiled human post-process is actually installed"),
	              RestoredPost && RestoredPost->GetClass() == Post)) return false;
	if (!TestNotNull(TEXT("Restored post-process has its linked base-pose input"),
	                 RestoredPost->GetLinkedInputPoseNode())) return false;
	TestTrue(TEXT("Restored single-node base asset is the original idle"),
	         Mesh->GetSingleNodeInstance() && Mesh->GetSingleNodeInstance()->GetCurrentAsset() == Idle);
	PoseFrames(Mesh);
	const double RestoredDistance = PoseDistance(Hands(Mesh), Rest);
	AddInfo(FString::Printf(TEXT("Restore after one pose frame: post=%s, pose distance=%.6f, held distance=%.6f"),
	                       *GetPathNameSafe(RestoredPost->GetClass()), RestoredDistance, HeldDistance));
	TestTrue(TEXT("Mesh/animation reinitialization cannot retain the old carry pose"),
	         RestoredDistance < .05);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNormalToyGaitTest, "ToonStory.NormalAnimation.ToyGait",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNormalToyGaitTest::RunTest(const FString&)
{
	auto* World = AnimationTestWorld();
	if (!TestNotNull(TEXT("Standalone game world"), World)) return false;
	auto* Class = LoadClass<ATBToyCharacter>(nullptr, TEXT("/Game/Characters/Toys/BP_ToyCharacter.BP_ToyCharacter_C"));
	if (!TestNotNull(TEXT("Existing toy catalogue"), Class)) return false;
	TArray<AActor*> Fixtures;
	ON_SCOPE_EXIT { for (auto* Actor : Fixtures) if (IsValid(Actor)) Actor->Destroy(); };
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Owner = World->SpawnActor<ATBCharacter>(ATBCharacter::StaticClass(), FVector(-12000, 0, 2000),
	                                          FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Toy presentation owner"), Owner)) return false;
	Fixtures.Add(Owner);
	Owner->SetActorTickEnabled(false);
	Owner->SetActorEnableCollision(false);
	Owner->GetCharacterMovement()->SetComponentTickEnabled(false);
	auto* Info = World->SpawnActor<ATBPlayerState>();
	if (!TestNotNull(TEXT("Toy fixture state"), Info)) return false;
	Fixtures.Add(Info);
	Info->Team = ETBTeam::Toy;
	Owner->SetPlayerState(Info);
	Spawn.Owner = Owner;
	int32 TestedMeshes = 0;
	for (auto VisualClass : Class->GetDefaultObject<ATBToyCharacter>()->ToyCatalogue)
	{
		auto* Visual = World->SpawnActor<AActor>(VisualClass, Spawn);
		if (!TestNotNull(TEXT("Catalogue visual"), Visual)) return false;
		Fixtures.Add(Visual);
		Visual->SetActorTickEnabled(false);
		TBToyPresentation::Initialize(Visual);
		if (Cast<ATBToyCarVisual>(Visual)) continue; // Car has no skeletal gait; existing appearance coverage remains.
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual);
		for (auto* Part : Parts)
		{
			if (Part->LeaderPoseComponent.IsValid()) continue;
			auto* Anim = Part->GetSingleNodeInstance();
			if (!TestNotNull(TEXT("Initialized single-node locomotion"), Anim)) return false;
			if (!TestNotNull(TEXT("Existing locomotion BlendSpace"), Cast<UBlendSpace>(Anim->GetCurrentAsset()))) return false;
			++TestedMeshes;
			float Input64 = 0, Rate64 = 0, Input50 = 0;
			for (float Speed : {0.f, 50.f, 64.f, 65.f, 66.f, 120.f, 0.f})
			{
				Owner->GetCharacterMovement()->Velocity = FVector(Speed, 0, 0);
				TBToyPresentation::Animate(Visual, Speed, false);
				PoseFrames(Part);
				FVector Input, Filtered;
				Anim->GetBlendSpaceState(Input, Filtered);
				TestTrue(TEXT("Finite, forward cadence"), FMath::IsFinite(Anim->GetPlayRate()) && Anim->GetPlayRate() > 0);
				TestFalse(TEXT("Moving and idle are not paused"), Part->bPauseAnims);
				if (Speed == 0) TestTrue(TEXT("Idle sample preserved"), FMath::IsNearlyZero(Input.X));
				if (Speed == 50) Input50 = Input.X;
				if (Speed == 64) { Input64 = Input.X; Rate64 = Anim->GetPlayRate(); }
				if (Speed == 66)
				{
					TestTrue(TEXT("Walk/run input continuous through 65"), Input.X >= Input64 && Input.X - Input64 < 5.);
					TestTrue(TEXT("Cadence continuous through 65"), FMath::Abs(Anim->GetPlayRate() - Rate64) < .1f);
				}
			}
			Owner->GetCharacterMovement()->Velocity = FVector(-50, 0, 0);
			TBToyPresentation::Animate(Visual, 50, false);
			FVector Input, Filtered;
			Anim->GetBlendSpaceState(Input, Filtered);
			TestTrue(TEXT("Backward travel reverses the same gait"), Anim->GetPlayRate() < 0 && FMath::IsNearlyEqual(float(Input.X), Input50));
			Info->bFrozen = true;
			TBToyPresentation::Animate(Visual, 50, true);
			TestTrue(TEXT("Frozen pose paused"), Part->bPauseAnims);
			Anim = Part->GetSingleNodeInstance();
			TestTrue(TEXT("Frozen selects authored rest sequence"), Anim && Cast<UAnimSequence>(Anim->GetCurrentAsset()));
			if (Anim) TestTrue(TEXT("Frozen rest starts at zero"), FMath::IsNearlyZero(Anim->GetCurrentTime()));
			Info->bFrozen = false;
			TBToyPresentation::Animate(Visual, 50, false);
			Anim = Part->GetSingleNodeInstance();
			TestTrue(TEXT("Thaw restores locomotion"), Anim && Cast<UBlendSpace>(Anim->GetCurrentAsset()) && !Part->bPauseAnims);
			if (Anim) TestTrue(TEXT("Thaw preserves backward playback"), Anim->GetPlayRate() < 0);
		}
	}
	TestTrue(TEXT("Skeletal catalogue entries tested"), TestedMeshes >= 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBHumanLookTest, "ToonStory.NormalAnimation.HumanLook",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBHumanLookTest::RunTest(const FString&)
{
	auto* World = AnimationTestWorld();
	auto* State = World ? World->GetGameState<ATBGameState>() : nullptr;
	if (!TestNotNull(TEXT("Standalone pose fixture world/state"), State)) return false;
	const ETBPhase OriginalPhase = State->Phase;
	const auto OriginalPreparation = State->HidePreparation;
	const double OriginalEnd = State->EndTime;
	TArray<AActor*> Fixtures;
	ON_SCOPE_EXIT
	{
		for (auto* Actor : Fixtures) if (IsValid(Actor)) Actor->Destroy();
		State->Phase = OriginalPhase; State->HidePreparation = OriginalPreparation; State->EndTime = OriginalEnd;
	};
	// Only a presentation fixture: normal Mode start/rematch and actual network delivery remain integration QA.
	State->Phase = ETBPhase::Playing; State->HidePreparation.bValid = true; State->HidePreparation.bHiding = false;
	++State->HidePreparation.Round; State->EndTime = World->GetTimeSeconds() + 600;
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto MakeBody = [&](FVector Location)
	{
		auto* Body = World->SpawnActor<ATBCharacter>(ATBCharacter::StaticClass(), Location, FRotator::ZeroRotator, Spawn);
		if (Body)
		{
			Fixtures.Add(Body); Body->SetActorTickEnabled(false); Body->SetActorEnableCollision(false);
			Body->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
		return Body;
	};
	auto* Source = MakeBody(FVector(-14000, 0, 2000));
	auto* Proxy = MakeBody(FVector(-14500, 0, 2000));
	auto* Controller = World->SpawnActor<AAIController>();
	auto* Player = World->SpawnActor<ATBPlayerState>();
	if (!TestNotNull(TEXT("Source human"), Source) || !TestNotNull(TEXT("Simulated peer human"), Proxy) ||
	    !TestNotNull(TEXT("Pose source controller"), Controller) || !TestNotNull(TEXT("Pose source state"), Player)) return false;
	Fixtures.Add(Controller); Fixtures.Add(Player);
	Player->Team = ETBTeam::Human; Player->SetOwner(Controller); Controller->SetPlayerState(Player);
	Controller->Possess(Source); Source->ApplyState();
	Proxy->SetPlayerState(Player); Proxy->ApplyState(); Proxy->SetRole(ROLE_SimulatedProxy);
	auto* Mesh = Proxy->GetMesh();
	auto* Child = Mesh->GetSkeletalMeshAsset();
	auto* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/ChildAnimation/Child_MM_Idle"));
	auto* Post = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/ChildAnimation/ABP_TBHumanPose.ABP_TBHumanPose_C"));
	if (!TestNotNull(TEXT("Configured human mesh"), Child) || !TestNotNull(TEXT("Existing idle"), Idle) ||
	    !TestNotNull(TEXT("Existing compiled human post-process"), Post)) return false;
	auto Configure = [&]()
	{
		Mesh->SetOverridePostProcessAnimBP(Post, true); Mesh->PlayAnimation(Idle, false);
		Mesh->GetSingleNodeInstance()->SetPosition(0, false); Mesh->GetSingleNodeInstance()->SetPlaying(false);
		Mesh->bPauseAnims = false;
	};
	Configure();
	if (!TestTrue(TEXT("Real linked-input HumanPose is installed"), Cast<UTBHumanPose>(Mesh->GetPostProcessInstance()) &&
	    Mesh->GetPostProcessInstance()->GetLinkedInputPoseNode())) return false;
	const auto& Ref = Child->GetRefSkeleton();
	const int32 Head = Ref.FindBoneIndex(TEXT("head"));
	if (!TestTrue(TEXT("Configured child exposes the required head/neck/torso chain"), Head != INDEX_NONE &&
	    Ref.FindBoneIndex(TEXT("neck_01")) != INDEX_NONE && Ref.FindBoneIndex(TEXT("spine_03")) != INDEX_NONE)) return false;
	FTransform RefHead = FTransform::Identity;
	for (int32 Bone = Head; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone)) RefHead = RefHead * Ref.GetRefBonePose()[Bone];
	const FVector FaceAxis = RefHead.GetRotation().UnrotateVector(FVector::YAxisVector);
	auto Face = [&]() { return Mesh->GetSocketQuaternion(TEXT("head")).RotateVector(FaceAxis).GetSafeNormal(); };
	auto BoneRotations = [&]()
	{
		TArray<FQuat> Result;
		for (FName Bone : {FName(TEXT("spine_03")), FName(TEXT("neck_01")), FName(TEXT("head"))})
			Result.Add(Mesh->GetSocketTransform(Bone, RTS_ParentBoneSpace).GetRotation());
		return Result;
	};
	FTBHumanLookSnapshot Read;
	TestFalse(TEXT("Unreceived peer snapshot does not substitute actor yaw for the real gaze"), Proxy->GetHumanLook(Read));
	PoseFrames(Mesh, 3);
	const auto Natural = BoneRotations();
	auto Publish = [&]()
	{
		Source->UpdateHumanLook(); Proxy->HumanLook = Source->HumanLook; Proxy->OnRep_HumanLook();
	};
	auto Aim = [&](FRotator Rotation)
	{
		Controller->SetControlRotation(Rotation); Publish();
	};
	Aim(FRotator::ZeroRotator); PoseFrames(Mesh, 90);
	TestTrue(TEXT("Ref-pose face basis aligns the configured child with actual forward gaze"), FVector::DotProduct(Face(), FVector::ForwardVector) > .995);
	TestTrue(TEXT("Simulated peer has no controller"), Proxy->GetController() == nullptr);
	TestTrue(TEXT("Authority samples exactly the same ControlRotation used by gaze judging"), Source->GetHumanLook(Read) && Read.View.Equals(Controller->GetControlRotation(), .01));
	Aim(FRotator(0, 45, 0)); PoseFrames(Mesh);
	const double FirstStep = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Face(), FVector::ForwardVector), -1., 1.)));
	TestTrue(TEXT("First look step is visible but interpolated rather than a full snap"), FirstStep > 1 && FirstStep < 44);
	PoseFrames(Mesh, 90);
	TestTrue(TEXT("Peer head follows yaw independently of an unchanged actor yaw"),
	    FMath::Abs(Proxy->GetActorRotation().Yaw) < .01 && FVector::DotProduct(Face(), FRotator(0, 45, 0).Vector()) > .995);
	const auto Turned = BoneRotations();
	TestTrue(TEXT("Torso visibly contributes to the turn"), Turned[0].AngularDistance(Natural[0]) > .03);
	TestTrue(TEXT("Neck visibly contributes to the turn"), Turned[1].AngularDistance(Natural[1]) > .03);
	for (const FRotator& Direction : {FRotator(0, -45, 0), FRotator(35, 0, 0), FRotator(-45, 0, 0), FRotator(-30, 35, 0)})
	{
		Aim(Direction); PoseFrames(Mesh, 90);
		TestTrue(FString::Printf(TEXT("Evaluated head face follows left/right/up/down %s"), *Direction.ToCompactString()),
		    FVector::DotProduct(Face(), Direction.Vector()) > .995);
	}
	Proxy->SetActorRotation(FRotator(0, 170, 0)); Aim(FRotator(0, 179, 0)); PoseFrames(Mesh, 120);
	TestTrue(TEXT("Near-wrap world gaze is reached with a matching body heading"), FVector::DotProduct(Face(), FRotator(0, 179, 0).Vector()) > .995);
	Aim(FRotator(0, -179, 0)); PoseFrames(Mesh);
	const double WrapStep = FMath::FindDeltaAngleDegrees(179., Face().Rotation().Yaw);
	TestTrue(TEXT("179 to -179 interpolates the short two-degree path"), WrapStep > 0 && WrapStep < 2);
	PoseFrames(Mesh, 90);
	TestTrue(TEXT("Wrapped gaze converges without a full spin"), FVector::DotProduct(Face(), FRotator(0, -179, 0).Vector()) > .995);
	Proxy->SetActorRotation(FRotator::ZeroRotator); Aim(FRotator(0, 45, 0)); PoseFrames(Mesh, 120);
	for (double BodyYaw : {10., 25., 45., 65.})
	{
		Proxy->SetActorRotation(FRotator(0, BodyYaw, 0)); PoseFrames(Mesh);
		TestTrue(TEXT("Root following the view does not drag the head away from its world gaze"),
		    FVector::DotProduct(Face(), FRotator(0, 45, 0).Vector()) > .995);
	}
	Proxy->SetActorRotation(FRotator::ZeroRotator);
	Aim(FRotator(89, 179, 0)); PoseFrames(Mesh, 120);
	const auto Limited = BoneRotations();
	const double MaxAngle[] = {19., 28.5, 47.5};
	for (int32 Part = 0; Part < 3; ++Part)
		TestTrue(TEXT("Each joint remains inside its bounded correction"), FMath::RadiansToDegrees(Limited[Part].AngularDistance(Natural[Part])) <= MaxAngle[Part] + .1);
	TestTrue(TEXT("Extreme view remains a finite anatomically bounded face direction"), !Face().ContainsNaN() &&
	    FMath::Abs(Face().Rotation().Yaw) <= 86 && FMath::Abs(Face().Rotation().Pitch) <= 76);
	Aim(FRotator(0, 30, 0)); Proxy->bIsCrouched = true; PoseFrames(Mesh, 90);
	TestTrue(TEXT("Crouch leg IK preserves head gaze"), FVector::DotProduct(Face(), FRotator(0, 30, 0).Vector()) > .995);
	Proxy->bIsCrouched = false;
	auto* Toy = World->SpawnActor<ATBDummyToy>();
	if (!TestNotNull(TEXT("Carry/look fixture"), Toy)) return false;
	Fixtures.Add(Toy); Toy->SetActorTickEnabled(false); Toy->SetActorEnableCollision(false);
	Toy->Visual->SetChildActorClass(ATBToyCarVisual::StaticClass()); Proxy->CarriedDummy = Toy;
	FVector Left, Right;
	if (!TestTrue(TEXT("Real carried prop exposes both contact targets"), TBToyPresentation::CarryContacts(Proxy, Left, Right))) return false;
	Aim(FRotator(-20, 35, 0)); PoseFrames(Mesh, 90);
	TestTrue(TEXT("Carry IK does not erase head direction"), FVector::DotProduct(Face(), FRotator(-20, 35, 0).Vector()) > .995);
	TestTrue(TEXT("Arms keep left/right palm contacts after torso look correction"),
	    FVector::Distance(Mesh->GetSocketLocation(TEXT("middle_01_l")), Left) < 8 &&
	    FVector::Distance(Mesh->GetSocketLocation(TEXT("middle_01_r")), Right) < 8);
	Toy->Visual->SetChildActorClass(nullptr); PoseFrames(Mesh, 60);
	TestTrue(TEXT("Missing carry contacts cannot skip valid head look"), FVector::DotProduct(Face(), FRotator(-20, 35, 0).Vector()) > .995);
	Proxy->CarriedDummy = nullptr;
	Player->bFrozen = true; Aim(FRotator(20, -30, 0)); PoseFrames(Mesh, 90);
	TestTrue(TEXT("A valid human gaze remains readable with a frozen flag; it does not use toy pause rules"),
	    FVector::DotProduct(Face(), FRotator(20, -30, 0).Vector()) > .995);
	Player->bFrozen = false;
	auto NaturalAgain = [&]()
	{
		PoseFrames(Mesh, 3); const auto Current = BoneRotations();
		for (int32 Part = 0; Part < 3; ++Part)
			TestTrue(TEXT("Invalid look cannot retain previous joint correction"), Current[Part].AngularDistance(Natural[Part]) < .001);
	};
	Player->Team = ETBTeam::Toy; Player->ToyState = ETBToyState::Boxed;
	TestFalse(TEXT("Role change including storage invalidates old human snapshot"), Proxy->GetHumanLook(Read)); NaturalAgain();
	Player->Team = ETBTeam::Human; Publish(); PoseFrames(Mesh, 90);
	State->HidePreparation.Round++; TestFalse(TEXT("New round rejects a previous-round view"), Proxy->GetHumanLook(Read)); NaturalAgain();
	Publish(); PoseFrames(Mesh, 90);
	State->HidePreparation.bHiding = true; TestFalse(TEXT("Preparation cannot reuse a previous gaze"), Proxy->GetHumanLook(Read)); NaturalAgain();
	State->HidePreparation.bHiding = false; Publish(); PoseFrames(Mesh, 90);
	const auto Previous = Source->HumanLook;
	Controller->UnPossess(); Proxy->HumanLook = Source->HumanLook; Proxy->OnRep_HumanLook();
	TestFalse(TEXT("Unpossess immediately publishes invalid view"), Source->HumanLook.bValid);
	TestFalse(TEXT("Received unpossess invalidates peer look"), Proxy->GetHumanLook(Read)); NaturalAgain();
	Proxy->HumanLook = Previous; Proxy->OnRep_HumanLook();
	TestFalse(TEXT("An older possession snapshot cannot revive the previous view"), Proxy->GetHumanLook(Read));
	Controller->Possess(Source); Aim(FRotator(0, 45, 0));
	TestTrue(TEXT("Repossess has a distinct valid generation"), Source->HumanLook.bValid && Source->HumanLook.Possession != Previous.Possession);
	PoseFrames(Mesh); const double NewFirstStep = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Face(), FVector::ForwardVector), -1., 1.)));
	TestTrue(TEXT("Repossess starts fresh interpolation rather than retaining the previous aim"), NewFirstStep > 1 && NewFirstStep < 44);
	PoseFrames(Mesh, 90);
	Proxy->HumanLook.Player = nullptr; TestFalse(TEXT("Unresolved snapshot player stays invalid"), Proxy->GetHumanLook(Read)); NaturalAgain();
	auto* OtherPlayer = World->SpawnActor<ATBPlayerState>();
	if (!TestNotNull(TEXT("Unrelated player reference fixture"), OtherPlayer)) return false;
	Fixtures.Add(OtherPlayer); OtherPlayer->Team = ETBTeam::Human; Proxy->HumanLook.Player = OtherPlayer;
	TestFalse(TEXT("Resolved but mismatched player reference cannot supply another human's look"), Proxy->GetHumanLook(Read));
	Publish(); State->EndTime = 0;
	TestFalse(TEXT("Unstarted deadline cannot revive a stale human view"), Proxy->GetHumanLook(Read));
	State->EndTime = State->GetServerWorldTimeSeconds() - 1;
	TestFalse(TEXT("Expired server deadline invalidates the human view"), Proxy->GetHumanLook(Read));
	State->EndTime = State->GetServerWorldTimeSeconds() + 600;
	Publish(); State->Phase = ETBPhase::Results; TestFalse(TEXT("Results invalidates the old gaze"), Proxy->GetHumanLook(Read)); NaturalAgain();
	State->Phase = ETBPhase::Playing; Publish();
	// Native post-process on an incompatible mesh: no guessed head-axis or old look survives the asset swap.
	auto* Duck = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/ToyCollectionRealistic/Duck/SK_Duck"));
	auto* DuckIdle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/ToyCollectionRealistic/Duck/AN_Idle"));
	if (!TestNotNull(TEXT("Existing missing-chain fallback mesh"), Duck) || !TestNotNull(TEXT("Matching fallback idle"), DuckIdle)) return false;
	Mesh->SetOverridePostProcessAnimBP(UTBHumanPose::StaticClass(), false); Mesh->SetSkeletalMesh(Duck); Mesh->PlayAnimation(DuckIdle, false);
	Mesh->GetSingleNodeInstance()->SetPlaying(false); PoseFrames(Mesh, 3);
	for (const auto& Bone : Mesh->GetComponentSpaceTransforms()) TestFalse(TEXT("Missing human chain keeps finite fallback pose"), Bone.ContainsNaN());
	Mesh->SetSkeletalMesh(nullptr); Mesh->SetSkeletalMesh(Child); Configure(); PoseFrames(Mesh, 90);
	TestTrue(TEXT("Restored mesh derives a fresh face basis and view"), FVector::DotProduct(Face(), FRotator(0, 45, 0).Vector()) > .995);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBEmoteTest, "ToonStory.NormalAnimation.Emotes",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBEmoteTest::RunTest(const FString&)
{
	auto* World = AnimationTestWorld(); auto* State = World ? World->GetGameState<ATBGameState>() : nullptr;
	if (!TestNotNull(TEXT("Isolated standalone emote fixture world"), State)) return false;
	const auto OriginalPhase = State->Phase; const auto OriginalHide = State->HidePreparation; const double OriginalEnd = State->EndTime;
	TArray<AActor*> Fixtures;
	ON_SCOPE_EXIT
	{
		for (auto* Actor : Fixtures) if (IsValid(Actor)) Actor->Destroy();
		State->Phase = OriginalPhase; State->HidePreparation = OriginalHide; State->EndTime = OriginalEnd;
	};
	State->Phase = ETBPhase::Playing; State->HidePreparation.bValid = true; State->HidePreparation.bHiding = false;
	++State->HidePreparation.Round; State->EndTime = State->GetServerWorldTimeSeconds() + 600;
	auto* Floor = World->SpawnActor<AActor>(); Fixtures.Add(Floor);
	auto* Surface = NewObject<UBoxComponent>(Floor); Floor->SetRootComponent(Surface);
	Surface->SetBoxExtent(FVector(600, 400, 5)); Surface->SetCollisionProfileName(TEXT("BlockAll")); Surface->RegisterComponent();
	Floor->SetActorLocation(FVector(-16000, 0, 1900));
	auto* Controller = World->SpawnActor<AAIController>(); auto* Player = World->SpawnActor<ATBPlayerState>();
	if (!Controller || !Player) return false;
	Fixtures.Add(Controller); Fixtures.Add(Player); Controller->SetPlayerState(Player); Player->SetOwner(Controller);
	State->AddPlayerState(Player);
	Player->Team = ETBTeam::Human;
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto MakeBody = [&](UClass* Class, double X)
	{
		auto* Body = World->SpawnActor<ATBCharacter>(Class, FVector(X, 0, 2000), FRotator::ZeroRotator, Spawn);
		if (Body)
		{
			Fixtures.Add(Body); Body->SetActorTickEnabled(false); Body->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
		return Body;
	};
	auto Stand = [&](ATBCharacter* Body)
	{
		Body->SetActorLocation(FVector(Body->GetActorLocation().X, 0, 1905 + Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.15));
		Body->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Body->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	};
	auto* Source = MakeBody(ATBCharacter::StaticClass(), -15900); auto* Proxy = MakeBody(ATBCharacter::StaticClass(), -16200);
	if (!Source || !Proxy) return false;
	Controller->Possess(Source); Proxy->SetPlayerState(Player); Source->ApplyState(); Proxy->ApplyState();
	Stand(Source); Stand(Proxy); Proxy->SetRole(ROLE_SimulatedProxy);
	auto* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/ChildAnimation/Child_MM_Idle"));
	auto* Post = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/ChildAnimation/ABP_TBHumanPose.ABP_TBHumanPose_C"));
	if (!TestNotNull(TEXT("Existing human idle"), Idle) || !TestNotNull(TEXT("Existing compiled human post-process"), Post)) return false;
	auto* HumanMesh = Proxy->GetMesh(); HumanMesh->SetOverridePostProcessAnimBP(Post, true); HumanMesh->PlayAnimation(Idle, false);
	HumanMesh->GetSingleNodeInstance()->SetPlaying(false); HumanMesh->GetSingleNodeInstance()->SetPosition(0, false);
	Controller->SetControlRotation(FRotator(-15, 30, 0)); Source->UpdateHumanLook(); Proxy->HumanLook = Source->HumanLook; Proxy->OnRep_HumanLook();
	PoseFrames(HumanMesh, 90); const FHands HumanRest = Hands(HumanMesh);
	const auto& HumanRef = HumanMesh->GetSkeletalMeshAsset()->GetRefSkeleton(); FTransform HeadRef = FTransform::Identity;
	for (int32 Bone = HumanRef.FindBoneIndex(TEXT("head")); Bone != INDEX_NONE; Bone = HumanRef.GetParentIndex(Bone)) HeadRef = HeadRef * HumanRef.GetRefBonePose()[Bone];
	const FVector FaceAxis = HeadRef.GetRotation().UnrotateVector(FVector::YAxisVector);
	FTBEmoteSample Sample;
	// Synchronous pose/state test: payload delivery and elapsed time are injected, never reported as network/input proof.
	auto Start = [&](ETBEmoteKind Kind)
	{
		Source->CancelEmote(); Source->Emote.NextAllowedAt = 0; Source->LastEmoteCancelFrame = MAX_uint64;
		Source->GetCharacterMovement()->Velocity = FVector::ZeroVector; Source->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Source->UpdateHumanLook();
		return TestTrue(TEXT("Eligible authority accepts a fresh emote"), Source->TryEmote(Kind));
	};
	auto Deliver = [&](float Progress = .31f)
	{
		Proxy->HumanLook = Source->HumanLook; Proxy->OnRep_HumanLook();
		Proxy->Emote = Source->Emote;
		const double Duration = Proxy->Emote.EndsAt - Proxy->Emote.StartsAt;
		Proxy->Emote.StartsAt = State->GetServerWorldTimeSeconds() - Duration * Progress;
		Proxy->Emote.EndsAt = Proxy->Emote.StartsAt + Duration; Proxy->OnRep_Emote();
	};
	for (ETBEmoteKind Kind : {ETBEmoteKind::Wave, ETBEmoteKind::Cheer, ETBEmoteKind::Dance, ETBEmoteKind::WildDance})
	{
		if (!Start(Kind)) return false;
		const FTransform Root = Proxy->GetActorTransform(); const double Half = Proxy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Deliver(); TestTrue(TEXT("Peer reconstructs active elapsed emote"), Proxy->GetEmoteSample(Sample) && Sample.Kind == Kind && FMath::IsNearlyEqual(Sample.Progress, .31f, .001f));
		PoseFrames(HumanMesh, 90);
		TestTrue(TEXT("Human arms/body visibly differ from ordinary idle"), PoseDistance(Hands(HumanMesh), HumanRest) > .5);
		TestTrue(TEXT("Real human view direction survives emote composition"), FVector::DotProduct(HumanMesh->GetSocketQuaternion(TEXT("head")).RotateVector(FaceAxis), Controller->GetControlRotation().Vector()) > .995);
		TestTrue(TEXT("Emote cannot move the actor or change its capsule"), Proxy->GetActorTransform().Equals(Root, .001) && Proxy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() == Half);
		Source->CancelEmote(); Proxy->Emote = Source->Emote; Proxy->OnRep_Emote(); PoseFrames(HumanMesh, 4);
		TestTrue(TEXT("Cancellation restores the unchanged normal base pose"), PoseDistance(Hands(HumanMesh), HumanRest) < .05);
	}
	if (!Start(ETBEmoteKind::Dance)) return false;
	Source->CrouchPressed();
	auto* Move = Source->GetCharacterMovement();
	TestTrue(TEXT("Dance allows the normal crouch input"), Move->bWantsToCrouch != 0);
	TestFalse(TEXT("A pending crouch immediately suppresses the emote pose"), Source->GetEmoteSample(Sample));
	Move->Crouch(false);
	TestTrue(TEXT("The character crouches and cancels the dance"), Source->bIsCrouched && !Source->Emote.bActive);
	Source->Emote.NextAllowedAt = 0; Source->LastEmoteCancelFrame = MAX_uint64;
	TestFalse(TEXT("A crouched character cannot start an emote"), Source->TryEmote(ETBEmoteKind::Wave));
	Source->CrouchReleased(); Move->UnCrouch(false);
	TestFalse(TEXT("Standing again does not resume the dance"), Source->Emote.bActive);
	Start(ETBEmoteKind::Wave); const double Cooldown = Source->Emote.NextAllowedAt;
	TestFalse(TEXT("Invalid ID is rejected"), Source->TryEmote(static_cast<ETBEmoteKind>(255)));
	TestFalse(TEXT("Active/cooldown request is rejected"), Source->TryEmote(ETBEmoteKind::Dance));
	TestEqual(TEXT("Rejected requests never extend cooldown"), Source->Emote.NextAllowedAt, Cooldown);
	TestFalse(TEXT("A proxy cannot authoritatively start an emote"), Proxy->TryEmote(ETBEmoteKind::Wave));
	Deliver(); Proxy->GetCharacterMovement()->Velocity = FVector(3, 0, 0);
	Proxy->UpdateEmote();
	TestFalse(TEXT("First payload waits for replicated stop"), Proxy->GetEmoteSample(Sample));
	Proxy->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	TestTrue(TEXT("Replicated stop allows the unseen revision to start"), Proxy->GetEmoteSample(Sample));
	Deliver(); TestTrue(TEXT("Peer initially presents the active revision"), Proxy->GetEmoteSample(Sample));
	Proxy->GetCharacterMovement()->Velocity = FVector(3, 0, 0); TestFalse(TEXT("Local movement immediately stops presentation"), Proxy->GetEmoteSample(Sample));
	Proxy->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	TestFalse(TEXT("Standing again cannot revive locally cancelled revision"), Proxy->GetEmoteSample(Sample));
	Source->GetCharacterMovement()->Velocity = FVector(3, 0, 0); Source->UpdateEmote();
	TestFalse(TEXT("Authority movement publishes cancellation"), Source->Emote.bActive);
	Source->GetCharacterMovement()->Velocity = FVector::ZeroVector; Source->Emote.NextAllowedAt = 0;
	TestFalse(TEXT("A same-frame cancellation cannot immediately restart"), Source->TryEmote(ETBEmoteKind::Wave));
	Start(ETBEmoteKind::Cheer); Deliver(); const auto OldStart = Proxy->Emote;
	Source->CancelEmote(); Proxy->Emote = Source->Emote; Proxy->OnRep_Emote();
	Proxy->Emote = OldStart; Proxy->OnRep_Emote(); TestFalse(TEXT("Delayed old start cannot override cancellation watermark"), Proxy->GetEmoteSample(Sample));
	Start(ETBEmoteKind::Dance); Deliver(); Proxy->HumanLook.Possession = 0;
	TestFalse(TEXT("A new payload waits for possession metadata"), Proxy->GetEmoteSample(Sample));
	Proxy->HumanLook = Source->HumanLook; Proxy->OnRep_HumanLook();
	TestTrue(TEXT("Reordered metadata before first presentation is deferred, not permanently cancelled"), Proxy->GetEmoteSample(Sample));
	Proxy->Emote.StartsAt = State->GetServerWorldTimeSeconds() - 3.1; Proxy->Emote.EndsAt = State->GetServerWorldTimeSeconds() - .1;
	TestFalse(TEXT("Late expired emote is not replayed"), Proxy->GetEmoteSample(Sample));
	Start(ETBEmoteKind::Wave); Deliver();
	Proxy->GetEmoteSample(Sample);
	Player->bFrozen = true; Proxy->UpdateEmote(); // No pose evaluation/HUD sample between state changes.
	Player->bFrozen = false; Proxy->UpdateEmote();
	TestTrue(TEXT("Server payload is still active in the offscreen cancellation fixture"), Proxy->Emote.bActive);
	TestFalse(TEXT("Remote GT observation prevents freeze/unfreeze revival even without pose evaluation"), Proxy->GetEmoteSample(Sample));
	for (int32 Condition = 0; Condition < 4; ++Condition)
	{
		Start(ETBEmoteKind::Wave); Deliver(); Proxy->GetEmoteSample(Sample);
		if (Condition == 0) Player->bFrozen = true;
		if (Condition == 1) Proxy->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		if (Condition == 2) Player->ToyState = ETBToyState::Carried;
		if (Condition == 2) Player->Team = ETBTeam::Toy;
		if (Condition == 3) State->Phase = ETBPhase::Results;
		TestFalse(TEXT("Freeze/fall/captured role/results takes priority"), Proxy->GetEmoteSample(Sample));
		Player->bFrozen = false; Player->Team = ETBTeam::Human; Player->ToyState = ETBToyState::Free;
		State->Phase = ETBPhase::Playing; Stand(Proxy);
		TestFalse(TEXT("Removing the interruption cannot resurrect the old revision"), Proxy->GetEmoteSample(Sample));
	}
	Start(ETBEmoteKind::Wave); Deliver(); Proxy->GetEmoteSample(Sample);
	++State->HidePreparation.Round; TestFalse(TEXT("Round change invalidates old presentation"), Proxy->GetEmoteSample(Sample));
	Source->UpdateEmote(); TestFalse(TEXT("Authority clears old round"), Source->Emote.bActive);
	Start(ETBEmoteKind::Wave); Deliver(); Proxy->GetEmoteSample(Sample);
	Controller->UnPossess(); Proxy->HumanLook = Source->HumanLook; Proxy->Emote = Source->Emote; Proxy->OnRep_Emote();
	TestFalse(TEXT("Unpossess cancels authority state"), Source->Emote.bActive);
	TestFalse(TEXT("Unpossess payload cancels peer state"), Proxy->GetEmoteSample(Sample));
	Controller->Possess(Source); Stand(Source);
	auto* Carried = World->SpawnActor<ATBDummyToy>(); Fixtures.Add(Carried);
	Start(ETBEmoteKind::Cheer); Source->CarriedDummy = Carried; Source->UpdateEmote();
	TestFalse(TEXT("Starting to carry cancels an emote"), Source->Emote.bActive);
	Source->LastEmoteCancelFrame = MAX_uint64; Source->Emote.NextAllowedAt = 0;
	TestFalse(TEXT("Carrying cannot start a new emote"), Source->TryEmote(ETBEmoteKind::Cheer)); Source->CarriedDummy = nullptr;
	auto* HumanAsset = Source->GetMesh()->GetSkeletalMeshAsset();
	Source->GetMesh()->SetSkeletalMesh(nullptr); Source->LastEmoteCancelFrame = MAX_uint64;
	TestFalse(TEXT("Missing model is explicitly unsupported"), Source->SupportsEmotes());
	TestFalse(TEXT("Unsupported presentation is not accepted as a silent emote success"), Source->TryEmote(ETBEmoteKind::Wave));
	Source->GetMesh()->SetSkeletalMesh(HumanAsset);
	// Every existing catalogue entry must be mapped or explicitly fail this acceptance test, never silently succeed.
	auto* ToyClass = LoadClass<ATBToyCharacter>(nullptr, TEXT("/Game/Characters/Toys/BP_ToyCharacter.BP_ToyCharacter_C"));
	if (!TestNotNull(TEXT("Existing toy catalogue"), ToyClass)) return false;
	Player->Team = ETBTeam::Toy;
	auto* ToySource = Cast<ATBToyCharacter>(MakeBody(ToyClass, -15900));
	auto* ToyProxy = Cast<ATBToyCharacter>(MakeBody(ToyClass, -16200));
	if (!ToySource || !ToyProxy) return false;
	Controller->Possess(ToySource); ToyProxy->SetPlayerState(Player); ToyProxy->SetRole(ROLE_SimulatedProxy);
	Source = ToySource; Proxy = ToyProxy;
	for (int32 Index = 0; Index < ToySource->ToyCatalogue.Num(); ++Index)
	{
		ToySource->SelectedToyIndex = ToyProxy->SelectedToyIndex = Index;
		ToySource->RefreshToyPresentation(); ToyProxy->RefreshToyPresentation(); Stand(Source); Stand(Proxy);
		TestTrue(TEXT("Catalogue visual has explicit emote support"), Source->SupportsEmotes());
		auto* Visual = ToyProxy->ToyVisual->GetChildActor();
		if (!TestNotNull(TEXT("Catalogue visual instance"), Visual)) return false;
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual);
		for (auto* Part : Parts)
			if (auto* Anim = Part->GetSingleNodeInstance()) { Anim->SetPlaying(false); Anim->SetPosition(0, false); }
		for (ETBEmoteKind Kind : {ETBEmoteKind::Wave, ETBEmoteKind::Cheer, ETBEmoteKind::Dance, ETBEmoteKind::WildDance})
		{
			TMap<USkeletalMeshComponent*, TArray<FTransform>> RestPoses;
			for (auto* Part : Parts)
				if (!Part->LeaderPoseComponent.IsValid() && Part->GetSkeletalMeshAsset())
				{ PoseFrames(Part, 2); RestPoses.Add(Part, Part->GetComponentSpaceTransforms()); }
			if (!Start(Kind)) return false;
			if (Kind == ETBEmoteKind::Wave)
			{
				ToyProxy->ToyVisual->SetChildActorClass(nullptr);
				Deliver(); Proxy->UpdateEmote();
				TestFalse(TEXT("First payload waits while runtime child visual is missing"), Proxy->GetEmoteSample(Sample));
				ToyProxy->RefreshToyPresentation(); Visual = ToyProxy->ToyVisual->GetChildActor();
				if (!TestNotNull(TEXT("Delayed child visual is recreated"), Visual)) return false;
				TestTrue(TEXT("Same revision resumes at elapsed time after visual preparation"), Proxy->GetEmoteSample(Sample));
				Parts.Reset(); Visual->GetComponents(Parts); RestPoses.Reset();
				// Obtain the recreated visual's normal baseline without publishing another emote revision.
				const auto Active = Proxy->Emote; Proxy->Emote.bActive = false;
				for (auto* Part : Parts)
					if (!Part->LeaderPoseComponent.IsValid() && Part->GetSkeletalMeshAsset())
					{
						if (auto* Anim = Part->GetSingleNodeInstance()) { Anim->SetPlaying(false); Anim->SetPosition(0, false); }
						PoseFrames(Part, 2); RestPoses.Add(Part, Part->GetComponentSpaceTransforms());
					}
				Proxy->Emote = Active;
			}
			Deliver(); const FTransform ActorBefore = Proxy->GetActorTransform();
			if (auto* Car = Cast<ATBToyCarVisual>(Visual))
			{
				const FTransform Rest = Car->Car->GetRelativeTransform(); Car->Animate(0, false);
				TestFalse(TEXT("Car body has a visible emote rotation"), Car->Car->GetRelativeTransform().Equals(Rest, .001));
				Source->CancelEmote(); Proxy->Emote = Source->Emote; Proxy->OnRep_Emote(); Car->Animate(0, false);
				TestTrue(TEXT("Car returns exactly to its saved baseline"), Car->Car->GetRelativeTransform().Equals(Rest, .001));
			}
			else
			{
				int32 Mapped = 0;
				for (auto* Part : Parts)
				{
					if (Part->LeaderPoseComponent.IsValid() || !Part->GetSkeletalMeshAsset()) continue;
					FTBEmotePose Pose; TBToyPresentation::BuildEmotePose(Part, Proxy, Pose);
					TestTrue(TEXT("Current skeletal model maps a semantic emote"), !Pose.Bones.IsEmpty());
					if (Part->GetSkeletalMeshAsset()->GetName() == TEXT("SK_Duck"))
						TestTrue(TEXT("Duck emote drives the bone weighted to its surface"),
						    Pose.Bones.ContainsByPredicate([](const FTBEmoteBone& Bone) { return Bone.Name == TEXT("pelvis"); }));
					TestNotNull(TEXT("Toy's existing post-process really uses the native facing/emote proxy"), Cast<UTBToyFacingPose>(Part->GetPostProcessInstance()));
					++Mapped; PoseFrames(Part, 2);
					const auto& Rest = RestPoses.FindChecked(Part);
					const auto& Moving = Part->GetComponentSpaceTransforms();
					bool Changed = false;
					for (int32 Bone = 1; Bone < FMath::Min(Rest.Num(), Moving.Num()); ++Bone)
						Changed |= Moving[Bone].GetRotation().AngularDistance(Rest[Bone].GetRotation()) > .001;
					TestTrue(TEXT("Evaluated toy bones visibly change, not only the snapshot ID"), Changed);
					TestTrue(TEXT("Emote never changes the skeleton root"), !Rest.IsEmpty() && Rest[0].Equals(Moving[0], .001));
					for (const auto& Bone : Part->GetComponentSpaceTransforms()) TestFalse(TEXT("Emote pose remains finite"), Bone.ContainsNaN());
				}
				TestTrue(TEXT("Skeletal catalogue visual was actually tested"), Mapped > 0);
				Source->CancelEmote(); Proxy->Emote = Source->Emote; Proxy->OnRep_Emote();
				for (auto* Part : Parts)
					if (const auto* Rest = RestPoses.Find(Part))
					{
						PoseFrames(Part, 2); const auto& Current = Part->GetComponentSpaceTransforms();
						bool Restored = Rest->Num() == Current.Num();
						for (int32 Bone = 0; Restored && Bone < Current.Num(); ++Bone) Restored &= (*Rest)[Bone].Equals(Current[Bone], .001);
						TestTrue(TEXT("Each model restores its ordinary pose after cancellation"), Restored);
					}
			}
			TestTrue(TEXT("Toy emote never changes gameplay actor transform"), Proxy->GetActorTransform().Equals(ActorBefore, .001));
		}
		Start(ETBEmoteKind::Dance); Deliver(); Proxy->GetEmoteSample(Sample);
		ToyProxy->SelectedToyIndex = INDEX_NONE; TestFalse(TEXT("Changing toy visual invalidates the old emote"), Proxy->GetEmoteSample(Sample));
		ToyProxy->SelectedToyIndex = Index; TestFalse(TEXT("Restoring the visual cannot revive that revision"), Proxy->GetEmoteSample(Sample));
	}
	// Execute the real Started delegates on an owned pawn. This is not physical keyboard/network delivery.
	auto* LocalPC = Cast<ATBController>(World->GetFirstPlayerController());
	auto* LocalPS = LocalPC ? LocalPC->GetPlayerState<ATBPlayerState>() : nullptr;
	if (!TestNotNull(TEXT("Local player for emote input routing"), LocalPS) ||
	    !TestTrue(TEXT("Input fixture begins with menu/chat closed"), !LocalPC->IsChatOpen() && !LocalPC->IsLeaveMenuOpen())) return false;
	const TWeakObjectPtr<APawn> PreviousPawn = LocalPC->GetPawn();
	const auto SavedTeam = LocalPS->Team; const auto SavedToyState = LocalPS->ToyState; const bool SavedFrozen = LocalPS->bFrozen;
	const FRotator SavedView = LocalPC->GetControlRotation();
	auto* Input = NewObject<UEnhancedInputComponent>(Source); Input->RegisterComponent();
	ON_SCOPE_EXIT
	{
		if (IsValid(Input)) Input->DestroyComponent();
		if (IsValid(Source)) Source->CancelEmote();
		if (IsValid(LocalPC))
		{
			LocalPC->CloseChat();
			if (LocalPC->IsLeaveMenuOpen()) LocalPC->ToggleLeaveMenu();
			if (IsValid(LocalPS)) { LocalPS->Team = SavedTeam; LocalPS->ToyState = SavedToyState; LocalPS->bFrozen = SavedFrozen; }
			if (PreviousPawn.IsValid()) LocalPC->Possess(PreviousPawn.Get()); else LocalPC->UnPossess();
			LocalPC->SetControlRotation(SavedView);
			LocalPC->ClientPreparationRole(OriginalHide.Round, SavedTeam);
		}
		if (IsValid(Controller) && IsValid(Source)) Controller->Possess(Source);
	};
	LocalPS->Team = ETBTeam::Toy; LocalPS->ToyState = ETBToyState::Free; LocalPS->bFrozen = false;
	LocalPC->Possess(Source); ToySource->SelectedToyIndex = 0; Source->ApplyState(); Stand(Source);
	Source->SetupPlayerInputComponent(Input);
	LocalPC->ClientPreparationRole(State->HidePreparation.Round, ETBTeam::Toy);
	LocalPC->PlayerTick(0); // Create the ordinary chat widget through the existing local-player path.
	const auto* InputSettings = GetDefault<UTBInputSettings>();
	const TSoftObjectPtr<UInputAction> Actions[] = {InputSettings->EmoteWaveAction, InputSettings->EmoteCheerAction, InputSettings->EmoteDanceAction};
	const ETBEmoteKind Kinds[] = {ETBEmoteKind::Wave, ETBEmoteKind::Cheer, ETBEmoteKind::Dance};
	const FName Names[] = {TB::Keys::EmoteWave, TB::Keys::EmoteCheer, TB::Keys::EmoteDance};
	auto ResetInputEmote = [&]()
	{
		Source->CancelEmote(); Source->Emote.NextAllowedAt = 0; Source->LastEmoteCancelFrame = MAX_uint64;
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
	{
		const UInputAction* Action = Actions[Index].LoadSynchronous();
		if (!TestNotNull(TEXT("Configured emote action"), Action)) return false;
		TB::FKeyboardKey Row;
		if (!TestTrue(TEXT("Input fixture has a non-conflicting saved/default emote key"), TB::FindKeyboardKey(LocalPC, Names[Index], Row) &&
		    TB::CheckRemap(LocalPC, Names[Index], Row.Key).Result == TB::ERemapResult::OK)) return false;
		const FKey SavedKey = Row.Key;
		auto Dispatch = [&]()
		{
			int32 Count = 0;
			for (const auto& Binding : Input->GetActionEventBindings())
				if (Binding->GetAction() == Action && Binding->GetTriggerEvent() == ETriggerEvent::Started)
				{ Binding->Execute(FInputActionInstance(Action)); ++Count; }
			return TestEqual(TEXT("Exactly one Started delegate handles the emote"), Count, 1);
		};
		ResetInputEmote();
		if (!Dispatch()) return false;
		if (!TestTrue(TEXT("Started action routes to its exact server-authorized emote kind"), Source->Emote.bActive && Source->Emote.Kind == Kinds[Index])) return false;
		const uint32 AcceptedRevision = Source->Emote.Revision;
		const double AcceptedCooldown = Source->Emote.NextAllowedAt;
		Dispatch();
		TestTrue(TEXT("Repeated Started cannot restart an active emote or extend cooldown"),
		    Source->Emote.Revision == AcceptedRevision && Source->Emote.NextAllowedAt == AcceptedCooldown);
		ResetInputEmote(); LocalPC->ToggleLeaveMenu(); Dispatch();
		TestFalse(TEXT("Menu consumes the emote shortcut without starting it"), Source->Emote.bActive); LocalPC->ToggleLeaveMenu();
		// Ordinary chat is intentionally unavailable during Playing.
		State->Phase = ETBPhase::Lobby;
		LocalPC->PlayerTick(0);
		LocalPC->OpenChat();
		if (!TestTrue(TEXT("Ordinary chat is open for the negative input case"), LocalPC->IsChatOpen())) return false;
		Dispatch(); TestFalse(TEXT("Typing a digit in chat cannot emote"), Source->Emote.bActive); LocalPC->CloseChat();
		State->Phase = ETBPhase::Playing;
		LocalPC->PlayerTick(0);
		State->HidePreparation.bHiding = true; Dispatch(); TestFalse(TEXT("Preparation blocks every emote shortcut"), Source->Emote.bActive);
		State->HidePreparation.bHiding = false;
		LocalPS->bFrozen = true; Dispatch(); TestFalse(TEXT("Frozen pawn cannot emote from input"), Source->Emote.bActive); LocalPS->bFrozen = false;
		LocalPS->ToyState = ETBToyState::Boxed; Dispatch(); TestFalse(TEXT("Boxed pawn cannot emote from input"), Source->Emote.bActive); LocalPS->ToyState = ETBToyState::Free;
		Source->GetCharacterMovement()->Velocity = FVector(3, 0, 0); Dispatch();
		TestFalse(TEXT("Moving pawn cannot emote from input"), Source->Emote.bActive); Source->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		State->Phase = ETBPhase::Results; Dispatch(); TestFalse(TEXT("Results blocks the shortcut"), Source->Emote.bActive); State->Phase = ETBPhase::Playing;
		TestTrue(TEXT("The input route never rewrites the saved keyboard profile"), TB::FindKeyboardKey(LocalPC, Names[Index], Row) &&
		    Row.Key == SavedKey);
	}
	ResetInputEmote();
	int32 SecretBindings = 0;
	for (auto& Binding : Input->KeyBindings)
		if (Binding.Chord.Key == EKeys::Zero && Binding.KeyEvent == IE_Pressed)
		{
			++SecretBindings;
			Binding.KeyDelegate.Execute(EKeys::Zero);
		}
	TestEqual(TEXT("One hidden zero-key binding"), SecretBindings, 1);
	TestTrue(TEXT("Zero starts the hidden wild dance"), Source->Emote.bActive && Source->Emote.Kind == ETBEmoteKind::WildDance);
	ResetInputEmote(); LocalPC->ToggleLeaveMenu();
	for (auto& Binding : Input->KeyBindings)
		if (Binding.Chord.Key == EKeys::Zero) Binding.KeyDelegate.Execute(EKeys::Zero);
	TestFalse(TEXT("Hidden dance respects menus"), Source->Emote.bActive);
	LocalPC->ToggleLeaveMenu();
	return !HasAnyErrors();
}
#endif
