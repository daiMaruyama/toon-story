#include "TBToyPresentation.h"
#include "TBCharacter.h"
#include "TBToyCharacter.h"
#include "TBToyCarVisual.h"
#include "GamePlay/TBDummyToy.h"
#include "Data/TBRuleData.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_LinkedInputPose.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

namespace
{
	FName EmoteBone(const FReferenceSkeleton& Ref, std::initializer_list<const TCHAR*> Names)
	{
		for (const TCHAR* Name : Names)
		{
			const int32 Index = Ref.FindBoneIndex(Name);
			if (Index > 0 && Ref.GetParentIndex(Index) != INDEX_NONE) return Name;
		}
		return NAME_None;
	}
	FName EmoteTorso(const FReferenceSkeleton& Ref)
	{
		return EmoteBone(Ref, {TEXT("spine_03"), TEXT("spine"), TEXT("Spine02"), TEXT("Spine01"), TEXT("spine_01"), TEXT("body"), TEXT("chest")});
	}
	FName EmoteHead(const FReferenceSkeleton& Ref)
	{
		return EmoteBone(Ref, {TEXT("head"), TEXT("Head01"), TEXT("head_01"), TEXT("neck_01"), TEXT("neck")});
	}
	struct FCarEmoteRest
	{
		FTransform Transform;
		TWeakObjectPtr<ATBCharacter> Pawn;
	};
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FCarEmoteRest> CarEmoteRest;
}

bool TBToyPresentation::SupportsEmoteMesh(const USkeletalMesh* Mesh)
{
	if (!Mesh) return false;
	const auto& Ref = Mesh->GetRefSkeleton();
	return !EmoteTorso(Ref).IsNone() || !EmoteHead(Ref).IsNone();
}

bool TBToyPresentation::SupportsEmoteClass(UClass* VisualClass)
{
	if (!IsValid(VisualClass) || !VisualClass->IsChildOf(AActor::StaticClass())) return false;
	if (VisualClass->IsChildOf(ATBToyCarVisual::StaticClass())) return true;
	// Blueprint component templates are available at runtime too; no spawning/animation on a dedicated server.
	TInlineComponentArray<USkeletalMeshComponent*> Parts(VisualClass->GetDefaultObject<AActor>());
	for (auto* Part : Parts) if (SupportsEmoteMesh(Part->GetSkeletalMeshAsset())) return true;
	for (UClass* Parent = VisualClass; Parent; Parent = Parent->GetSuperClass())
		if (auto* BP = Cast<UBlueprintGeneratedClass>(Parent); BP && BP->SimpleConstructionScript)
			for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
				if (auto* Part = Cast<USkeletalMeshComponent>(Node->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(VisualClass))))
					if (SupportsEmoteMesh(Part->GetSkeletalMeshAsset())) return true;
	return false;
}

