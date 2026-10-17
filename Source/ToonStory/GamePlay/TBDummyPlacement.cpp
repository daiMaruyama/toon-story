#include "TBDummyToy.h"
#include "Character/TBCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void ATBDummyToy::OnRep_Placement()
{
	SettleFrames = 12;
	SetActorTickEnabled(true);
}

void ATBDummyToy::Place(const FVector& Position, ATBBox* Box)
{
	if (!HasAuthority())
	{
		return;
	}
	auto* Previous = Holder.Get();
	Holder = nullptr;
	StoredBox = Box;
	bFalling = true;
	FallSpeed = 0;
	OnRep_Holder();
	SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
	if (Previous)
	{
		Previous->CarriedDummy = nullptr;
		Previous->ClearHeldInteraction();
		Previous->ApplyState();
		Previous->ForceNetUpdate();
	}
	ForceNetUpdate();
}

void ATBDummyToy::Fall(float DeltaSeconds)
{
	FallSpeed = FMath::Min(FallSpeed + 980.f * DeltaSeconds, 4000.f);
	const FVector Start = Collision->GetComponentLocation();
	const FVector End = Start - FVector(0, 0, FallSpeed * DeltaSeconds);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DecoyFall), false, this);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(
	        Hit, Start, End, FQuat::Identity, ECC_GameTraceChannel1,
	        FCollisionShape::MakeCapsule(Collision->GetScaledCapsuleRadius(), Collision->GetScaledCapsuleHalfHeight()),
	        Params))
	{
		SetActorLocation(Hit.Location - Collision->GetRelativeLocation());
		bFalling = false;
		FallSpeed = 0;
		OnRep_Placement();
		ForceNetUpdate();
	}
	else
	{
		SetActorLocation(GetActorLocation() + End - Start);
	}
}
