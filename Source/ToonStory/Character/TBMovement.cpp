#include "TBMovement.h"
#include "Character/TBCharacter.h"
#include "GamePlay/TBRuleMath.h"
#include "GameFramework/PhysicsVolume.h"

void UTBMovement::PerformMovement(float DeltaSeconds)
{
	const auto* ToyCharacter = Cast<ATBCharacter>(CharacterOwner);
	if (ToyCharacter && ToyCharacter->IsPhysicsLocked())
	{
		StopMovementImmediately();
		return;
	}
	if (ToyCharacter && ToyCharacter->IsGazeFrozen())
	{
		// 移動予測・補正を維持し、凍結中は鉛直方向だけ動かす。
		if (MovementMode != MOVE_Custom || CustomMovementMode != 1)
		{
			SetMovementMode(MOVE_Custom, 1);
		}
		Acceleration = FVector::ZeroVector;
		CharacterOwner->StopJumping();
	}
	else if (MovementMode == MOVE_Custom && CustomMovementMode == 1)
	{
		SetMovementMode(MOVE_Falling);
	}
	Super::PerformMovement(DeltaSeconds);
}

// 凍結中の専用物理。横速度と上昇を止め、落下と床の衝突を処理する。
void UTBMovement::PhysCustom(float DeltaSeconds, int32 Iterations)
{
	if (CustomMovementMode != 1)
	{
		Super::PhysCustom(DeltaSeconds, Iterations);
		return;
	}
	if (!UpdatedComponent || DeltaSeconds < MIN_TICK_TIME)
	{
		return;
	}
	// 静止した床を前提とし、斜面の滑りや移動床には対応しない。
	Velocity.X = 0;
	Velocity.Y = 0;
	Velocity.Z = TBRuleMath::FrozenVerticalSpeed(Velocity.Z);
	const float OldZ = Velocity.Z;
	const auto* Volume = GetPhysicsVolume();
	Velocity.Z = FMath::Max(Velocity.Z + GetGravityZ() * DeltaSeconds, -(Volume ? Volume->TerminalVelocity : 4000.f));
	const FVector Delta(0, 0, (OldZ + Velocity.Z) * .5f * DeltaSeconds);
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit() && Hit.Normal.Z > 0)
	{
		Velocity.Z = 0;
	}
}

// サーバーが受け取った移動入力にも、凍結・運搬中の制限を適用する。
void UTBMovement::MoveAutonomous(float ClientTimeStamp, float DeltaSeconds, uint8 Flags, const FVector& NewAcceleration)
{
	const auto* ToyCharacter = Cast<ATBCharacter>(CharacterOwner);
	if (ToyCharacter && ToyCharacter->IsPhysicsLocked())
	{
		StopMovementImmediately();
		DisableMovement();
		return;
	}
	if (ToyCharacter && ToyCharacter->IsGazeFrozen())
	{
		Super::MoveAutonomous(ClientTimeStamp, DeltaSeconds, 0, FVector::ZeroVector);
		return;
	}
	Super::MoveAutonomous(ClientTimeStamp, DeltaSeconds, Flags, NewAcceleration);
}
