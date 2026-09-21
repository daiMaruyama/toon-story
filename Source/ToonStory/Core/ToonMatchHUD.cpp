#include "Core/ToonMatchHUD.h"
#include "Core/BatteryTagGameState.h"
#include "Core/ToonStoryPlayerState.h"
#include "GameFramework/PlayerController.h"

void AToonMatchHUD::DrawHUD()
{
	Super::DrawHUD();
	const AToonStoryGameState* State = GetWorld()->GetGameState<AToonStoryGameState>();
	if (!Canvas || !State || !PlayerOwner) return;
	const FToonMatchStatus Status = State->GetMatchStatus();
	const AToonStoryPlayerState* Player = PlayerOwner->GetPlayerState<AToonStoryPlayerState>();
	const FString Team = !Player || Player->GetTeam() == EToonTeam::Unassigned ? TEXT("Unassigned")
		: Player->GetTeam() == EToonTeam::Human ? TEXT("Human") : TEXT("Toy");
	const int32 Seconds = FMath::CeilToInt(State->GetPhaseRemainingSeconds());
	FString Heading;
	switch (Status.Phase)
	{
	case EToonMatchPhase::Waiting:
		Heading = FString::Printf(TEXT("LOBBY  Players: %d / %d minimum  Ready: %d / %d"),
			Status.ConnectedPlayers, Status.MinimumPlayers, Status.ReadyPlayers, Status.ConnectedPlayers);
		break;
	case EToonMatchPhase::Countdown:
		Heading = FString::Printf(TEXT("Starting in %d"), Seconds);
		break;
	case EToonMatchPhase::Playing:
		Heading = FString::Printf(TEXT("Time %02d:%02d"), Seconds / 60, Seconds % 60);
		break;
	case EToonMatchPhase::Finished:
		Heading = FString::Printf(TEXT("%s  |  Next lobby in %d"),
			Status.WinningTeam.IsNone() ? TEXT("Round cancelled") : *FString::Printf(TEXT("%s wins!"), *Status.WinningTeam.ToString()), Seconds);
		break;
	}
	DrawRect(FLinearColor(0, 0, 0, 0.7f), 20, 20, 710, 135);
	DrawText(Heading, FLinearColor::White, 35, 30, nullptr, 1.4f);
	DrawText(FString::Printf(TEXT("Your team: %s"), *Team), FLinearColor::White, 35, 65);
	if (Status.Phase == EToonMatchPhase::Waiting || Status.Phase == EToonMatchPhase::Countdown)
	{
		DrawText(Player && Player->IsMatchReady() ? TEXT("READY - Press R to cancel") : TEXT("Press R when ready"),
			FLinearColor::Yellow, 35, 95);
	}
	else if (const ABatteryTagGameState* BatteryState = Cast<ABatteryTagGameState>(State))
	{
		const FBatteryTagProgress Progress = BatteryState->GetProgress();
		DrawText(FString::Printf(TEXT("Batteries: %d / %d   Captured: %d / %d"),
			Progress.DepositedBatteries, Progress.RequiredBatteries, Progress.CapturedToys, Progress.TotalToys),
			FLinearColor::Yellow, 35, 95);
	}
}
