#include "TBHumanPose.h"
#include "TBCharacter.h"
#include "TBToyPresentation.h"
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
		bool HasContacts = false;

	public:
		explicit FHumanPoseProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance)
		{
		}
		virtual void Initialize(UAnimInstance* Instance) override
		{
			FAnimInstanceProxy::Initialize(Instance);
			Input = Instance->GetLinkedInputPoseNode();
		}
		virtual void PreUpdate(UAnimInstance* Instance, float Delta) override
		{
			FAnimInstanceProxy::PreUpdate(Instance, Delta);
			Input = Instance->GetLinkedInputPoseNode();
			const auto* Character = Cast<ATBCharacter>(Instance->GetOwningActor());
			CrouchAmount = FMath::FInterpTo(CrouchAmount, Character && Character->bIsCrouched ? 1.f : 0.f, Delta, 14.f);
			GripAmount = FMath::FInterpTo(
			    GripAmount, Character && (Character->CarriedToy || Character->CarriedDummy) ? 1.f : 0.f, Delta, 12.f);
			MeshScale = FMath::Max(.01f, float(Instance->GetSkelMeshComponent()->GetComponentScale().Z));
			auto* Mesh = Instance->GetSkelMeshComponent();
			HasContacts = TBToyPresentation::CarryContacts(Character, Contact[0], Contact[1]);
			if (HasContacts)
			{
				if (CachedMesh != Mesh->GetSkeletalMeshAsset())
				{
					CachedMesh = Mesh->GetSkeletalMeshAsset();
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
					Contact[Side] = Transform.InverseTransformPosition(Contact[Side]);
					ElbowDirection[Side] = (Right * (Side == 0 ? -35.f : 35.f) - Forward * 5.f - Up * 25.f) / MeshScale;
					HandRotation[Side] = FRotationMatrix::MakeFromXY(Forward, Up).ToQuat() * PalmBasis[Side].Inverse();
				}
			}
		}
		virtual bool Evaluate(FPoseContext& Output) override
		{
			if (!Input)
			{
				return false;
			}
			Input->Evaluate_AnyThread(Output);
			if (CrouchAmount < .001f && GripAmount < .001f)
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
