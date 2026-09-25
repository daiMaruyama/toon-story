// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToonStoryGameMode.h"
#include "ToonStory.h"
#include "Core/ToonStoryGameState.h"
#include "Core/ToonStoryPlayerState.h"
#include "ToonStoryPlayerController.h"
#include "GameFramework/GameSession.h"
#include "TimerManager.h"

AToonStoryGameMode::AToonStoryGameMode()
{
	GameStateClass = AToonStoryGameState::StaticClass();
	PlayerStateClass = AToonStoryPlayerState::StaticClass();
	PlayerControllerClass = AToonStoryPlayerController::StaticClass();
}

void AToonStoryGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogToonStory, Log, TEXT("ToonStory GameMode BeginPlay"));
	if (bEnableMatchLoop) RefreshLobby();
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
	const bool bValidPhase = State && (State->GetMatchStatus().Phase == EToonMatchPhase::Waiting
		|| State->GetMatchStatus().Phase == EToonMatchPhase::Countdown);
	if (bStartingRound || bUpdatingLobby || !HasAuthority() || !bValidPhase
		|| !FMath::IsFinite(RoundDurationSeconds) || RoundDurationSeconds <= 0.0f || !CanStartRound())
	{
		return false;
	}
	if (bEnableMatchLoop && (!IsLobbyReady() || State->GetMatchStatus().Phase != EToonMatchPhase::Countdown
		|| State->GetPhaseRemainingSeconds() > 0.0)) return false;

	TGuardValue<bool> StartingGuard(bStartingRound, true);
	PrepareRound();
	FToonMatchStatus Status = State->GetMatchStatus();
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
	if (bEnableMatchLoop)
	{
		const float Delay = FMath::IsFinite(ResultDisplaySeconds) ? FMath::Max(0.1f, ResultDisplaySeconds) : 8.0f;
		Status.EndServerTime = State->GetServerWorldTimeSeconds() + Delay;
		GetWorldTimerManager().SetTimer(ResultTimer, this, &AToonStoryGameMode::ReturnToLobby, Delay, false);
	}
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
	GetWorldTimerManager().ClearTimer(LobbyCountdownTimer);
	GetWorldTimerManager().ClearTimer(ResultTimer);
	Super::EndPlay(EndPlayReason);
}

void AToonStoryGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	const AToonStoryGameState* State = GetToonGameState();
	if (ErrorMessage.IsEmpty() && bEnableMatchLoop && State
		&& (State->GetMatchStatus().Phase == EToonMatchPhase::Playing || State->GetMatchStatus().Phase == EToonMatchPhase::Finished))
	{
		ErrorMessage = TEXT("A round is already in progress. Join the next lobby.");
	}
}

void AToonStoryGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (!bEnableMatchLoop || !IsValid(NewPlayer)) return;
	AToonStoryPlayerState* Player = NewPlayer->GetPlayerState<AToonStoryPlayerState>();
	if (!Player)
	{
		UE_LOG(LogToonStory, Error, TEXT("Match loop requires ToonStoryPlayerState. Check the GameMode Blueprint defaults."));
		return;
	}
	const AToonStoryGameState* State = GetToonGameState();
	// A connection accepted before the countdown ended may finish login after it.
	if (State && (State->GetMatchStatus().Phase == EToonMatchPhase::Playing || State->GetMatchStatus().Phase == EToonMatchPhase::Finished))
	{
		if (GameSession) GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("Round already started.")));
		return;
	}
	CancelCountdown();
	LobbyPlayers.AddUnique(NewPlayer);
	Player->SetMatchReady(false);
	RebuildLobbyRoles();
	RefreshLobby();
}

void AToonStoryGameMode::Logout(AController* Exiting)
{
	if (bEnableMatchLoop)
	{
		const AToonStoryGameState* State = GetToonGameState();
		if (State && State->GetMatchStatus().Phase == EToonMatchPhase::Playing) AbortRound();
		CancelCountdown();
		LobbyPlayers.RemoveAll([Exiting](const TWeakObjectPtr<APlayerController>& Player)
		{
			return !Player.IsValid() || Player.Get() == Exiting;
		});
		RebuildLobbyRoles();
		RefreshLobby();
	}
	Super::Logout(Exiting);
}

void AToonStoryGameMode::RebuildLobbyRoles() {}

