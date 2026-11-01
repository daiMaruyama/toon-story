#include "TBHumanPose.h"
#include "TBCharacter.h"
#include "TBToyPresentation.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/TBDummyToy.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "Components/SkeletalMeshComponent.h"
#include "TwoBoneIK.h"

namespace
{
	class FHumanPoseProxy : public FAnimInstanceProxy
	{
		FAnimNode_LinkedInputPose* Input = nullptr;
		float CrouchAmount = 0, GripAmount = 0, MeshScale = 1;
		FVector Contact[2], PalmOffset[2], ElbowDirection[2];
		FQuat PalmBasis[2], HandRotation[2];
		TWeakObjectPtr<USkeletalMesh> CachedMesh;
		TWeakObjectPtr<ATBCharacter> CachedCharacter;
		TWeakObjectPtr<AActor> CachedCarry;
		bool HasContacts = false, HasHandBasis = false, HadLiveContacts = false;
		bool HasLookBasis = false, HasLook = false;
		float LookYaw = 0, LookPitch = 0;
		uint32 LookRound = 0, LookPossession = 0;
		TWeakObjectPtr<ATBPlayerState> LookPlayer;
		FVector HeadForwardRef = FVector::YAxisVector, HeadUpRef = FVector::UpVector;
		FVector LookForwardCS = FVector::YAxisVector, LookUpCS = FVector::UpVector;
		FTBEmotePose EmotePose;

		void ResetLook()
		{
			HasLook = false; LookYaw = LookPitch = 0;
			LookRound = LookPossession = 0; LookPlayer.Reset();
		}

		void ResetGrip()
		{
			GripAmount = 0;
			HasContacts = HadLiveContacts = false;
			CachedCarry.Reset();
		}

