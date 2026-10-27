// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBGameState.h"
#include "GamePlay/TBBox.h"
#include "Net/UnrealNetwork.h"

void ATBGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBGameState, Settings);
	DOREPLIFETIME(ATBGameState, StageName);
	DOREPLIFETIME(ATBGameState, SelectedStage);
	DOREPLIFETIME(ATBGameState, bTravelPending);
	DOREPLIFETIME(ATBGameState, PlayerLimit);
	DOREPLIFETIME(ATBGameState, Phase);
	DOREPLIFETIME(ATBGameState, Winner);
	DOREPLIFETIME(ATBGameState, Reason);
	DOREPLIFETIME(ATBGameState, EndTime);
	DOREPLIFETIME(ATBGameState, Collected);
	DOREPLIFETIME(ATBGameState, BoxedCount);
	DOREPLIFETIME(ATBGameState, Box);
	DOREPLIFETIME(ATBGameState, HidePreparation);
}

float ATBGameState::HideProgress() const
{
	const double Duration = HidePreparation.EndsAt - HidePreparation.StartsAt;
	return HidePreparation.bValid && Duration > 0
	    ? FMath::Clamp((GetServerWorldTimeSeconds() - HidePreparation.StartsAt) / Duration, 0.0, 1.0) : 0.f;
}

bool ATBGameState::IsPreparing() const
{
	return Phase == ETBPhase::Playing && (!HidePreparation.bValid || HidePreparation.bHiding);
}

float ATBGameState::HideRemaining() const
{
	return IsPreparing() && HidePreparation.bValid
	    ? FMath::Max(0., HidePreparation.EndsAt - GetServerWorldTimeSeconds()) : 0.f;
}

float ATBGameState::Remaining() const
{
	if (IsPreparing()) return Settings.Duration;
	return Phase == ETBPhase::Playing ? FMath::Max(0., EndTime - GetServerWorldTimeSeconds()) : 0.f;
}
