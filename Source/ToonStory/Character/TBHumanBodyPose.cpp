#include "TBHumanPose.h"
#include "TBCharacter.h"
#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
TArray<FTransform> ComponentRestPose(const FReferenceSkeleton& Skeleton)
{
	TArray<FTransform> Result = Skeleton.GetRefBonePose();
	for (int32 Bone = 1; Bone < Result.Num(); ++Bone)
		Result[Bone] *= Result[Skeleton.GetParentIndex(Bone)];
	return Result;
}

class FBodyPoseProxy : public FAnimInstanceProxy
{
	TWeakObjectPtr<USkeletalMesh> SourceMesh, TargetMesh;
	TArray<FTransform> SourceRest, TargetRest;
	TArray<int32> SourceIndices;
	TArray<FQuat> Rotations;
	int32 Pelvis = INDEX_NONE;
	int32 LowerSpine = INDEX_NONE, Neck = INDEX_NONE, Head = INDEX_NONE;
	float StandWeight = 0;
	FVector Up, Forward;
	bool bPreserveLook = false;
	FVector HipOffset = FVector::ZeroVector;

public:
	using FAnimInstanceProxy::FAnimInstanceProxy;

	virtual void PreUpdate(UAnimInstance* Instance, float Delta) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, Delta);
		auto* Assembly = Instance->GetOwningActor();
		auto* Pawn = Assembly ? Cast<ATBCharacter>(Assembly->GetOwner()) : nullptr;
		auto* Driver = Pawn ? Pawn->GetMesh() : nullptr;
		auto* Source = Driver ? Driver->GetSkeletalMeshAsset() : nullptr;
		auto* Target = Instance->GetSkelMeshComponent()->GetSkeletalMeshAsset();
		Rotations.Reset();
		HipOffset = FVector::ZeroVector;
		if (!Source || !Target) return;
		if (SourceMesh.Get() != Source || TargetMesh.Get() != Target)
		{
			SourceMesh = Source;
			TargetMesh = Target;
			SourceRest = ComponentRestPose(Source->GetRefSkeleton());
			TargetRest = ComponentRestPose(Target->GetRefSkeleton());
			SourceIndices.Reset();
			for (const auto& Bone : Target->GetRefSkeleton().GetRefBoneInfo())
				SourceIndices.Add(Source->GetRefSkeleton().FindBoneIndex(Bone.Name));
			Pelvis = Target->GetRefSkeleton().FindBoneIndex(TEXT("pelvis"));
			LowerSpine = Target->GetRefSkeleton().FindBoneIndex(TEXT("spine_01"));
			Neck = Target->GetRefSkeleton().FindBoneIndex(TEXT("neck_01"));
			Head = Target->GetRefSkeleton().FindBoneIndex(TEXT("head"));
		}
		FTBEmoteSample Gesture;
		const bool Standing = Pawn->GetCharacterMovement()->IsMovingOnGround() && !Pawn->bIsCrouched &&
		    !Pawn->CarriedToy && !Pawn->CarriedDummy && !Pawn->GetEmoteSample(Gesture);
		const float Desired = Standing ? 1.f - FMath::Clamp(float(Pawn->GetVelocity().Size2D()) / 40.f, 0.f, 1.f) : 0.f;
		StandWeight = FMath::FInterpTo(StandWeight, Desired, Delta, 12.f);
		const auto& Transform = Instance->GetSkelMeshComponent()->GetComponentTransform();
		Forward = Transform.InverseTransformVectorNoScale(Pawn->GetActorForwardVector());
		Up = Transform.InverseTransformVectorNoScale(FVector::UpVector);
		FTBHumanLookSnapshot Look;
		bPreserveLook = Pawn->GetHumanLook(Look);
		const auto& Pose = Driver->GetComponentSpaceTransforms();
		Rotations.SetNumUninitialized(TargetRest.Num());
		for (int32 Bone = 0; Bone < TargetRest.Num(); ++Bone)
		{
			const int32 Src = SourceIndices[Bone];
			const int32 Parent = Target->GetRefSkeleton().GetParentIndex(Bone);
			if (Pose.IsValidIndex(Src))
				Rotations[Bone] = Pose[Src].GetRotation() * SourceRest[Src].GetRotation().Inverse() * TargetRest[Bone].GetRotation();
			else
				Rotations[Bone] = Parent == INDEX_NONE ? TargetRest[Bone].GetRotation() :
				    Rotations[Parent] * Target->GetRefSkeleton().GetRefBonePose()[Bone].GetRotation();
			if (Bone == Pelvis && Pose.IsValidIndex(Src))
				HipOffset = (Pose[Src].GetLocation() - SourceRest[Src].GetLocation()) *
				    (TargetRest[Bone].GetLocation().Z / FMath::Max(1., SourceRest[Src].GetLocation().Z));
		}
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		Output.ResetToRefPose();
		if (Rotations.IsEmpty()) return true;
		// The child MetaHuman face copies these body curves before evaluating its own DNA rig.
		static const FName MouthCornerLeft(TEXT("CTRL_expressions_mouthCornerPullL"));
		static const FName MouthCornerRight(TEXT("CTRL_expressions_mouthCornerPullR"));
		static const FName CheekLeft(TEXT("CTRL_expressions_eyeCheekRaiseL"));
		static const FName CheekRight(TEXT("CTRL_expressions_eyeCheekRaiseR"));
		Output.Curve.Set(MouthCornerLeft, .58f);
		Output.Curve.Set(MouthCornerRight, .58f);
		Output.Curve.Set(CheekLeft, .10f);
		Output.Curve.Set(CheekRight, .10f);
		const auto& Bones = Output.Pose.GetBoneContainer();
		for (auto Index : Output.Pose.ForEachBoneIndex())
		{
			const int32 Bone = Bones.MakeMeshPoseIndex(Index).GetInt();
			if (!Rotations.IsValidIndex(Bone)) continue;
			const auto Parent = Output.Pose.GetParentBoneIndex(Index);
			FQuat ParentRotation = Parent.IsValid() ? Rotations[Bones.MakeMeshPoseIndex(Parent).GetInt()] : FQuat::Identity;
			Output.Pose[Index].SetRotation((ParentRotation.Inverse() * Rotations[Bone]).GetNormalized());
			if (Bone == Pelvis) Output.Pose[Index].AddToTranslation(ParentRotation.UnrotateVector(HipOffset));
		}
		if (StandWeight > .001f && LowerSpine != INDEX_NONE && Neck != INDEX_NONE && Head != INDEX_NONE)
		{
			const auto Spine = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(LowerSpine));
			const auto Collar = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Neck));
			const auto Skull = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Head));
			if (Spine != INDEX_NONE && Collar != INDEX_NONE && Skull != INDEX_NONE)
			{
				FCSPose<FCompactPose> Pose;
				Pose.InitPose(Output.Pose);
				const FVector Torso = Pose.GetComponentSpaceTransform(Collar).GetLocation() -
				                      Pose.GetComponentSpaceTransform(Spine).GetLocation();
				// Measure the *retargeted* torso. Keep a relaxed 3-degree lean and leave
				// the pelvis, foot placements and breathing from the source animation intact.
				const double Lean = FMath::Atan2(Torso.Dot(Forward), Torso.Dot(Up));
				const double Angle = FMath::Clamp(Lean - FMath::DegreesToRadians(3.), 0., FMath::DegreesToRadians(32.)) * StandWeight;
				const FQuat Lift(Up.Cross(Forward).GetSafeNormal(), -Angle);
				const auto Parent = Bones.GetParentBoneIndex(Spine);
				if (Parent != INDEX_NONE)
				{
					const FQuat Base = Pose.GetComponentSpaceTransform(Parent).GetRotation();
					auto& Local = Output.Pose[Spine];
					Local.SetRotation((Base.Inverse() * Lift * Base * Local.GetRotation()).GetNormalized());
					if (bPreserveLook)
					{
						// The source already aimed the head at the player's view in a match.
						const auto HeadParent = Bones.GetParentBoneIndex(Skull);
						const FQuat NeckRotation = Pose.GetComponentSpaceTransform(HeadParent).GetRotation();
						auto& HeadLocal = Output.Pose[Skull];
						HeadLocal.SetRotation((NeckRotation.Inverse() * Lift.Inverse() * NeckRotation * HeadLocal.GetRotation()).GetNormalized());
					}
				}
			}
		}
		return true;
	}
};
}

FAnimInstanceProxy* UTBHumanBodyPose::CreateAnimInstanceProxy() { return new FBodyPoseProxy(this); }
void UTBHumanBodyPose::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
