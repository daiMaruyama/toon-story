#include "TBToyPresentation.h"
#include "TBCharacter.h"
#include "TBToyCharacter.h"
#include "TBToyCarVisual.h"
#include "GamePlay/TBDummyToy.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"

int32 TBToyPresentation::SelectionWeight(const UClass* VisualClass)
{
	if (!VisualClass) return 0;
	return VisualClass->GetName().Contains(TEXT("SpaceRanger")) ? 1 : 4;
}

int32 TBToyPresentation::SelectToy(const TArray<TSubclassOf<AActor>>& Catalogue)
{
	int32 Total = 0;
	for (auto Class : Catalogue) Total += SelectionWeight(Class.Get());
	if (Total == 0) return INDEX_NONE;
	int32 Roll = FMath::RandHelper(Total);
	for (int32 Index = 0; Index < Catalogue.Num(); ++Index)
	{
		Roll -= SelectionWeight(Catalogue[Index].Get());
		if (Roll < 0) return Index;
	}
	return INDEX_NONE;
}

void TBToyPresentation::Initialize(AActor* Visual)
{
	if (!Visual)
	{
		return;
	}
	Visual->SetActorEnableCollision(false);
	TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual);
	for (auto* Part : Parts)
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Visual->GetClass()->GetName().Contains(TEXT("SpaceRanger")))
		{
			// Only recolor the plain armor; retain face, sticker and detail materials.
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				const auto* Original = Part->GetMaterial(Slot);
				if (!Original || Original->GetName() != TEXT("M_plastic_base")) continue;
				auto* Base = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
				if (Base)
				{
					auto* Tint = UMaterialInstanceDynamic::Create(Base, Part);
					Tint->SetVectorParameterValue(TEXT("Color"), FLinearColor(.30f, .42f, .37f));
					Part->SetMaterial(Slot, Tint);
				}
			}
		}
		Part->SetComponentTickEnabled(true);
		Part->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		if (Part->LeaderPoseComponent.IsValid() || !Part->GetSkeletalMeshAsset())
		{
			continue;
		}
		const FString Directory = FPackageName::GetLongPackagePath(Part->GetSkeletalMeshAsset()->GetPathName());
		const FString Name = Directory.Contains(TEXT("/Recon")) ? TEXT("BS_Recon") : TEXT("BS_Locomotion");
		if (auto* Blend = LoadObject<UBlendSpace>(nullptr, *(Directory / Name)))
		{
			// Configure the post-process before PlayAnimation: reinitializing afterwards clears single-node playback.
			Part->SetOverridePostProcessAnimBP(
			    LoadClass<UAnimInstance>(nullptr, *(Directory / TEXT("ABP_TBLookLocomotion.ABP_TBLookLocomotion_C"))),
			    false);
			Part->PlayAnimation(Blend, true);
		}
	}
}

void TBToyPresentation::Animate(AActor* Visual, float Speed, bool Frozen)
{
	if (auto* Car = Cast<ATBToyCarVisual>(Visual))
	{
		Car->Animate(Speed, Frozen);
		return;
	}
	if (!Visual)
	{
		return;
	}
	TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual);
	for (auto* Part : Parts)
	{
		if (auto* Anim = Part->GetSingleNodeInstance())
		{
			if (Frozen)
			{
				// A watched/captured player uses the same authored rest pose as a decoy.
				if (auto* Blend = Cast<UBlendSpace>(Anim->GetCurrentAsset()))
				{
					const FBlendSample* Idle = nullptr;
					for (const FBlendSample& Sample : Blend->GetBlendSamples())
					{
						if (Sample.Animation && (!Idle || Sample.SampleValue.X < Idle->SampleValue.X))
						{
							Idle = &Sample;
						}
					}
					if (Idle)
					{
						Part->bPauseAnims = false;
						Part->PlayAnimation(Idle->Animation, false);
						Part->GetSingleNodeInstance()->SetPosition(0, false);
						Part->GetSingleNodeInstance()->SetPlaying(false);
						Part->TickAnimation(0, false);
						Part->RefreshBoneTransforms();
					}
				}
				Part->bPauseAnims = true;
				continue;
			}
			if (!Cast<UBlendSpace>(Anim->GetCurrentAsset()) && Part->GetSkeletalMeshAsset())
			{
				const FString Directory = FPackageName::GetLongPackagePath(Part->GetSkeletalMeshAsset()->GetPathName());
				const FString Name = Directory.Contains(TEXT("/Recon")) ? TEXT("BS_Recon") : TEXT("BS_Locomotion");
				if (auto* Blend = LoadObject<UBlendSpace>(nullptr, *(Directory / Name)))
				{
					Part->PlayAnimation(Blend, true);
				}
				Anim = Part->GetSingleNodeInstance();
			}
			Part->bPauseAnims = false;
			const bool Moving = Speed > 1.f;
			const float ReferenceSpeed = Speed > 65.f ? 120.f : 30.f;
			Anim->SetBlendSpacePosition(FVector(Moving ? ReferenceSpeed : 0.f, 0, 0));
			const auto* Pawn = Cast<ATBCharacter>(Visual->GetOwner());
			const bool Backwards =
			    Pawn && FVector::DotProduct(Pawn->GetVelocity(), Pawn->GetActorForwardVector()) < -1.f;
			Anim->SetPlayRate(Moving ? FMath::Clamp(Speed / (ReferenceSpeed * SizeScale), .15f, 2.f) *
			                               (Backwards ? -1.f : 1.f)
			                         : 1.f);
		}
		else
		{
			Part->bPauseAnims = Frozen;
		}
	}
}

