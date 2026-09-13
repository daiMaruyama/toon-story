#include "Character/HumanCharacter.h"

#include "Character/ToyCharacter.h"
#include "Core/GazeFreezeSubsystem.h"
#include "Core/ToyBoxPlayerState.h"

AHumanCharacter::AHumanCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

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
	AToyBoxPlayerState* TargetPS = Target ? Target->GetPlayerState<AToyBoxPlayerState>() : nullptr;
	if (!TargetPS || TargetPS->ToyState != EToyState::Free)
	{
		return;
	}

	if (!CanReachToy(Target))
	{
		return;
	}

	// M3 の簡易版。M6 で Grabbed → Carried → Storing を挟むようになる。
	TargetPS->SetToyState(EToyState::Boxed);
}
