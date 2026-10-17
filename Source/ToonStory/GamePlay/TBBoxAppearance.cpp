#include "TBBox.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

void ATBBox::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshAppearance();
}

void ATBBox::RefreshAppearance()
{
	if (UStaticMesh* Mesh = BoxMesh.LoadSynchronous())
	{
		Visual->SetStaticMesh(Mesh);
		const FBox Bounds = Mesh->GetBoundingBox();
		const FVector C = Bounds.GetCenter();
		const FVector E = Bounds.GetExtent();
		const FVector Centers[] = {{C.X, C.Y, Bounds.Min.Z + 3}, {Bounds.Min.X + 3, C.Y, C.Z},
		                           {Bounds.Max.X - 3, C.Y, C.Z}, {C.X, Bounds.Min.Y + 3, C.Z},
		                           {C.X, Bounds.Max.Y - 3, C.Z}, {C.X, C.Y, Bounds.Max.Z - 3}};
		const FVector Extents[] = {{E.X, E.Y, 3}, {3, E.Y, E.Z}, {3, E.Y, E.Z},
		                           {E.X, 3, E.Z}, {E.X, 3, E.Z}, {E.X, E.Y, 3}};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Centers); ++Index)
		{
			if (auto* Collision =
			        Cast<UBoxComponent>(GetDefaultSubobjectByName(*FString::Printf(TEXT("ChestCollision%d"), Index))))
			{
				Collision->SetRelativeLocation(Centers[Index]);
				Collision->SetBoxExtent(Extents[Index]);
			}
		}
	}
}
