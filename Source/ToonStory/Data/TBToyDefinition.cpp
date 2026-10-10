#include "Data/TBToyDefinition.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#endif

bool UTBToyDefinition::ValidateDefinition(FString& OutMessage) const
{
	const auto Fail = [&OutMessage](const TCHAR* Message) { OutMessage = Message; return false; };
	if (!SkeletalMesh && !StaticMesh) return Fail(TEXT("Assign a skeletal or static mesh."));
	if (!FMath::IsFinite(Height) || Height <= 0.f) return Fail(TEXT("Height must be positive."));
	if (!FMath::IsFinite(WalkSpeed) || !FMath::IsFinite(RunSpeed) || WalkSpeed < 0.f || RunSpeed < 0.f)
		return Fail(TEXT("Movement speeds must be finite and non-negative."));
	if (WalkSpeed > 0.f && RunSpeed > 0.f && RunSpeed <= WalkSpeed)
		return Fail(TEXT("Run speed must exceed walk speed."));
	if (SkeletalMesh)
	{
		auto* Skeleton = SkeletalMesh->GetSkeleton();
		if (!Skeleton) return Fail(TEXT("The skeletal mesh needs a skeleton."));
		for (UAnimSequence* Clip : {Idle.Get(), Walk.Get(), Run.Get(), Jump.Get()})
		{
			if (Clip && Clip->GetSkeleton() != Skeleton) return Fail(TEXT("Retarget clips to the mesh skeleton first."));
		}
		if (Locomotion && Locomotion->GetSkeleton() != Skeleton)
			return Fail(TEXT("The locomotion blend space must use the mesh skeleton."));
		if (!AnimationBlueprint && !Locomotion && (!Idle || !Walk))
			return Fail(TEXT("Assign an Animation Blueprint, locomotion blend space, or idle/walk clips."));
		if (Locomotion && !AnimationBlueprint)
		{
			for (float Speed : {0.f, WalkSpeed, RunSpeed})
			{
				TArray<FBlendSampleData> Samples;
				int32 Segment = INDEX_NONE;
				if (!Locomotion->GetSamplesFromBlendInput(FVector(Speed, 0, 0), Samples, Segment, true) || Samples.IsEmpty())
					return Fail(TEXT("The blend space has no evaluable samples. Rebuild locomotion and save it."));
			}
		}
	}
	OutMessage = TEXT("Valid. Save the definition and any modified assets before playing.");
	return true;
}

void UTBToyDefinition::ValidateToy()
{
	ValidateDefinition(SetupStatus);
	UE_LOG(LogTemp, Display, TEXT("%s: %s"), *GetName(), *SetupStatus);
}

void UTBToyDefinition::CreateLocomotion()
{
#if WITH_EDITOR
	if (Locomotion)
	{
		SetupStatus = TEXT("Locomotion is already assigned. Edit its samples/speeds in the Blend Space editor; existing work was preserved.");
		return;
	}
	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton() || !Idle || !Walk || !Run ||
	    !FMath::IsFinite(WalkSpeed) || !FMath::IsFinite(RunSpeed) || WalkSpeed <= 0.f || RunSpeed <= WalkSpeed)
	{
		SetupStatus = TEXT("Assign a skeletal mesh and idle/walk/run clips; set 0 < Walk Speed < Run Speed.");
		return;
	}
	for (UAnimSequence* Clip : {Idle.Get(), Walk.Get(), Run.Get()})
	{
		if (Clip->GetSkeleton() != SkeletalMesh->GetSkeleton() || Clip->IsValidAdditive())
		{
			SetupStatus = TEXT("Use non-additive clips retargeted to the mesh skeleton.");
			return;
		}
	}
	const FString Folder = FPackageName::GetLongPackagePath(GetOutermost()->GetName());
	if (Folder != TEXT("/Game") && !Folder.StartsWith(TEXT("/Game/")))
	{
		SetupStatus = TEXT("Save this definition in the project's Content folder first.");
		return;
	}
	const FString Name = TEXT("BS_") + GetName() + TEXT("_Locomotion");
	const FString PackageName = Folder / Name;
	if (FPackageName::DoesPackageExist(PackageName) || FindPackage(nullptr, *PackageName))
	{
		SetupStatus = TEXT("An asset with this output name exists. Assign it explicitly; it was not overwritten.");
		return;
	}
	Modify();
	auto* Package = CreatePackage(*PackageName);
	auto* Blend = NewObject<UBlendSpace1D>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
	Blend->SetSkeleton(SkeletalMesh->GetSkeleton());
	// These are UE's reflected editor properties, also used by the native Blend Space editor.
	auto* AxisProperty = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
	check(AxisProperty);
	auto* Axis = AxisProperty->ContainerPtrToValuePtr<FBlendParameter>(Blend, 0);
	Axis->DisplayName = TEXT("Speed (cm/s)");
	Axis->Min = 0.f;
	Axis->Max = RunSpeed;
	Axis->GridNum = 8;
	Axis->bSnapToGrid = false;
	Blend->InterpolationParam[0].InterpolationTime = .16f;
	Blend->TargetWeightInterpolationSpeedPerSec = 5.f;
	Blend->AddSample(Idle, FVector::ZeroVector);
	Blend->AddSample(Walk, FVector(WalkSpeed, 0, 0));
	Blend->AddSample(Run, FVector(RunSpeed, 0, 0));
	Blend->ResampleData();
	Blend->PostEditChange();
	FAssetRegistryModule::AssetCreated(Blend);
	Blend->MarkPackageDirty();
	Locomotion = Blend;
	MarkPackageDirty();
	SetupStatus = TEXT("Created locomotion with idle/walk/run samples. Tune sample Rate Scale in the Blend Space editor, then Save All.");
#endif
}

void UTBToyDefinition::PrepareMaterials()
{
#if WITH_EDITOR
	if (!SkeletalMesh) { SetupStatus = TEXT("Static meshes do not need skeletal material usage."); return; }
	TSet<UMaterial*> Materials;
	for (const auto& Slot : SkeletalMesh->GetMaterials())
		if (Slot.MaterialInterface) Materials.Add(Slot.MaterialInterface->GetMaterial());
	for (const auto& Override : MaterialOverrides)
		if (Override) Materials.Add(Override->GetMaterial());
	for (auto* Material : Materials)
	{
		if (Material && !Material->GetUsageByFlag(MATUSAGE_SkeletalMesh))
		{
			Material->Modify();
			Material->SetMaterialUsage(MATUSAGE_SkeletalMesh);
			Material->PostEditChange();
			Material->MarkPackageDirty();
		}
	}
	SetupStatus = TEXT("Skeletal material usage prepared. Save modified materials with Save All.");
#endif
}

void UTBToyDefinition::RebuildLocomotion()
{
#if WITH_EDITOR
	if (Locomotion)
	{
		Locomotion->Modify();
		Locomotion->ResampleData();
		Locomotion->MarkPackageDirty();
		ValidateToy();
	}
#endif
}
