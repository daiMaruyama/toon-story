#include "GamePlay/Items/TBEscapeVehicleMovement.h"
#include "Data/TBCharacterData.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

namespace
{
	// CharacterMovement が床との間に保つ距離と同じ範囲の中間値。
	constexpr float FloorClearance = 2.15f;
	constexpr float WalkableNormalZ = .71f;
}

UTBEscapeVehicleMovement::UTBEscapeVehicleMovement()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UTBEscapeVehicleMovement::TryStep(const FVector& Delta, float StepHeight, const FQuat& Rotation)
{
	if (!UpdatedComponent || Delta.IsNearlyZero() || StepHeight <= 0.f) return false;
	FScopedMovementUpdate Step(UpdatedComponent, EScopedUpdate::DeferredUpdates);
	FHitResult Up, Forward, Down;
	SafeMoveUpdatedComponent(FVector(0, 0, StepHeight + FloorClearance), Rotation, true, Up);
	if (Up.bBlockingHit || Up.bStartPenetrating) { Step.RevertMove(); return false; }
	SafeMoveUpdatedComponent(Delta, Rotation, true, Forward);
	if (Forward.bBlockingHit || Forward.bStartPenetrating) { Step.RevertMove(); return false; }
	SafeMoveUpdatedComponent(FVector(0, 0, -(StepHeight + 2.f * FloorClearance)), Rotation, true, Down);
	if (!Down.IsValidBlockingHit() || Down.ImpactNormal.Z < WalkableNormalZ)
	{ Step.RevertMove(); return false; }
	FHitResult Lift;
	SafeMoveUpdatedComponent(FVector(0, 0, FloorClearance), Rotation, true, Lift);
	bGrounded = true;
	VerticalSpeed = 0.f;
	return true;
}

void UTBEscapeVehicleMovement::Advance(const FVector& HorizontalDelta, const FRotator& Rotation, float DeltaSeconds)
{
	if (!UpdatedComponent || !GetWorld() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
	const float Dt = FMath::Min(DeltaSeconds, .1f);
	const FQuat Orientation = Rotation.Quaternion();
	FHitResult HorizontalHit;
	SafeMoveUpdatedComponent(HorizontalDelta, Orientation, true, HorizontalHit);
	if (HorizontalHit.IsValidBlockingHit())
	{
		const FVector Remaining = HorizontalDelta * (1.f - HorizontalHit.Time);
		const float Configured = TB::CharacterData().Toy.MaxStepHeight;
		const float StepHeight = FMath::IsFinite(Configured) ? FMath::Clamp(Configured, 0.f, 100.f) : 0.f;
		if (!bGrounded || HorizontalHit.ImpactNormal.Z >= WalkableNormalZ || !TryStep(Remaining, StepHeight, Orientation))
			SlideAlongSurface(HorizontalDelta, 1.f - HorizontalHit.Time, HorizontalHit.Normal, HorizontalHit, true);
	}
	VerticalSpeed = FMath::Max(VerticalSpeed + GetWorld()->GetGravityZ() * Dt, -4000.f);
	FHitResult FloorHit;
	SafeMoveUpdatedComponent(FVector(0, 0, VerticalSpeed * Dt), Orientation, true, FloorHit);
	bGrounded = FloorHit.IsValidBlockingHit() && FloorHit.ImpactNormal.Z >= WalkableNormalZ;
	if (FloorHit.bBlockingHit)
	{
		VerticalSpeed = 0.f;
		if (bGrounded)
		{
			FHitResult Lift;
			SafeMoveUpdatedComponent(FVector(0, 0, FloorClearance), Orientation, true, Lift);
		}
	}
	else if (UpdatedPrimitive && VerticalSpeed <= 0.f)
	{
		// 着地直後の隙間でも接地を維持する。フレームごとに落下扱いにすると段差へ乗れない。
		FCollisionQueryParams Query(SCENE_QUERY_STAT(TBEscapeVehicleFloor), false, GetOwner());
		FCollisionResponseParams Responses;
		InitCollisionParams(Query, Responses);
		FHitResult Probe;
		const FVector Start = UpdatedComponent->GetComponentLocation();
		const bool bFound = GetWorld()->SweepSingleByChannel(Probe, Start,
		    Start - FVector(0, 0, FloorClearance * 2.f), Orientation,
		    UpdatedPrimitive->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(), Query, Responses);
		if (bFound && Probe.IsValidBlockingHit() && Probe.ImpactNormal.Z >= WalkableNormalZ)
		{
			FHitResult Adjustment;
			SafeMoveUpdatedComponent(FVector(0, 0, FloorClearance - Probe.Distance), Orientation, true, Adjustment);
			bGrounded = true;
			VerticalSpeed = 0.f;
		}
	}
}