void TBToyPresentation::Ground(UChildActorComponent* Visual, float FloorZ)
{
	if (!Visual || !Visual->GetChildActor())
	{
		return;
	}
	const FVector Local = Visual->GetRelativeLocation();
	if (!FMath::IsNearlyZero(Local.X) || !FMath::IsNearlyZero(Local.Y))
	{
		Visual->SetRelativeLocation(FVector(0, 0, Local.Z));
	}
	if (auto* Car = Cast<ATBToyCarVisual>(Visual->GetChildActor()))
	{
		Car->Ground(Visual, FloorZ);
		return;
	}
	TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual->GetChildActor());
	for (auto* Part : Parts)
	{
		if (Part->LeaderPoseComponent.IsValid() || !Part->GetSkeletalMeshAsset())
		{
			continue;
		}
		FName Left = Part->DoesSocketExist(TEXT("foot_L")) ? TEXT("foot_L") : TEXT("foot_l");
		FName Right = Part->DoesSocketExist(TEXT("foot_R")) ? TEXT("foot_R") : TEXT("foot_r");
		if (!Part->DoesSocketExist(Left) || !Part->DoesSocketExist(Right))
		{
			continue;
		}
		// Foot bones sit slightly above the sole. Use the lower supporting foot, never the lifted one.
		float Bottom = FMath::Min(Part->GetSocketLocation(Left).Z, Part->GetSocketLocation(Right).Z) - .7f;
		if (Part->GetSkeletalMeshAsset()->GetName().Contains(TEXT("Teddy")))
		{
			Bottom = FMath::Min(Bottom, float(Part->GetSocketLocation(TEXT("pelvis")).Z - 3.f));
		}
		const float Correction = FMath::Clamp(FloorZ - Bottom, -10.f, 10.f);
		Visual->AddWorldOffset(FVector(0, 0, Correction));
		break;
	}
}

void TBToyPresentation::Grip(UChildActorComponent* Visual, const FVector& HandPosition, FTBGripCache& Cache)
{
	if (!Visual || !Visual->GetChildActor())
	{
		return;
	}
	if (auto* Car = Cast<ATBToyCarVisual>(Visual->GetChildActor()))
	{
		Car->Grip(Visual, HandPosition);
		return;
	}
	if (!Cache.Part.IsValid() || Cache.Part->GetOwner() != Visual->GetChildActor())
	{
		Cache.Part.Reset();
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual->GetChildActor());
		for (auto* Part : Parts)
		{
			if (Part->LeaderPoseComponent.IsValid() || !Part->GetSkeletalMeshAsset())
			{
				continue;
			}
			if (Part->DoesSocketExist(TEXT("TB_CarryCenter")))
			{
				Cache.Part = Part;
				break;
			}
			const auto* Mesh = Part->GetSkeletalMeshAsset();
			const auto& Skeleton = Mesh->GetRefSkeleton();
			Cache.BodyBone = Skeleton.FindBoneIndex(TEXT("spine")) != INDEX_NONE ? TEXT("spine") : TEXT("Spine02");
			const int32 Head = Skeleton.FindBoneIndex(Cache.BodyBone);
			if (Head == INDEX_NONE)
			{
				continue;
			}
			FTransform Reference = FTransform::Identity;
			for (int32 Bone = Head; Bone != INDEX_NONE; Bone = Skeleton.GetParentIndex(Bone))
			{
				Reference = Reference * Skeleton.GetRefBonePose()[Bone];
			}
			FVector Contact = Reference.GetLocation();
			Contact.X = Mesh->GetBounds().Origin.X + Mesh->GetBounds().BoxExtent.X * .5f;
			Cache.ContactInBody = Reference.InverseTransformPosition(Contact);
			Cache.Part = Part;
			break;
		}
	}
	if (auto* Part = Cache.Part.Get())
	{
		const FVector Contact = Part->DoesSocketExist(TEXT("TB_CarryCenter"))
		                            ? Part->GetSocketLocation(TEXT("TB_CarryCenter"))
		                            : Part->GetSocketTransform(Cache.BodyBone).TransformPosition(Cache.ContactInBody);
		Visual->AddWorldOffset(HandPosition - Contact);
	}
}

bool TBToyPresentation::CarryContacts(const ATBCharacter* Human, FVector& Left, FVector& Right)
{
	const auto* Toy = Human ? Cast<ATBToyCharacter>(Human->CarriedToy) : nullptr;
	const auto* Visual = Toy                            ? Toy->ToyVisual.Get()
	                     : Human && Human->CarriedDummy ? Human->CarriedDummy->Visual.Get()
	                                                    : nullptr;
	if (!Visual || !Visual->GetChildActor())
	{
		return false;
	}
	if (const auto* Car = Cast<ATBToyCarVisual>(Visual->GetChildActor()))
	{
		return Car->CarryContacts(Human, Left, Right);
	}
	TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual->GetChildActor());
	for (auto* Part : Parts)
	{
		if (Part->LeaderPoseComponent.IsValid() || !Part->DoesSocketExist(TEXT("TB_CarryCenter")))
		{
			continue;
		}
		const FVector Center = Part->GetSocketLocation(TEXT("TB_CarryCenter"));
		Left = Human->GetCarryGripLocation() + Part->GetSocketLocation(TEXT("TB_CarryLeft")) - Center;
		Right = Human->GetCarryGripLocation() + Part->GetSocketLocation(TEXT("TB_CarryRight")) - Center;
		// Slender figures need staggered hands so the two sets of fingers do not intersect in front.
		const double Stagger = FMath::Clamp((12. - FVector::Distance(Left, Right)) * .6, 0., 3.);
		Left.Z -= Stagger;
		Right.Z += Stagger;
		// Keep the palm skin outside the torso; only the curled fingertips wrap around it.
		Left -= Human->GetActorRightVector() * 1.2f;
		Right += Human->GetActorRightVector() * 1.2f;
		return true;
	}
	return false;
}
