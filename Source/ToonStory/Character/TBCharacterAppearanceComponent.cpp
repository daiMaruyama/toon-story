#include "TBCharacterAppearanceComponent.h"
#include "TBCharacter.h"
#include "TBToyAnimationComponent.h"
#include "Data/TBToyDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

void UTBCharacterAppearanceComponent::SetDefinition(UTBToyDefinition* Definition)
{
	LoadedToyDefinition = Definition;
	bAppearanceInitialized = false;
}

void UTBCharacterAppearanceComponent::Apply(ATBCharacter& Character, UTBToyAnimationComponent& Animation, bool bToy)
{
	if (!LoadedToyDefinition || (bAppearanceInitialized && bShowingToy == bToy)) return;
	bAppearanceInitialized = true;
	bShowingToy = bToy;
	auto* Visual = Character.GetMesh();
	const float ChildRate = Visual->GlobalAnimRateScale;
	Animation.Reset();
	Visual->EmptyOverrideMaterials();
	Character.Body->EmptyOverrideMaterials();
	if (!bToy)
	{
		// TBCharacter already applied the child's mesh, animation and scale on the role transition.
		Visual->GlobalAnimRateScale = ChildRate;
		return;
	}
	const auto ApplyMaterials = [this](UMeshComponent* Component)
	{
		const auto& Materials = LoadedToyDefinition->MaterialOverrides;
		for (int32 Index = 0; Index < FMath::Min(Materials.Num(), Component->GetNumMaterials()); ++Index)
		{
			if (Materials[Index]) Component->SetMaterial(Index, Materials[Index]);
		}
	};
	const float Height = FMath::Max(1.f, LoadedToyDefinition->Height);
	const FRotator Facing(0, LoadedToyDefinition->FacingYaw, 0);
	if (auto* Mesh = LoadedToyDefinition->SkeletalMesh.Get())
	{
		Character.Body->SetVisibility(false);
		Visual->SetSkeletalMesh(Mesh);
		ApplyMaterials(Visual);
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const float Scale = Height / FMath::Max(1.f, float(Bounds.BoxExtent.Z * 2));
		Visual->SetRelativeScale3D(FVector(Scale));
		Visual->SetRelativeLocation(FVector(0, 0, -13) - FVector(0, 0, Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale);
		Visual->SetRelativeRotation(Facing);
		Visual->SetVisibility(true);
		Animation.Configure(Visual, LoadedToyDefinition, Character.ToySpeed, Character.ToySprintSpeed);
	}
	else if (auto* StaticMesh = LoadedToyDefinition->StaticMesh.Get())
	{
		Visual->SetVisibility(false);
		Visual->SetRelativeTransform(FTransform(FVector(0, 0, -13)));
		Character.Body->SetStaticMesh(StaticMesh);
		ApplyMaterials(Character.Body);
		const FBoxSphereBounds Bounds = StaticMesh->GetBounds();
		const float Scale = Height / FMath::Max(1.f, float(Bounds.BoxExtent.Z * 2));
		Character.Body->SetRelativeScale3D(FVector(Scale));
		Character.Body->SetRelativeRotation(Facing);
		Character.Body->SetRelativeLocation(-Facing.RotateVector(FVector(Bounds.Origin.X, Bounds.Origin.Y,
			Bounds.Origin.Z - Bounds.BoxExtent.Z)) * Scale);
		Character.Body->SetVisibility(true);
	}
	// Preserve network smoothing and the existing carry-camera anchor when mesh proportions change.
	Character.CacheInitialMeshOffset(Visual->GetRelativeLocation(), Visual->GetRelativeRotation());
	const FTransform MeshTransform = Visual->GetRelativeTransform();
	Character.CarryAnchor->SetRelativeLocation(MeshTransform.InverseTransformPosition(FVector(95, 0, 10)));
	Character.CarryAnchor->SetRelativeRotation(MeshTransform.GetRotation().Inverse());
}
