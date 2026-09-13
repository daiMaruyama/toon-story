#include "Character/HumanCharacter.h"

#include "Core/GazeFreezeSubsystem.h"

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
