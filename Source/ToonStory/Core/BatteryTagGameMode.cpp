#include "Core/BatteryTagGameMode.h"
#include "Core/BatteryTagGameState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Controller.h"
#include "Core/ToonStoryPlayerState.h"
#include "Core/ToonMatchHUD.h"
#include "GameFramework/PlayerController.h"

ABatteryTagGameMode::ABatteryTagGameMode()
{
	GameStateClass = ABatteryTagGameState::StaticClass();
	bEnableMatchLoop = true;
	HUDClass = AToonMatchHUD::StaticClass();
}

bool ABatteryTagGameMode::RegisterToy(APlayerState* Player)
{
	if (bEnableMatchLoop || bProcessingRuleEvent) return false;
	TGuardValue<bool> EventGuard(bProcessingRuleEvent, true);
	const AToonStoryGameState* State = GetToonGameState();
	if (!HasAuthority() || !State || State->GetMatchStatus().Phase != EToonMatchPhase::Waiting
		|| !IsValid(Player) || Player->GetWorld() != GetWorld() || Toys.Contains(Player)) return false;
	Toys.Add(Player, false);
	PublishProgress();
	return true;
}

bool ABatteryTagGameMode::UnregisterToy(APlayerState* Player)
{
	if (bEnableMatchLoop || bProcessingRuleEvent) return false;
	TGuardValue<bool> EventGuard(bProcessingRuleEvent, true);
	const AToonStoryGameState* State = GetToonGameState();
	if (!HasAuthority() || !State || State->GetMatchStatus().Phase != EToonMatchPhase::Waiting
		|| !Player || Toys.Remove(Player) == 0) return false;
	PublishProgress();
	return true;
}

bool ABatteryTagGameMode::IsRosterValid() const
{
	if (Toys.IsEmpty()) return false;
	for (const auto& Entry : Toys)
	{
		if (!Entry.Key.IsValid()) return false;
	}
	return true;
}

bool ABatteryTagGameMode::CanStartRound() const
{
	return !bProcessingRuleEvent && GetGameState<ABatteryTagGameState>() && RequiredBatteries > 0 && IsRosterValid()
		&& (!bEnableMatchLoop || GetLobbyPlayers().Num() >= 3);
}

void ABatteryTagGameMode::RebuildLobbyRoles()
{
	const AToonStoryGameState* State = GetToonGameState();
	if (!State || State->GetMatchStatus().Phase != EToonMatchPhase::Waiting) return;
	TGuardValue<bool> Guard(bProcessingRuleEvent, true);
	Toys.Reset();
	bool bHumanAssigned = false;
	for (const auto& Entry : GetLobbyPlayers())
	{
		AToonStoryPlayerState* Player = Entry.IsValid() ? Entry->GetPlayerState<AToonStoryPlayerState>() : nullptr;
		if (!Player) continue;
		const EToonTeam Team = bHumanAssigned ? EToonTeam::Toy : EToonTeam::Human;
		if (Player->GetTeam() != Team) Player->SetMatchReady(false);
		Player->SetTeam(Team);
		if (Team == EToonTeam::Toy) Toys.Add(Player, false);
		bHumanAssigned = true;
	}
	PublishProgress();
}

void ABatteryTagGameMode::PrepareRound()
{
	TGuardValue<bool> EventGuard(bProcessingRuleEvent, true);
	RoundRequiredBatteries = RequiredBatteries;
	DepositedBatteryIds.Reset();
	for (auto& Entry : Toys) Entry.Value = false;
	PublishProgress();
}

void ABatteryTagGameMode::PublishProgress(bool bNotify)
{
	if (ABatteryTagGameState* State = GetGameState<ABatteryTagGameState>())
	{
		FBatteryTagProgress Progress;
		Progress.DepositedBatteries = DepositedBatteryIds.Num();
		Progress.RequiredBatteries = GetToonGameState()->GetMatchStatus().Phase == EToonMatchPhase::Waiting
			? RequiredBatteries : RoundRequiredBatteries;
		Progress.TotalToys = Toys.Num();
		for (const auto& Entry : Toys) Progress.CapturedToys += Entry.Value ? 1 : 0;
		State->SetProgress(Progress, bNotify);
	}
}

bool ABatteryTagGameMode::TryRecordBatteryDeposit(APlayerState* Player, FGuid BatteryId)
{
	if (bProcessingRuleEvent) return false;
	TGuardValue<bool> EventGuard(bProcessingRuleEvent, true);
	if (!CanAcceptRoundEvent()) return false;
	if (!IsRosterValid()) { AbortRound(); return false; }
	const bool* bCaptured = Toys.Find(Player);
	if (!IsValid(Player) || !bCaptured || *bCaptured || !BatteryId.IsValid() || DepositedBatteryIds.Contains(BatteryId)) return false;
	DepositedBatteryIds.Add(BatteryId);
	// Commit counts before result notification; listeners see the final count immediately.
	PublishProgress(false);
	if (DepositedBatteryIds.Num() >= RoundRequiredBatteries) FinishRound(TEXT("Toy"), TEXT("BatteryGoal"));
	GetGameState<ABatteryTagGameState>()->OnRep_Progress();
	return true;
}

bool ABatteryTagGameMode::SetToyCaptured(APlayerState* Player, bool bCaptured)
{
	if (bProcessingRuleEvent) return false;
	TGuardValue<bool> EventGuard(bProcessingRuleEvent, true);
	if (!CanAcceptRoundEvent()) return false;
	if (!IsRosterValid()) { AbortRound(); return false; }
	bool* Current = Toys.Find(Player);
	if (!IsValid(Player) || !Current || *Current == bCaptured) return false;
	*Current = bCaptured;
	bool bAllCaptured = true;
	for (const auto& Entry : Toys) bAllCaptured &= Entry.Value;
	PublishProgress(false);
	if (bAllCaptured) FinishRound(TEXT("Human"), TEXT("AllCaptured"));
	GetGameState<ABatteryTagGameState>()->OnRep_Progress();
	return true;
}

void ABatteryTagGameMode::HandleTimeExpired()
{
	if (!IsRosterValid()) { AbortRound(); return; }
	FinishRound(TEXT("Human"), TEXT("TimeExpired"));
}

void ABatteryTagGameMode::Logout(AController* Exiting)
{
	if (bEnableMatchLoop) { Super::Logout(Exiting); return; }
	// Temporary fail-safe: don't award a win from a disappearing participant.
	// Matchmaking owner must replace this with the agreed disconnect/rejoin policy.
	const AToonStoryGameState* State = GetToonGameState();
	if (State && State->GetMatchStatus().Phase == EToonMatchPhase::Playing) AbortRound();
	else if (Exiting) UnregisterToy(Exiting->PlayerState);
	Super::Logout(Exiting);
}
