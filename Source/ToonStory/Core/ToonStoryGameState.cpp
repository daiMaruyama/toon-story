#include "Core/ToonStoryGameState.h"
#include "Net/UnrealNetwork.h"

double AToonStoryGameState::GetRemainingSeconds() const
{
	return MatchStatus.Phase == EToonMatchPhase::Playing
		? FMath::Max(0.0, MatchStatus.EndServerTime - GetServerWorldTimeSeconds()) : 0.0;
}

void AToonStoryGameState::SetMatchStatus(const FToonMatchStatus& NewStatus)
{
	if (!HasAuthority()) return;
	MatchStatus = NewStatus;
	ForceNetUpdate();
	OnRep_MatchStatus(); // C++ authority writes also notify the listen-server UI.
}

void AToonStoryGameState::OnRep_MatchStatus()
{
	OnMatchStatusChanged.Broadcast();
}

void AToonStoryGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AToonStoryGameState, MatchStatus);
}
