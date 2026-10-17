#include "TBToyFacingPose.h"
#include "TBCharacter.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	class FToyFacingProxy : public FAnimInstanceProxy
	{
		FAnimNode_LinkedInputPose* Input = nullptr;
		float StepYaw = 0;

	public:
		explicit FToyFacingProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
		{
		}
		void PreUpdate(UAnimInstance* Instance, float Delta) override
		{
			FAnimInstanceProxy::PreUpdate(Instance, Delta);
			Input = Instance->GetLinkedInputPoseNode();
			auto* Visual = Instance->GetOwningActor();
			const auto* Pawn = Visual ? Cast<ATBCharacter>(Visual->GetOwner()) : nullptr;
			float Desired = 0;
			if (Pawn && !Pawn->IsMovementLocked() && Pawn->GetVelocity().SizeSquared2D() > 1)
			{
				Desired =
				    FMath::FindDeltaAngleDegrees(Pawn->GetActorRotation().Yaw, Pawn->GetVelocity().Rotation().Yaw);
				// Backward travel uses the reversed walk cycle, keeping hip twist within 90 degrees.
				if (Desired > 90)
				{
					Desired -= 180;
				}
				if (Desired < -90)
				{
					Desired += 180;
				}
			}
			// Frozen players must not retain a strafe twist that distinguishes them from decoys.
			StepYaw = Pawn && Pawn->IsMovementLocked() ? 0.f : FMath::FInterpTo(StepYaw, Desired, Delta, 12);
		}
		bool Evaluate(FPoseContext& Output) override
		{
			if (!Input)
			{
				return false;
			}
			Input->Evaluate_AnyThread(Output);
			if (FMath::Abs(StepYaw) < .01f)
			{
				return true;
			}
			auto Bone = [&](FName Name)
			{
				FBoneReference Ref;
				Ref.BoneName = Name;
				Ref.Initialize(Output.Pose.GetBoneContainer());
				return Ref.GetCompactPoseIndex(Output.Pose.GetBoneContainer());
			};
			auto Pelvis = Bone(TEXT("pelvis")), Spine = Bone(TEXT("spine"));
			if (Pelvis == INDEX_NONE)
			{
				Pelvis = Bone(TEXT("Hips"));
			}
			if (Spine == INDEX_NONE)
			{
				Spine = Bone(TEXT("Spine01"));
			}
			if (Pelvis == INDEX_NONE || Spine == INDEX_NONE)
			{
				return true;
			}
			FCSPose<FCompactPose> Before;
			Before.InitPose(Output.Pose);
			const FQuat TorsoRotation = Before.GetComponentSpaceTransform(Spine).GetRotation();
			auto Parent = Output.Pose.GetBoneContainer().GetParentBoneIndex(Pelvis);
			FTransform Hip = Before.GetComponentSpaceTransform(Pelvis);
			Hip.SetRotation(
			    (FQuat(FVector::UpVector, FMath::DegreesToRadians(StepYaw)) * Hip.GetRotation()).GetNormalized());
			Output.Pose[Pelvis] =
			    Parent != INDEX_NONE ? Hip.GetRelativeTransform(Before.GetComponentSpaceTransform(Parent)) : Hip;
			FCSPose<FCompactPose> After;
			After.InitPose(Output.Pose);
			Parent = Output.Pose.GetBoneContainer().GetParentBoneIndex(Spine);
			const FQuat ParentRotation =
			    Parent != INDEX_NONE ? After.GetComponentSpaceTransform(Parent).GetRotation() : FQuat::Identity;
			Output.Pose[Spine].SetRotation((ParentRotation.Inverse() * TorsoRotation).GetNormalized());
			return true;
		}
	};
} // namespace
FAnimInstanceProxy* UTBToyFacingPose::CreateAnimInstanceProxy()
{
	return new FToyFacingProxy(this);
}
void UTBToyFacingPose::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete Proxy;
}
