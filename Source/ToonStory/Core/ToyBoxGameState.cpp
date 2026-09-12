#include "Core/ToyBoxGameState.h"

#include "Core/ToyBoxPlayerState.h"
#include "Net/UnrealNetwork.h"

AToyBoxGameState::AToyBoxGameState()
{
}

void AToyBoxGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AToyBoxGameState, Settings);
	DOREPLIFETIME(AToyBoxGameState, Phase);
	DOREPLIFETIME(AToyBoxGameState, Result);
	DOREPLIFETIME(AToyBoxGameState, MatchEndServerTime);
	DOREPLIFETIME(AToyBoxGameState, CollectedItems);
}

float AToyBoxGameState::GetRemainingSeconds() const
{
	if (Phase != EMatchPhase::InProgress)
	{
		return 0.f;
	}

	// GetServerWorldTimeSeconds() は double を返すので、計算も double で揃える。
	const double Remaining = static_cast<double>(MatchEndServerTime) - GetServerWorldTimeSeconds();
	return static_cast<float>(FMath::Max(0.0, Remaining));
}

int32 AToyBoxGameState::CountBoxedToys() const
{
	int32 Count = 0;
	for (const APlayerState* PS : PlayerArray)
	{
		const AToyBoxPlayerState* ToyPS = Cast<AToyBoxPlayerState>(PS);
		if (ToyPS && ToyPS->IsToy() && ToyPS->IsBoxed())
		{
			++Count;
		}
	}
	return Count;
}

int32 AToyBoxGameState::CountFreeToys() const
{
	int32 Count = 0;
	for (const APlayerState* PS : PlayerArray)
	{
		const AToyBoxPlayerState* ToyPS = Cast<AToyBoxPlayerState>(PS);
		if (ToyPS && ToyPS->IsToy() && !ToyPS->IsBoxed())
		{
			++Count;
		}
	}
	return Count;
}

int32 AToyBoxGameState::CountPlayersOnTeam(ETeamId Team) const
{
	int32 Count = 0;
	for (const APlayerState* PS : PlayerArray)
	{
		const AToyBoxPlayerState* ToyPS = Cast<AToyBoxPlayerState>(PS);
		if (ToyPS && ToyPS->TeamId == Team)
		{
			++Count;
		}
	}
	return Count;
}

void AToyBoxGameState::SetSettings(const FMatchSettings& NewSettings)
{
	if (!HasAuthority())
	{
		return;
	}

	Settings = NewSettings;
	OnRep_Settings();
}

void AToyBoxGameState::SetPhase(EMatchPhase NewPhase)
{
	if (!HasAuthority() || Phase == NewPhase)
	{
		return;
	}

	Phase = NewPhase;
	OnRep_Phase();
}

void AToyBoxGameState::SetResult(EMatchResult NewResult)
{
	if (HasAuthority())
	{
		Result = NewResult;
	}
}

void AToyBoxGameState::SetMatchEndServerTime(float NewEndTime)
{
	if (HasAuthority())
	{
		MatchEndServerTime = NewEndTime;
	}
}

void AToyBoxGameState::SetCollectedItems(int32 NewCount)
{
	if (HasAuthority())
	{
		CollectedItems = FMath::Max(0, NewCount);
	}
}

void AToyBoxGameState::OnRep_Settings()
{
	OnMatchSettingsChanged.Broadcast(Settings);
}

void AToyBoxGameState::OnRep_Phase()
{
	OnMatchPhaseChanged.Broadcast(Phase);
}
