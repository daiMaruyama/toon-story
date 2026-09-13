#include "Character/HumanCharacter.h"

#include "Character/ToyCharacter.h"
#include "Combat/CarryComponent.h"
#include "Core/GazeFreezeSubsystem.h"
#include "Core/ToyBoxPlayerState.h"

AHumanCharacter::AHumanCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	CarryComponent = CreateDefaultSubobject<UCarryComponent>(TEXT("CarryComponent"));

	bReplicates = true;
	SetReplicateMovement(true);
}

void AHumanCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UGazeFreezeSubsystem* Gaze = UGazeFreezeSubsystem::Get(this))
	{
		Gaze->RegisterHuman(this);
	}
}

void AHumanCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGazeFreezeSubsystem* Gaze = UGazeFreezeSubsystem::Get(this))
	{
		Gaze->UnregisterHuman(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AHumanCharacter::GetGazeOrigin(FVector& OutLocation, FVector& OutDirection) const
{
	OutLocation = GetPawnViewLocation();
	OutDirection = GetViewRotation().Vector();
}

bool AHumanCharacter::CanReachToy(const AToyCharacter* Target) const
{
	if (!Target)
	{
		return false;
	}

	// 遅延ぶんの余裕を持たせた距離チェック。
	const float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (Distance > GrabRange + GrabRangeTolerance)
	{
		return false;
	}

	// 正面にいるか。
	const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	return FVector::DotProduct(GetActorForwardVector(), ToTarget) >= GrabFacingDot;
}

void AHumanCharacter::RequestCapture(AToyCharacter* Target)
{
	ServerTryCapture(Target);
}

bool AHumanCharacter::ServerTryCapture_Validate(AToyCharacter* Target)
{
	// 明らかな不正だけここで弾く。成立の可否は _Implementation 側で判断する。
	return Target != nullptr;
}

void AHumanCharacter::ServerTryCapture_Implementation(AToyCharacter* Target)
{
	// 距離・正面・相手の状態はすべて CarryComponent 側で検証する。
	if (CarryComponent)
	{
		CarryComponent->BeginGrab(Target);
	}
}

void AHumanCharacter::RequestStore()
{
	ServerTryStore();
}

void AHumanCharacter::ServerTryStore_Implementation()
{
	if (CarryComponent)
	{
		CarryComponent->BeginStore();
	}
}

void AHumanCharacter::RequestRelease()
{
	ServerRelease();
}

void AHumanCharacter::ServerRelease_Implementation()
{
	if (CarryComponent)
	{
		CarryComponent->ReleaseCarried();
	}
}
