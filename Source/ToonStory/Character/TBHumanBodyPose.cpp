#include "TBHumanPose.h"
#include "TBCharacter.h"
#include "TBToyPresentation.h"
#include "GamePlay/TBDummyToy.h"
#include "TwoBoneIK.h"
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
	int32 ArmBones[2][3] = {{INDEX_NONE, INDEX_NONE, INDEX_NONE}, {INDEX_NONE, INDEX_NONE, INDEX_NONE}};
	FVector PalmOffset[2], GripContact[2], ElbowDirection[2];
	FQuat PalmBasis[2], HandRotation[2];
	TArray<TPair<int32, FQuat>> FingerRest[2];
	bool bHasHandBasis = false, bHasGripTargets = false, bHadLiveGrip = false;
	float GripWeight = 0;
	TWeakObjectPtr<AActor> GripObject;

	void ResetGrip()
	{
		GripWeight = 0;
		bHasGripTargets = bHadLiveGrip = false;
		GripObject.Reset();
	}

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
		if (!Source || !Target) { ResetGrip(); return; }
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
			ResetGrip();
			bHasHandBasis = true;
			const auto& Ref = Target->GetRefSkeleton();
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const FString Suffix = Side == 0 ? TEXT("l") : TEXT("r");
				ArmBones[Side][0] = Ref.FindBoneIndex(*("upperarm_" + Suffix));
				ArmBones[Side][1] = Ref.FindBoneIndex(*("lowerarm_" + Suffix));
				ArmBones[Side][2] = Ref.FindBoneIndex(*("hand_" + Suffix));
				const int32 Middle = Ref.FindBoneIndex(*("middle_01_" + Suffix));
				const int32 Index = Ref.FindBoneIndex(*("index_01_" + Suffix));
				const int32 Pinky = Ref.FindBoneIndex(*("pinky_01_" + Suffix));
				if (ArmBones[Side][0] == INDEX_NONE || ArmBones[Side][1] == INDEX_NONE || ArmBones[Side][2] == INDEX_NONE ||
				    Middle == INDEX_NONE || Index == INDEX_NONE || Pinky == INDEX_NONE)
				{ bHasHandBasis = false; continue; }
				const auto& Hand = TargetRest[ArmBones[Side][2]];
				PalmOffset[Side] = Hand.InverseTransformPosition(TargetRest[Middle].GetLocation());
				const FVector Across = Hand.InverseTransformVector(TargetRest[Index].GetLocation() - TargetRest[Pinky].GetLocation());
				PalmBasis[Side] = FRotationMatrix::MakeFromXY(PalmOffset[Side], Across).ToQuat();
				FingerRest[Side].Reset();
				// The MetaHuman reference fingers are already curled. Flatten their flexion
				// into the palm plane, preserving the mesh's own finger spread and thumb side.
				const FVector PalmNormal = Hand.TransformVectorNoScale(PalmBasis[Side].GetAxisZ()).GetSafeNormal();
				TArray<FTransform> OpenHand = TargetRest;
				for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
				{
					const int32 Parent = Ref.GetParentIndex(Bone);
					int32 Ancestor = Parent;
					while (Ancestor != INDEX_NONE && Ancestor != ArmBones[Side][2]) Ancestor = Ref.GetParentIndex(Ancestor);
					if (Ancestor != ArmBones[Side][2]) continue;
					OpenHand[Bone] = Ref.GetRefBonePose()[Bone] * OpenHand[Parent];
					const FString Name = Ref.GetBoneName(Bone).ToString();
					if (Name.Contains(TEXT("_01_")) || Name.Contains(TEXT("_02_")) || Name.Contains(TEXT("_03_")))
					{
						const FVector Direction = OpenHand[Bone].GetRotation().GetAxisX();
						const FVector FlatDirection = FVector::VectorPlaneProject(Direction, PalmNormal).GetSafeNormal();
						if (!FlatDirection.IsNearlyZero())
							OpenHand[Bone].SetRotation((FQuat::FindBetweenNormals(Direction, FlatDirection) * OpenHand[Bone].GetRotation()).GetNormalized());
					}
					FingerRest[Side].Emplace(Bone, OpenHand[Bone].GetRelativeTransform(OpenHand[Parent]).GetRotation());
				}
			}
		}
		FTBEmoteSample Gesture;
		const bool bEmoting = Pawn->GetEmoteSample(Gesture);
		const bool Standing = Pawn->GetCharacterMovement()->IsMovingOnGround() && !Pawn->bIsCrouched &&
		    !Pawn->CarriedToy && !Pawn->CarriedDummy && (!bEmoting || Gesture.Kind == ETBEmoteKind::HairDisappear);
		const float Desired = Standing ? 1.f - FMath::Clamp(float(Pawn->GetVelocity().Size2D()) / 40.f, 0.f, 1.f) : 0.f;
		StandWeight = FMath::FInterpTo(StandWeight, Desired, Delta, 12.f);
		const auto& Transform = Instance->GetSkelMeshComponent()->GetComponentTransform();
		Forward = Transform.InverseTransformVectorNoScale(Pawn->GetActorForwardVector());
		Up = Transform.InverseTransformVectorNoScale(FVector::UpVector);
		// Source rotations alone cannot preserve contact after changing arm lengths and hand proportions.
		AActor* Carried = Pawn->CarriedToy ? static_cast<AActor*>(Pawn->CarriedToy.Get()) : Pawn->CarriedDummy.Get();
		FVector LiveContact[2];
		const bool LiveGrip = IsValid(Carried) && bHasHandBasis && TBToyPresentation::CarryContacts(Pawn, LiveContact[0], LiveContact[1]);
		if (Carried && !LiveGrip) ResetGrip();
		if (LiveGrip)
		{
			if (!bHadLiveGrip || GripObject.Get() != Carried) ResetGrip();
			const FVector Right = Transform.InverseTransformVectorNoScale(Pawn->GetActorRightVector());
			const double Scale = FMath::Max(.01, Transform.GetScale3D().GetAbsMax());
			for (int32 Side = 0; Side < 2; ++Side)
			{
				GripContact[Side] = Transform.InverseTransformPosition(LiveContact[Side] + Pawn->GetActorRightVector() * (Side == 0 ? -2.5 : 2.5));
				// Point fingers forward and the index/thumb side upward. Swapping these axes
				// rolls both wrists upside down even when the palm contact positions are correct.
				HandRotation[Side] = FRotationMatrix::MakeFromXY(Forward, Up).ToQuat() * PalmBasis[Side].Inverse();
				ElbowDirection[Side] = (Right * (Side == 0 ? -35. : 35.) - Forward * 5. - Up * 25.) / Scale;
			}
			GripObject = Carried;
			bHasGripTargets = true;
		}
		bHadLiveGrip = LiveGrip;
		GripWeight = FMath::FInterpTo(GripWeight, LiveGrip ? 1.f : 0.f, Delta, 12.f);
		if (!LiveGrip && GripWeight < .001f) ResetGrip();
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
		if (bHasGripTargets && GripWeight > .001f)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const auto Upper = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(ArmBones[Side][0]));
				const auto Lower = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(ArmBones[Side][1]));
				const auto Hand = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(ArmBones[Side][2]));
				if (Upper == INDEX_NONE || Lower == INDEX_NONE || Hand == INDEX_NONE) continue;
				const auto Parent = Bones.GetParentBoneIndex(Upper);
				if (Parent == INDEX_NONE) continue;
				FCSPose<FCompactPose> Pose;
				Pose.InitPose(Output.Pose);
				FTransform U = Pose.GetComponentSpaceTransform(Upper), L = Pose.GetComponentSpaceTransform(Lower), H = Pose.GetComponentSpaceTransform(Hand);
				const FVector Target = GripContact[Side] - HandRotation[Side].RotateVector(PalmOffset[Side]);
				AnimationCore::SolveTwoBoneIK(U, L, H, U.GetLocation() + ElbowDirection[Side],
				    FMath::Lerp(H.GetLocation(), Target, double(GripWeight)), false, 1., 1.);
				H.SetRotation(FQuat::Slerp(H.GetRotation(), HandRotation[Side], GripWeight).GetNormalized());
				Output.Pose[Upper] = U.GetRelativeTransform(Pose.GetComponentSpaceTransform(Parent));
				Output.Pose[Lower] = L.GetRelativeTransform(U);
				Output.Pose[Hand] = H.GetRelativeTransform(L);
				// Finger curls copied from the original child's smaller hand are not compatible
				// with this mesh. Blend to this hand's own relaxed reference fingers while holding.
				for (const auto& Finger : FingerRest[Side])
				{
					const auto Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Finger.Key));
					if (Index != INDEX_NONE)
						Output.Pose[Index].SetRotation(FQuat::Slerp(Output.Pose[Index].GetRotation(), Finger.Value, GripWeight).GetNormalized());
				}
			}
		}
		return true;
	}
};
}

FAnimInstanceProxy* UTBHumanBodyPose::CreateAnimInstanceProxy() { return new FBodyPoseProxy(this); }
void UTBHumanBodyPose::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
