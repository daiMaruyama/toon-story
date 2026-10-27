#include "TBCharacter.h"
#include "TBToyPresentation.h"
#include "TBCharacterGeometry.h"
#include "Data/TBCharacterData.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void ATBCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// The human mesh is fitted at runtime; the Blueprint default mesh offset is not its standing baseline.
	const FVector Offset = StandingMeshOffset + FVector(0, 0, HalfHeightAdjust);
	GetMesh()->GetRelativeLocation_DirectMutable() = Offset;
	CacheInitialMeshOffset(Offset, GetMesh()->GetRelativeRotation());
}

void ATBCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	GetMesh()->GetRelativeLocation_DirectMutable() = StandingMeshOffset;
	CacheInitialMeshOffset(StandingMeshOffset, GetMesh()->GetRelativeRotation());
}

FVector ATBCharacter::GetCarryGripLocation() const
{
	// A stable target in front of the torso avoids feeding the animated hand back into the arm IK.
	return GetActorLocation() + GetActorForwardVector() * (bIsCrouched ? 23.f : 29.f) +
	       FVector(0, 0, bIsCrouched ? 12.f : 6.f);
}

void ATBCharacter::CrouchPressed()
{
	if (IsHuman() && CanAct())
	{
		Crouch();
	}
}

void ATBCharacter::CrouchReleased()
{
	UnCrouch();
}

void ATBCharacter::InitializeComfortMovement()
{
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = true;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(TBCharacterGeometry::HumanCrouchedHalfHeight);
	CrouchedEyeHeight = 26.f;
	bUseControllerRotationYaw = false;
}

void ATBCharacter::ApplyComfortBody(bool Toy)
{
	// Recreating the collision shape each frame causes unnecessary overlap/penetration corrections at walls.
	if (!FMath::IsNearlyEqual(GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),
	                          Toy ? TBCharacterGeometry::ToyHalfHeight : (bIsCrouched ? TBCharacterGeometry::HumanCrouchedHalfHeight : TBCharacterGeometry::HumanHalfHeight)))
	{
		GetCapsuleComponent()->SetCapsuleSize(Toy ? TBCharacterGeometry::ToyRadius : TBCharacterGeometry::HumanRadius,
		                                      Toy ? TBCharacterGeometry::ToyHalfHeight : (bIsCrouched ? TBCharacterGeometry::HumanCrouchedHalfHeight : TBCharacterGeometry::HumanHalfHeight));
	}
	BaseEyeHeight = Toy ? 7.f * TBToyPresentation::SizeScale : (bIsCrouched ? 26.f : 42.f);
	Camera->SetRelativeLocation(FVector(0, 0, BaseEyeHeight));
}

void ATBCharacter::ApplyComfortMovement(bool Toy)
{
	auto* Movement = GetCharacterMovement();
	const auto& Feel = TB::CharacterData();
	Movement->GetNavAgentPropertiesRef().bCanCrouch = !Toy;
	Movement->MaxWalkSpeedCrouched = Feel.Human.WalkSpeed * Feel.Human.CrouchSpeedRatio;
	Movement->MaxWalkSpeed =
	    Toy ? Feel.Toy.WalkSpeed : ((CarriedToy || CarriedDummy) ? Feel.Human.CarrySpeed : Feel.Human.WalkSpeed);
	// Follow look yaw while the directional locomotion handles strafing and backward steps.
	bUseControllerRotationYaw = false;
	Movement->bUseControllerDesiredRotation = !IsMovementLocked();
	Movement->bOrientRotationToMovement = false;
	Movement->RotationRate = FRotator(0, Feel.View.TurnRate, 0);
}
