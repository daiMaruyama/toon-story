#include "TBToyCarVisual.h"
#include "TBCharacter.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ATBToyCarVisual::ATBToyCarVisual()
{
	PrimaryActorTick.bCanEverTick = false;
	auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Car = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Car"));
	Car->SetupAttachment(Root);
	Car->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Car->SetGenerateOverlapEvents(false);
	Car->SetCanEverAffectNavigation(false);
	Car->SetRelativeRotation(FRotator(0, -90, 0));
	Car->SetRelativeScale3D(FVector(.85f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/ToyWorld/Meshes/SM_ToyCar_01"));
	Car->SetStaticMesh(Mesh.Object);
}

void ATBToyCarVisual::Animate(float Speed, bool Frozen)
{
	const auto* Pawn = Cast<ATBCharacter>(GetOwner());
	if (!Pawn)
	{
		return;
	}
	if (Pawn->Carrier)
	{
		SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	else if (!Frozen && Speed > 1.f)
	{
		// A wheeled toy faces its travel direction, including strafing, without changing the player's camera.
		const FVector Velocity = Pawn->GetVelocity();
		SetActorRotation(FRotator(0, Velocity.Rotation().Yaw, 0));
	}
}

void ATBToyCarVisual::Ground(UChildActorComponent* Visual, float FloorZ)
{
	const FBox Bounds = Car->CalcBounds(Car->GetComponentTransform()).GetBox();
	Visual->AddWorldOffset(FVector(0, 0, FloorZ - Bounds.Min.Z));
}

void ATBToyCarVisual::Grip(UChildActorComponent* Visual, const FVector& Position)
{
	SetActorRelativeRotation(FRotator::ZeroRotator);
	const FVector Center = Car->CalcBounds(Car->GetComponentTransform()).Origin;
	Visual->AddWorldOffset(Position - Center);
}

bool ATBToyCarVisual::CarryContacts(const ATBCharacter* Human, FVector& Left, FVector& Right) const
{
	if (!Human || !Car->GetStaticMesh())
	{
		return false;
	}
	// Support the underside from both sides, leaving room for the palms outside the narrow body.
	const FBox Bounds = Car->GetStaticMesh()->GetBoundingBox();
	const float HalfWidth = Bounds.GetExtent().X * Car->GetComponentScale().X;
	const float HalfHeight = Bounds.GetExtent().Z * Car->GetComponentScale().Z;
	const FVector Center = Human->GetCarryGripLocation() - FVector(0, 0, HalfHeight * .45f);
	Left = Center - Human->GetActorRightVector() * (HalfWidth + 3.f);
	Right = Center + Human->GetActorRightVector() * (HalfWidth + 3.f);
	return true;
}