	public:
		explicit FHumanPoseProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
		{
		}
		virtual void Initialize(UAnimInstance* Instance) override
		{
			FAnimInstanceProxy::Initialize(Instance);
			Input = Instance->GetLinkedInputPoseNode();
			ResetGrip();
			ResetLook();
			CrouchAmount = 0;
			CachedMesh.Reset();
			CachedCharacter.Reset();
			HasHandBasis = false;
			HasLookBasis = false;
		}
		virtual void PreUpdate(UAnimInstance* Instance, float Delta) override
		{
			FAnimInstanceProxy::PreUpdate(Instance, Delta);
			Input = Instance->GetLinkedInputPoseNode();
			auto* Character = Cast<ATBCharacter>(Instance->GetOwningActor());
			auto* Mesh = Instance->GetSkelMeshComponent();
			auto* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
			TBToyPresentation::BuildEmotePose(Mesh, Character, EmotePose);
			if (CachedMesh.Get() != Asset || CachedCharacter.Get() != Character)
			{
				ResetGrip();
				ResetLook();
				CachedMesh = Asset;
				CachedCharacter = Character;
				HasHandBasis = false;
				HasLookBasis = false;
				if (Asset)
				{
					const auto& Ref = Asset->GetRefSkeleton();
					const int32 Spine = Ref.FindBoneIndex(TEXT("spine_03"));
					const int32 Neck = Ref.FindBoneIndex(TEXT("neck_01"));
					const int32 Head = Ref.FindBoneIndex(TEXT("head"));
					HasLookBasis = Spine != INDEX_NONE && Neck != INDEX_NONE && Head != INDEX_NONE &&
					               Ref.GetParentIndex(Head) == Neck && Ref.GetParentIndex(Neck) == Spine;
					if (HasLookBasis)
					{
						FTransform HeadRef = FTransform::Identity;
						for (int32 Bone = Head; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone))
							HeadRef = HeadRef * Ref.GetRefBonePose()[Bone];
						// This configured child faces mesh +Y. Derive its face axes from bind pose, never bone X.
						HeadForwardRef = HeadRef.GetRotation().UnrotateVector(FVector::YAxisVector);
						HeadUpRef = HeadRef.GetRotation().UnrotateVector(FVector::UpVector);
					}
					HasHandBasis = true;
					for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
					{
						for (const TCHAR* Bone : {TEXT("upperarm"), TEXT("lowerarm"), TEXT("hand"),
						                          TEXT("middle_01"), TEXT("index_01"), TEXT("pinky_01")})
						{
							HasHandBasis &= Ref.FindBoneIndex(*FString::Printf(TEXT("%s_%s"), Bone, Side)) != INDEX_NONE;
						}
					}
				}
			}
			if (!Character || !Character->IsHuman() || !Asset)
			{
				ResetGrip();
				ResetLook();
				CrouchAmount = 0;
				return;
			}
			CrouchAmount = FMath::FInterpTo(CrouchAmount, Character->bIsCrouched ? 1.f : 0.f, Delta, 14.f);
			MeshScale = FMath::Max(.01f, float(Mesh->GetComponentScale().Z));
			// Read UObjects only on the game thread. Evaluate receives directions/angles and bind-pose axes only.
			FTBHumanLookSnapshot Look;
			if (HasLookBasis && Character->GetHumanLook(Look))
			{
				if (!HasLook || LookRound != Look.Round || LookPossession != Look.Possession || LookPlayer.Get() != Look.Player.Get())
				{
					ResetLook(); LookYaw = Character->GetActorRotation().Yaw;
				}
				LookRound = Look.Round; LookPossession = Look.Possession; LookPlayer = Look.Player.Get();
				const float Step = FMath::IsFinite(Delta) ? FMath::Clamp(Delta, 0.f, .05f) : 0.f;
				const float Alpha = 1.f - FMath::Exp(-16.f * Step);
				LookYaw = FRotator::NormalizeAxis(LookYaw + FMath::FindDeltaAngleDegrees(double(LookYaw), Look.View.Yaw) * Alpha);
				LookPitch = FMath::Lerp(LookPitch, float(FMath::Clamp(Look.View.Pitch, -75., 75.)), Alpha);
				const double RelativeYaw = FMath::Clamp(FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, double(LookYaw)), -85., 85.);
				const FVector Forward = FRotator(LookPitch, Character->GetActorRotation().Yaw + RelativeYaw, 0).Vector();
				LookForwardCS = Mesh->GetComponentTransform().InverseTransformVectorNoScale(Forward).GetSafeNormal();
				LookUpCS = Mesh->GetComponentTransform().InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
				HasLook = !LookForwardCS.IsNearlyZero() && !LookForwardCS.ContainsNaN() && !LookUpCS.ContainsNaN();
			}
			else ResetLook();
			AActor* Carried = Character->CarriedToy ? static_cast<AActor*>(Character->CarriedToy.Get())
			                                     : Character->CarriedDummy.Get();
			FVector LiveContact[2];
			const bool Live = IsValid(Carried) && HasHandBasis &&
			                  TBToyPresentation::CarryContacts(Character, LiveContact[0], LiveContact[1]);
			if (Carried && !Live)
			{
				// A changed/invalid visual must never use the previous object's hand targets.
				ResetGrip();
				return;
			}
			if (Live)
			{
				if (!HadLiveContacts || CachedCarry.Get() != Carried)
				{
					ResetGrip();
				}
				if (!HasContacts)
				{
					const auto& Ref = CachedMesh->GetRefSkeleton();
					auto Pose = [&](const FString& Name)
					{
						FTransform Result;
						for (int B = Ref.FindBoneIndex(*Name); B != INDEX_NONE; B = Ref.GetParentIndex(B))
						{
							Result = Result * Ref.GetRefBonePose()[B];
						}
						return Result;
					};
					for (int Side = 0; Side < 2; ++Side)
					{
						const FString S = Side == 0 ? TEXT("l") : TEXT("r");
						const FTransform Hand = Pose(TEXT("hand_") + S);
						PalmOffset[Side] = Hand.InverseTransformPosition(Pose(TEXT("middle_01_") + S).GetLocation());
						const FVector Across = Hand.InverseTransformVector(Pose(TEXT("index_01_") + S).GetLocation() -
						                                                   Pose(TEXT("pinky_01_") + S).GetLocation());
						PalmBasis[Side] = FRotationMatrix::MakeFromXY(PalmOffset[Side], Across).ToQuat();
					}
				}
				const auto& Transform = Mesh->GetComponentTransform();
				const FVector Forward = Transform.InverseTransformVectorNoScale(Character->GetActorForwardVector());
				const FVector Up = Transform.InverseTransformVectorNoScale(FVector::UpVector);
				const FVector Right = Transform.InverseTransformVectorNoScale(Character->GetActorRightVector());
				for (int Side = 0; Side < 2; ++Side)
				{
					Contact[Side] = Transform.InverseTransformPosition(LiveContact[Side]);
					ElbowDirection[Side] = (Right * (Side == 0 ? -35.f : 35.f) - Forward * 5.f - Up * 25.f) / MeshScale;
					HandRotation[Side] = FRotationMatrix::MakeFromXY(Forward, Up).ToQuat() * PalmBasis[Side].Inverse();
				}
				CachedCarry = Carried;
				HasContacts = true;
			}
			HadLiveContacts = Live;
			GripAmount = FMath::FInterpTo(GripAmount, Live ? 1.f : 0.f, Delta, 12.f);
			// On release keep only the last component-space pose, not a live reference to the dropped toy.
			// Reacquisition, owner/mesh change and missing bones invalidate it before evaluation.
			if (!Live && GripAmount < .001f) ResetGrip();
		}
		virtual bool Evaluate(FPoseContext& Output) override
		{
			if (!Input)
			{
				return false;
			}
			Input->Evaluate_AnyThread(Output);
			if (CrouchAmount < .001f && GripAmount < .001f && !HasLook && EmotePose.Bones.IsEmpty())
			{
				return true;
			}
			auto Index = [&](FName Name)
			{
				FBoneReference Ref;
				Ref.BoneName = Name;
				Ref.Initialize(Output.Pose.GetBoneContainer());
				return Ref.GetCompactPoseIndex(Output.Pose.GetBoneContainer());
			};
			if (CrouchAmount > .001f)
			{
				FCSPose<FCompactPose> Original;
				Original.InitPose(Output.Pose);
				const auto Pelvis = Index(TEXT("pelvis"));
				if (Pelvis != INDEX_NONE)
				{
					const FVector Offset(0, -5.f * CrouchAmount / MeshScale, -28.f * CrouchAmount / MeshScale);
					const auto Parent = Output.Pose.GetBoneContainer().GetParentBoneIndex(Pelvis);
					Output.Pose[Pelvis].AddToTranslation(
					    Original.GetComponentSpaceTransform(Parent).InverseTransformVector(Offset));
					for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
					{
						const auto Thigh = Index(*FString::Printf(TEXT("thigh_%s"), Side));
						const auto Calf = Index(*FString::Printf(TEXT("calf_%s"), Side));
						const auto Foot = Index(*FString::Printf(TEXT("foot_%s"), Side));
						if (Thigh == INDEX_NONE || Calf == INDEX_NONE || Foot == INDEX_NONE)
						{
							continue;
						}
						FTransform T = Original.GetComponentSpaceTransform(Thigh),
						           C = Original.GetComponentSpaceTransform(Calf),
						           F = Original.GetComponentSpaceTransform(Foot);
						const FVector Target = F.GetLocation();
						T.AddToTranslation(Offset);
						C.AddToTranslation(Offset);
						F.AddToTranslation(Offset);
						AnimationCore::SolveTwoBoneIK(T, C, F, T.GetLocation() + FVector(0, 100, -25), Target, false,
						                              1., 1.);
						FTransform P = Original.GetComponentSpaceTransform(Pelvis);
						P.AddToTranslation(Offset);
						Output.Pose[Thigh] = T.GetRelativeTransform(P);
						Output.Pose[Calf] = C.GetRelativeTransform(T);
						Output.Pose[Foot] = F.GetRelativeTransform(C);
					}
				}
			}
			TBToyPresentation::ApplyEmotePose(Output, EmotePose);
			if (HasLook)
			{
				const FCompactPoseBoneIndex Bones[] = {Index(TEXT("spine_03")), Index(TEXT("neck_01")), Index(TEXT("head"))};
				// A LOD may remove a required bone even if it exists in the asset. Leave that pose untouched.
				if (Bones[0] != INDEX_NONE && Bones[1] != INDEX_NONE && Bones[2] != INDEX_NONE)
				{
					FCSPose<FCompactPose> Pose;
					Pose.InitPose(Output.Pose);
					const FQuat HeadRotation = Pose.GetComponentSpaceTransform(Bones[2]).GetRotation();
					const FQuat CurrentFace = FRotationMatrix::MakeFromXZ(HeadRotation.RotateVector(HeadForwardRef),
					    HeadRotation.RotateVector(HeadUpRef)).ToQuat();
					FQuat Correction = (FRotationMatrix::MakeFromXZ(LookForwardCS, LookUpCS).ToQuat() * CurrentFace.Inverse()).GetNormalized();
					Correction.EnforceShortestArcWith(FQuat::Identity);
					FVector Axis; double Angle;
					Correction.ToAxisAndAngle(Axis, Angle);
					// Added correction from the base pose, not absolute anatomical joint angles:
					// torso <=19, neck <=28.5, head <=47.5 degrees.
					if (Angle > FMath::DegreesToRadians(95.)) Correction = FQuat(Axis, FMath::DegreesToRadians(95.));
					const double Weight[] = {.20, .30, .50};
					for (int32 Part = 0; Part < 3; ++Part)
					{
						FCSPose<FCompactPose> Current;
						Current.InitPose(Output.Pose);
						const auto Parent = Output.Pose.GetBoneContainer().GetParentBoneIndex(Bones[Part]);
						const FQuat ParentRotation = Parent == INDEX_NONE ? FQuat::Identity : Current.GetComponentSpaceTransform(Parent).GetRotation();
						const FQuat Increment = FQuat::Slerp(FQuat::Identity, Correction, Weight[Part]);
						Output.Pose[Bones[Part]].SetRotation((ParentRotation.Inverse() * Increment *
						    Current.GetComponentSpaceTransform(Bones[Part]).GetRotation()).GetNormalized());
					}
				}
			}
			if (GripAmount > .001f && HasContacts)
			{
				for (int Side = 0; Side < 2; ++Side)
				{
					const FString S = Side == 0 ? TEXT("l") : TEXT("r");
					const auto Upper = Index(*("upperarm_" + S)), Lower = Index(*("lowerarm_" + S)),
					           Hand = Index(*("hand_" + S));
					if (Upper == INDEX_NONE || Lower == INDEX_NONE || Hand == INDEX_NONE)
					{
						continue;
					}
					FCSPose<FCompactPose> Pose;
					Pose.InitPose(Output.Pose);
					FTransform U = Pose.GetComponentSpaceTransform(Upper), L = Pose.GetComponentSpaceTransform(Lower),
					           H = Pose.GetComponentSpaceTransform(Hand);
					const FVector Target = Contact[Side] - HandRotation[Side].RotateVector(PalmOffset[Side]);
					// Human mesh faces +Y: derive left/right from the actor instead of assuming mesh X signs.
					const FVector Pole = U.GetLocation() + ElbowDirection[Side];
					AnimationCore::SolveTwoBoneIK(
					    U, L, H, Pole, FMath::Lerp(H.GetLocation(), Target, double(GripAmount)), false, 1., 1.);
					H.SetRotation(FQuat::Slerp(H.GetRotation(), HandRotation[Side], GripAmount));
					const auto Parent = Output.Pose.GetBoneContainer().GetParentBoneIndex(Upper);
					Output.Pose[Upper] = U.GetRelativeTransform(Pose.GetComponentSpaceTransform(Parent));
					Output.Pose[Lower] = L.GetRelativeTransform(U);
					Output.Pose[Hand] = H.GetRelativeTransform(L);
				}
				for (const TCHAR* Side : {TEXT("l"), TEXT("r")})
				{
					for (const TCHAR* Finger : {TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky")})
					{
						for (int Joint = 1; Joint <= 3; ++Joint)
						{
							const auto Bone = Index(*FString::Printf(TEXT("%s_0%d_%s"), Finger, Joint, Side));
							if (Bone == INDEX_NONE)
							{
								continue;
							}
							auto& Transform = Output.Pose[Bone];
							Transform.SetRotation(
							    (Transform.GetRotation() *
							     FQuat(FVector::ZAxisVector,
							           FMath::DegreesToRadians((Side[0] == 'r' ? -1.f : 1.f) * 22.f * GripAmount)))
							        .GetNormalized());
						}
					}
				}
			}
			return true;
		}
	};
} // namespace
FAnimInstanceProxy* UTBHumanPose::CreateAnimInstanceProxy()
{
	return new FHumanPoseProxy(this);
}
void UTBHumanPose::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
	delete Proxy;
}
