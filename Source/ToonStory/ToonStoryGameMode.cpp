// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToonStoryGameMode.h"
#include "ToonStory.h"
#include "Core/ToonStoryGameState.h"
#include "TimerManager.h"

AToonStoryGameMode::AToonStoryGameMode()
{
	GameStateClass = AToonStoryGameState::StaticClass();
}

void AToonStoryGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogToonStory, Log, TEXT("ToonStory GameMode BeginPlay"));
}

AToonStoryGameState* AToonStoryGameMode::GetToonGameState() const
{
	return GetGameState<AToonStoryGameState>();
}

bool AToonStoryGameMode::CanStartRound() const { return true; }
void AToonStoryGameMode::PrepareRound() {}

bool AToonStoryGameMode::TryStartRound()
{
	AToonStoryGameState* State = GetToonGameState();
	if (bStartingRound || !HasAuthority() || !State || State->GetMatchStatus().Phase != EToonMatchPhase::Waiting
		|| !FMath::IsFinite(RoundDurationSeconds) || RoundDurationSeconds <= 0.0f || !CanStartRound())
	{
		return false;
	}

	TGuardValue<bool> StartingGuard(bStartingRound, true);
	PrepareRound();
	FToonMatchStatus Status;
	Status.Phase = EToonMatchPhase::Playing;
	Status.EndServerTime = State->GetServerWorldTimeSeconds() + RoundDurationSeconds;
	GetWorldTimerManager().SetTimer(RoundDeadlineTimer, this,
		&AToonStoryGameMode::CheckRoundDeadline, RoundDurationSeconds, false);
	State->SetMatchStatus(Status);
	UE_LOG(LogToonStory, Log, TEXT("Round started (%s)"), *GetClass()->GetName());
	return true;
}

void AToonStoryGameMode::CheckRoundDeadline()
{
	AToonStoryGameState* State = GetToonGameState();
	if (!HasAuthority() || !State || State->GetMatchStatus().Phase != EToonMatchPhase::Playing) return;
	const double Remaining = State->GetRemainingSeconds();
	if (Remaining <= 0.0)
	{
		HandleTimeExpired();
	}
	else
	{
		GetWorldTimerManager().SetTimer(RoundDeadlineTimer, this,
			&AToonStoryGameMode::CheckRoundDeadline, FMath::Max(0.001f, static_cast<float>(Remaining)), false);
	}
}

bool AToonStoryGameMode::CanAcceptRoundEvent()
{
	AToonStoryGameState* State = GetToonGameState();
	if (!HasAuthority() || !State || State->GetMatchStatus().Phase != EToonMatchPhase::Playing) return false;
	if (State->GetRemainingSeconds() <= 0.0)
	{
		HandleTimeExpired();
		return false;
	}
	return true;
}

bool AToonStoryGameMode::FinishRound(FName WinningTeam, FName Reason)
{
	AToonStoryGameState* State = GetToonGameState();
	if (!HasAuthority() || !State || State->GetMatchStatus().Phase != EToonMatchPhase::Playing) return false;
	FToonMatchStatus Status = State->GetMatchStatus();
	Status.Phase = EToonMatchPhase::Finished;
	Status.WinningTeam = WinningTeam;
	Status.EndReason = Reason;
	GetWorldTimerManager().ClearTimer(RoundDeadlineTimer);
	State->SetMatchStatus(Status);
	UE_LOG(LogToonStory, Log, TEXT("Round finished: winner=%s reason=%s"), *WinningTeam.ToString(), *Reason.ToString());
	return true;
}

void AToonStoryGameMode::HandleTimeExpired()
{
	FinishRound(NAME_None, TEXT("TimeExpired"));
}

bool AToonStoryGameMode::AbortRound()
{
	return FinishRound(NAME_None, TEXT("Aborted"));
}

void AToonStoryGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RoundDeadlineTimer);
	Super::EndPlay(EndPlayReason);
}
