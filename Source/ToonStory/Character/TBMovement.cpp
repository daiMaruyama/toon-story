// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBMovement.h"
#include "Character/TBCharacter.h"
#include "Character/TBNPCController.h"
#include "Data/TBCharacterData.h"
#include "GamePlay/TBRuleMath.h"
#include "GameFramework/PhysicsVolume.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void UTBMovement::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration,
    const FVector& ImpactVelocity)
{
	auto* Part = Impact.GetComponent();
	if (!Part || !Part->ComponentHasTag(TEXT("TBPushable")))
	{
		Super::ApplyImpactPhysicsForces(Impact, ImpactAcceleration, ImpactVelocity);
		return;
	}
	// Small props use a modest server-authoritative push instead of the default
	// character force, which is tuned for much heavier physics objects.
	if (!CharacterOwner || !CharacterOwner->HasAuthority() || !Part->IsSimulatingPhysics()) return;
	const FVector Direction = FVector(ImpactVelocity.X, ImpactVelocity.Y, 0).GetSafeNormal();
	const auto* Pawn = Cast<ATBCharacter>(CharacterOwner);
	if (!Pawn || !Pawn->IsHuman()) return;
	const float DesiredSpeed = FMath::Min(float(ImpactVelocity.Size2D()) * 1.25f, 360.f);
	const float CurrentSpeed = FVector::DotProduct(Part->GetPhysicsLinearVelocity(), Direction);
	const float DeltaSpeed = FMath::Clamp(DesiredSpeed - CurrentSpeed, 0.f, 280.f);
	if (!Direction.IsNearlyZero() && DeltaSpeed > 0)
		Part->AddImpulseAtLocation(Direction * Part->GetMass() * DeltaSpeed, Impact.ImpactPoint);
}

void UTBMovement::SweepPushableProps(float DeltaSeconds)
{
	bBrushingProp = false;
	const FVector Approach = (Velocity + Acceleration * DeltaSeconds) * FVector(1, 1, 0);
	if (!CharacterOwner || Approach.IsNearlyZero() || !UpdatedComponent) return;
	const auto* Capsule = CharacterOwner->GetCapsuleComponent();
	const FVector Start = Capsule->GetComponentLocation();
	FCollisionObjectQueryParams Types(ECC_PhysicsBody);
	Types.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Contacts;
	GetWorld()->SweepMultiByObjectType(Contacts, Start, Start + Approach * DeltaSeconds, Capsule->GetComponentQuat(),
	    Types, Capsule->GetCollisionShape(), FCollisionQueryParams(SCENE_QUERY_STAT(TBPushableSweep), false, CharacterOwner));
	for (const auto& Hit : Contacts)
	{
		auto* Part = Hit.GetComponent();
		if (!Part || !Part->ComponentHasTag(TEXT("TBPushable")) ||
		    FVector::DotProduct(Approach, Part->Bounds.Origin - Start) <= 0) continue;
		FHitResult Contact = Hit;
		if (Hit.bStartPenetrating) Contact.ImpactPoint = Part->Bounds.Origin;
		// The object query excludes static walls; check occlusion before applying contact effects.
		if (!TB::ClearPath(CharacterOwner, Hit.GetActor(), Start, Contact.ImpactPoint)) continue;
		bBrushingProp = true;
		ApplyImpactPhysicsForces(Contact, Acceleration, Approach);
	}
}

namespace
{
	class FTBSavedMove : public FSavedMove_Character
	{
	public:
		bool bSavedSprint = false;
		virtual void Clear() override
		{
			Super::Clear();
			bSavedSprint = false;
		}
		virtual uint8 GetCompressedFlags() const override
		{
			return Super::GetCompressedFlags() | (bSavedSprint ? FLAG_Custom_0 : 0);
		}
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
		{
			return bSavedSprint == static_cast<const FTBSavedMove*>(NewMove.Get())->bSavedSprint &&
			       Super::CanCombineWith(NewMove, Character, MaxDelta);
		}
		virtual void SetMoveFor(ACharacter* Character, float Delta, const FVector& Accel,
		                        FNetworkPredictionData_Client_Character& Data) override
		{
			Super::SetMoveFor(Character, Delta, Accel, Data);
			bSavedSprint = CastChecked<UTBMovement>(Character->GetCharacterMovement())->bWantsToSprint;
		}
		virtual void PrepMoveFor(ACharacter* Character) override
		{
			Super::PrepMoveFor(Character);
			CastChecked<UTBMovement>(Character->GetCharacterMovement())->bWantsToSprint = bSavedSprint;
		}

	private:
		using Super = FSavedMove_Character;
	};

	class FTBClientPredictionData : public FNetworkPredictionData_Client_Character
	{
	public:
		explicit FTBClientPredictionData(const UCharacterMovementComponent& Movement)
		    : FNetworkPredictionData_Client_Character(Movement)
		{
		}
		virtual FSavedMovePtr AllocateNewMove() override
		{
			return FSavedMovePtr(new FTBSavedMove());
		}
	};
} // namespace

float UTBMovement::GetMaxSpeed() const
{
	const auto* Character = Cast<ATBCharacter>(CharacterOwner);
	if (Character && Character->IsMovementLocked())
	{
		return 0.f;
	}
	if (Character && Character->TBPS() && Character->TBPS()->Team == ETBTeam::Toy && (IsMovingOnGround() || IsFalling()))
	{
		const float Speed = bWantsToSprint ? TB::CharacterData().Toy.SprintSpeed : Super::GetMaxSpeed();
		return bBrushingProp ? .85f * Speed : Speed;
	}
	if (Character && Character->IsHuman())
		if (const auto* NPC = Cast<ATBNPCController>(Character->GetController()))
			return Super::GetMaxSpeed() * FMath::Clamp(NPC->HumanSpeedScale, .1f, 1.f);
	return Super::GetMaxSpeed();
}

void UTBMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UTBMovement::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		auto* MutableThis = const_cast<UTBMovement*>(this);
		MutableThis->ClientPredictionData = new FTBClientPredictionData(*this);
	}
	return ClientPredictionData;
}

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
	SweepPushableProps(DeltaSeconds);
	Super::PerformMovement(DeltaSeconds);
	bBrushingProp = false;
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
	// Preserve walking's floor clearance when freezing; gravity must not close that gap.
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	if (CurrentFloor.IsWalkableFloor() && CurrentFloor.GetDistanceToFloor() <= MAX_FLOOR_DIST &&
	    CurrentFloor.GetDistanceToFloor() >= MIN_FLOOR_DIST && Velocity.Z <= 0)
	{
		Velocity = FVector::ZeroVector;
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
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		if (CurrentFloor.IsWalkableFloor()) AdjustFloorHeight();
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
		// 凍結でジャンプ入力は捨てるが、押し続けているダッシュ入力は復帰後へ保持する。
		Super::MoveAutonomous(ClientTimeStamp, DeltaSeconds, Flags & FSavedMove_Character::FLAG_Custom_0,
		                      FVector::ZeroVector);
		return;
	}
	Super::MoveAutonomous(ClientTimeStamp, DeltaSeconds, Flags, NewAcceleration);
}