bool AToonStoryGameMode::SetPlayerReady(APlayerController* Player, bool bReady)
{
	AToonStoryGameState* State = GetToonGameState();
	if (!HasAuthority() || !bEnableMatchLoop || bUpdatingLobby || bStartingRound || !State || !IsValid(Player)
		|| !LobbyPlayers.Contains(Player)) return false;
	const EToonMatchPhase Phase = State->GetMatchStatus().Phase;
	if (Phase != EToonMatchPhase::Waiting && Phase != EToonMatchPhase::Countdown) return false;
	AToonStoryPlayerState* PlayerState = Player->GetPlayerState<AToonStoryPlayerState>();
	if (!PlayerState) return false;
	{
		TGuardValue<bool> Guard(bUpdatingLobby, true);
		PlayerState->SetMatchReady(bReady);
	}
	RefreshLobby();
	return true;
}

bool AToonStoryGameMode::IsLobbyReady() const
{
	if (LobbyPlayers.Num() < FMath::Max(1, MinimumPlayers)) return false;
	for (const auto& Entry : LobbyPlayers)
	{
		const AToonStoryPlayerState* Player = Entry.IsValid() ? Entry->GetPlayerState<AToonStoryPlayerState>() : nullptr;
		if (!Player || !Player->IsMatchReady()) return false;
	}
	return true;
}

void AToonStoryGameMode::RefreshLobby()
{
	AToonStoryGameState* State = GetToonGameState();
	if (!HasAuthority() || !bEnableMatchLoop || bUpdatingLobby || !State) return;
	FToonMatchStatus Status = State->GetMatchStatus();
	if (Status.Phase != EToonMatchPhase::Waiting && Status.Phase != EToonMatchPhase::Countdown) return;
	TGuardValue<bool> Guard(bUpdatingLobby, true);
	Status.ConnectedPlayers = LobbyPlayers.Num();
	Status.ReadyPlayers = 0;
	Status.MinimumPlayers = FMath::Max(1, MinimumPlayers);
	for (const auto& Entry : LobbyPlayers)
	{
		const AToonStoryPlayerState* Player = Entry.IsValid() ? Entry->GetPlayerState<AToonStoryPlayerState>() : nullptr;
		if (Player && Player->IsMatchReady()) ++Status.ReadyPlayers;
	}
	const bool bCanStart = IsLobbyReady() && CanStartRound();
	if (!bCanStart)
	{
		GetWorldTimerManager().ClearTimer(LobbyCountdownTimer);
		Status.Phase = EToonMatchPhase::Waiting;
		Status.EndServerTime = 0;
	}
	else if (Status.Phase == EToonMatchPhase::Waiting)
	{
		const float Delay = FMath::IsFinite(StartCountdownSeconds) ? FMath::Max(0.1f, StartCountdownSeconds) : 3.0f;
		Status.Phase = EToonMatchPhase::Countdown;
		Status.EndServerTime = State->GetServerWorldTimeSeconds() + Delay;
		GetWorldTimerManager().SetTimer(LobbyCountdownTimer, this, &AToonStoryGameMode::CompleteCountdown, Delay, false);
	}
	State->SetMatchStatus(Status);
}

void AToonStoryGameMode::CancelCountdown()
{
	AToonStoryGameState* State = GetToonGameState();
	if (!State || State->GetMatchStatus().Phase != EToonMatchPhase::Countdown) return;
	GetWorldTimerManager().ClearTimer(LobbyCountdownTimer);
	FToonMatchStatus Status = State->GetMatchStatus();
	Status.Phase = EToonMatchPhase::Waiting;
	Status.EndServerTime = 0;
	State->SetMatchStatus(Status);
}

void AToonStoryGameMode::CompleteCountdown()
{
	const AToonStoryGameState* State = GetToonGameState();
	if (!State || State->GetMatchStatus().Phase != EToonMatchPhase::Countdown) return;
	if (!IsLobbyReady() || !CanStartRound()) { CancelCountdown(); RefreshLobby(); return; }
	const double Remaining = State->GetPhaseRemainingSeconds();
	if (Remaining > 0.0)
	{
		GetWorldTimerManager().SetTimer(LobbyCountdownTimer, this, &AToonStoryGameMode::CompleteCountdown,
			FMath::Max(0.001f, static_cast<float>(Remaining)), false);
		return;
	}
	if (!TryStartRound()) CancelCountdown();
}

void AToonStoryGameMode::ReturnToLobby()
{
	const AToonStoryGameState* State = GetToonGameState();
	if (!HasAuthority() || !State || State->GetMatchStatus().Phase != EToonMatchPhase::Finished) return;
	// Reload world actors too: resetting only scores would leave consumed batteries and captured pawns behind.
	if (!GetWorld()->ServerTravel(TEXT("?Restart"), false))
	{
		UE_LOG(LogToonStory, Error, TEXT("Could not restart the map; preserving the finished result."));
	}
}