void TBToyPresentation::BuildEmotePose(const USkeletalMeshComponent* Mesh, const ATBCharacter* Pawn, FTBEmotePose& Out)
{
	Out.Bones.Reset();
	FTBEmoteSample Sample;
	const auto* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!IsValid(Pawn) || !Asset || !Pawn->GetEmoteSample(Sample) || Sample.Weight <= 0) return;
	const auto& Ref = Asset->GetRefSkeleton();
	const FVector Forward = Mesh->GetComponentTransform().InverseTransformVectorNoScale(Pawn->GetActorForwardVector()).GetSafeNormal();
	const FVector Right = Mesh->GetComponentTransform().InverseTransformVectorNoScale(Pawn->GetActorRightVector()).GetSafeNormal();
	const FVector Up = Mesh->GetComponentTransform().InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
	if (Forward.ContainsNaN() || Forward.IsNearlyZero() || Right.IsNearlyZero() || Up.IsNearlyZero()) return;
	const double Beat = Sample.Progress * 2 * PI;
	auto Add = [&](FName Name, FVector Axis, double Degrees)
	{
		if (!Name.IsNone()) Out.Bones.Add({Name, FQuat(Axis, FMath::DegreesToRadians(Degrees * Sample.Weight))});
	};
	const FName Torso = EmoteTorso(Ref), Head = EmoteHead(Ref);
	const FString Name = Asset->GetName();
	const bool Animal = Name.Contains(TEXT("Duck")) || Name.Contains(TEXT("Dinosaur"));
	const FName UpperL = EmoteBone(Ref, {TEXT("upperarm_l"), TEXT("L_UpperArm"), TEXT("LeftArm"), TEXT("Arm_L"), TEXT("mixamorig:LeftArm")});
	const FName UpperR = EmoteBone(Ref, {TEXT("upperarm_r"), TEXT("R_UpperArm"), TEXT("RightArm"), TEXT("Arm_R"), TEXT("mixamorig:RightArm")});
	const bool Arms = !Animal && !UpperL.IsNone() && !UpperR.IsNone();
	if (Sample.Kind == ETBEmoteKind::Wave)
	{
		if (Arms)
		{
			Add(UpperR, Forward, 125);
			Add(EmoteBone(Ref, {TEXT("lowerarm_r"), TEXT("R_Forearm"), TEXT("RightForeArm"), TEXT("mixamorig:RightForeArm")}), Forward, 15 + 18 * FMath::Sin(Beat * 3));
		}
		else { Add(Torso, Up, 9 * FMath::Sin(Beat * 3)); Add(Head, Right, 8 * FMath::Sin(Beat * 2)); }
	}
	else if (Sample.Kind == ETBEmoteKind::Cheer)
	{
		if (Arms) { Add(UpperR, Forward, 140 + 7 * FMath::Sin(Beat * 2)); Add(UpperL, Forward, -140 - 7 * FMath::Sin(Beat * 2)); }
		Add(Torso, Right, -10 * FMath::Sin(PI * Sample.Progress)); Add(Head, Right, 6 * FMath::Sin(Beat * 2));
	}
	else if (Sample.Kind == ETBEmoteKind::WildDance)
	{
		Add(Torso, Up, 38 * FMath::Sin(Beat * 8));
		Add(Torso, Forward, 24 * FMath::Sin(Beat * 12));
		Add(Torso, Right, 18 * FMath::Sin(Beat * 6));
		Add(Head, Right, 16 * FMath::Sin(Beat * 12));
		Add(Head, Up, 22 * FMath::Sin(Beat * 5));
		if (Arms)
		{
			Add(UpperR, Forward, 95 + 65 * FMath::Sin(Beat * 8));
			Add(UpperL, Forward, -95 + 65 * FMath::Cos(Beat * 8));
			Add(UpperR, Right, 45 * FMath::Cos(Beat * 6));
			Add(UpperL, Right, -45 * FMath::Sin(Beat * 6));
		}
		Add(EmoteBone(Ref, {TEXT("thigh_l"), TEXT("LeftUpLeg"), TEXT("mixamorig:LeftUpLeg")}), Right, 20 * FMath::Sin(Beat * 8));
		Add(EmoteBone(Ref, {TEXT("thigh_r"), TEXT("RightUpLeg"), TEXT("mixamorig:RightUpLeg")}), Right, -20 * FMath::Sin(Beat * 8));
	}
	else if (Sample.Kind == ETBEmoteKind::Dance)
	{
		Add(Torso, Up, 12 * FMath::Sin(Beat * 3)); Add(Torso, Forward, 7 * FMath::Sin(Beat * 6));
		Add(Head, Right, 6 * FMath::Sin(Beat * 6));
		if (Arms) { Add(UpperR, Forward, 45 + 22 * FMath::Sin(Beat * 3)); Add(UpperL, Forward, -45 + 22 * FMath::Sin(Beat * 3)); }
	}
}

void TBToyPresentation::ApplyEmotePose(FPoseContext& Output, const FTBEmotePose& Pose)
{
	// Bone names and numeric rotations only. Never touch UObjects on the animation worker.
	for (const FTBEmoteBone& Change : Pose.Bones)
	{
		FBoneReference Reference; Reference.BoneName = Change.Name; Reference.Initialize(Output.Pose.GetBoneContainer());
		const auto Bone = Reference.GetCompactPoseIndex(Output.Pose.GetBoneContainer());
		if (Bone == INDEX_NONE || Change.Rotation.ContainsNaN()) continue;
		FCSPose<FCompactPose> Current; Current.InitPose(Output.Pose);
		const auto Parent = Output.Pose.GetBoneContainer().GetParentBoneIndex(Bone);
		if (Parent == INDEX_NONE) continue; // Never move the skeleton root.
		Output.Pose[Bone].SetRotation((Current.GetComponentSpaceTransform(Parent).GetRotation().Inverse() * Change.Rotation *
		    Current.GetComponentSpaceTransform(Bone).GetRotation()).GetNormalized());
	}
}

void TBToyPresentation::RestoreEmoteVisual(AActor* Visual)
{
	for (auto It = CarEmoteRest.CreateIterator(); It; ++It)
	{
		auto* Part = It.Key().Get();
		if (!Part || Part->GetOwner() == Visual)
		{
			if (Part)
			{
				Part->SetRelativeTransform(It.Value().Transform);
				if (auto* Pawn = It.Value().Pawn.Get()) Part->GetOwner()->OnEndPlay.RemoveDynamic(Pawn, &ATBCharacter::OnEmoteVisualEndPlay);
			}
			It.RemoveCurrent();
		}
	}
}

void TBToyPresentation::RestoreEmoteOwner(ATBCharacter* Pawn)
{
	TArray<AActor*> Visuals;
	for (const auto& Pair : CarEmoteRest)
		if (Pair.Value.Pawn.Get() == Pawn && Pair.Key.IsValid()) Visuals.AddUnique(Pair.Key->GetOwner());
	for (AActor* Visual : Visuals) RestoreEmoteVisual(Visual);
}

void TBToyPresentation::ApplyCarEmote(UStaticMeshComponent* Car, ATBCharacter* Pawn)
{
	if (!IsValid(Car)) return;
	FTBEmoteSample Sample;
	if (!IsValid(Pawn) || !Pawn->GetEmoteSample(Sample)) { RestoreEmoteVisual(Car->GetOwner()); return; }
	for (auto It = CarEmoteRest.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
	if (!CarEmoteRest.Contains(Car))
	{
		CarEmoteRest.Add(Car, {Car->GetRelativeTransform(), Pawn});
		Car->GetOwner()->OnEndPlay.AddUniqueDynamic(Pawn, &ATBCharacter::OnEmoteVisualEndPlay);
	}
	const auto& Rest = CarEmoteRest.FindChecked(Car);
	const double Beat = Sample.Progress * 2 * PI;
	const FRotator Sway = Sample.Kind == ETBEmoteKind::Wave ? FRotator(0, 10 * FMath::Sin(Beat * 3), 0) :
	    Sample.Kind == ETBEmoteKind::WildDance ? FRotator(18 * FMath::Sin(Beat * 12), 38 * FMath::Sin(Beat * 8), 24 * FMath::Cos(Beat * 10)) :
	    Sample.Kind == ETBEmoteKind::Cheer ? FRotator(-9 * FMath::Sin(Beat * 2), 0, 0) :
	    FRotator(5 * FMath::Sin(Beat * 6), 8 * FMath::Sin(Beat * 3), 7 * FMath::Sin(Beat * 6));
	FTransform Pose = Rest.Transform;
	Pose.SetRotation((Sway.Quaternion() * Rest.Transform.GetRotation()).GetNormalized());
	Car->SetRelativeTransform(FTransform(FQuat::Slerp(Rest.Transform.GetRotation(), Pose.GetRotation(), Sample.Weight), Rest.Transform.GetLocation(), Rest.Transform.GetScale3D()));
}

int32 TBToyPresentation::SelectionWeight(const UClass* VisualClass)
{
	if (!VisualClass) return 0;
	const auto& Toys = TB::Rules().Toys;
	const int32* Weight = Toys.ToyWeights.Find(TSoftClassPtr<AActor>(VisualClass));
	// 上限を付けて、カタログ全体の合計があふれないようにする。
	return FMath::Clamp(Weight ? *Weight : Toys.DefaultToyWeight, 0, 1000);
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
			if (!Anim) continue;
			Part->bPauseAnims = false;
			const bool Moving = Speed > 1.f;
			float MinSpeed = MAX_flt, MaxSpeed = 0.f;
			if (const auto* Blend = Cast<UBlendSpace>(Anim->GetCurrentAsset()))
			{
				for (const FBlendSample& Sample : Blend->GetBlendSamples())
				{
					if (Sample.Animation && Sample.SampleValue.X > 0.f)
					{
						MinSpeed = FMath::Min(MinSpeed, float(Sample.SampleValue.X));
						MaxSpeed = FMath::Max(MaxSpeed, float(Sample.SampleValue.X));
					}
				}
			}
			// Blend through the authored walk/run range rather than jumping between samples at speed 65.
			// Outside that range, adjust cadence as before; idle and the decoy-matching frozen pose stay intact.
			const float ReferenceSpeed = Moving && MaxSpeed > 0.f
			                                 ? FMath::Clamp(Speed / SizeScale, MinSpeed, MaxSpeed)
			                                 : 0.f;
			Anim->SetBlendSpacePosition(FVector(ReferenceSpeed, 0, 0));
			const auto* Pawn = Cast<ATBCharacter>(Visual->GetOwner());
			const bool Backwards =
			    Pawn && FVector::DotProduct(Pawn->GetVelocity(), Pawn->GetActorForwardVector()) < -1.f;
			Anim->SetPlayRate(ReferenceSpeed > 0.f ? FMath::Clamp(Speed / (ReferenceSpeed * SizeScale), .15f, 2.f) *
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
