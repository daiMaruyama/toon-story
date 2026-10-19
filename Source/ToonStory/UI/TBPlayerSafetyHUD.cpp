// Copyright (c) 2026 Mamodou. All Rights Reserved.
#include "TBHUD.h"
#include "TBPaperStyle.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

bool ATBHUD::HandleSafetyClick(FName Name)
{
	auto* PC = Cast<ATBController>(GetOwningPlayerController());
	auto* State = GetWorld()->GetGameState<ATBGameState>();
	if (!PC || !State)
	{
		return false;
	}
	if (Name == TEXT("Players"))
	{
		bPlayersOpen = true;
		KickSelection.Reset();
		return true;
	}
	// Lobby rows open the same confirmation used by the in-match participant list.
	if (!bPlayersOpen && State->Phase == ETBPhase::Lobby && bLobbySettingsOpen && !bStagePickerOpen &&
	    PC->HasAuthority() && !State->bTravelPending)
	{
		for (APlayerState* Player : State->PlayerArray)
		{
			if (Player && Player != PC->PlayerState &&
			    Name == FName(*FString::Printf(TEXT("Kick_%d"), Player->GetPlayerId())))
			{
				KickSelection = Player;
				bPlayersOpen = true;
				return true;
			}
		}
	}
	if (bPlayersOpen)
	{
		if (Name == TEXT("ClosePlayers"))
		{
			bPlayersOpen = false;
			KickSelection.Reset();
		}
		else if (Name == TEXT("CancelKick"))
		{
			bPlayersOpen = State->Phase != ETBPhase::Lobby;
			KickSelection.Reset();
		}
		else if (Name == TEXT("ConfirmKick"))
		{
			if (PC->HasAuthority() && !State->bTravelPending && KickSelection.IsValid() && State->PlayerArray.Contains(KickSelection.Get()))
			{
				PC->ServerKickPlayer(KickSelection->GetPlayerId());
			}
			bPlayersOpen = State->Phase != ETBPhase::Lobby;
			KickSelection.Reset();
		}
		else if (PC->HasAuthority() && !KickSelection.IsValid())
		{
			for (APlayerState* Player : State->PlayerArray)
			{
				if (Player && Player != PC->PlayerState &&
				    Name == FName(*FString::Printf(TEXT("Kick_%d"), Player->GetPlayerId())))
				{
					KickSelection = Player;
					break;
				}
			}
		}
		return true;
	}
	if (Name == TEXT("Unstuck"))
	{
		PC->ServerUnstuck();
		return true;
	}
	return false;
}

void ATBHUD::DrawPlayers()
{
	auto* PC = Cast<ATBController>(GetOwningPlayerController());
	auto* State = GetWorld()->GetGameState<ATBGameState>();
	if (!PC || !State)
	{
		return;
	}
	const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	const float X = (Canvas->ClipX - 700 * S) * .5f, Y = (Canvas->ClipY - 600 * S) * .5f;
	DrawRect(FLinearColor(0, 0, 0, .6f), 0, 0, Canvas->ClipX, Canvas->ClipY);
	PaperPanel(TBPaper::Cream, X, Y, 700 * S, 600 * S);
	auto Text = [&](const FString& Value, float PX, float PY, float Scale = 1.f)
	{ DrawText(Value, TBPaper::Ink, X + PX * S, Y + PY * S, PaperFont, Scale * S); };
	auto Button = [&](FName Id, const FString& Label, float PX, float PY, float W)
	{
		PaperPanel(FLinearColor::White, X + PX * S, Y + PY * S, W * S, 42 * S);
		Text(Label, PX + 12, PY + 7, .9f);
		AddHitBox(FVector2D(X + PX * S, Y + PY * S), FVector2D(W * S, 42 * S), Id, true);
	};
	Text(TEXT("参加者一覧"), 30, 24, 1.5f);
	if (KickSelection.IsValid() && !State->PlayerArray.Contains(KickSelection.Get()))
	{
		KickSelection.Reset();
	}
	if (KickSelection.IsValid())
	{
		Text(KickSelection->GetPlayerName().Left(28), 30, 130, 1.2f);
		Text(TEXT("このプレイヤーを部屋から退出させますか？"), 30, 190);
		Text(TEXT("これはキックです。再参加を禁止するものではありません。"), 30, 240, .8f);
		Button(TEXT("ConfirmKick"), TEXT("退出させる"), 30, 330, 300);
		Button(TEXT("CancelKick"), TEXT("キャンセル"), 360, 330, 300);
	}
	else
	{
		Text(PC->HasAuthority() ? TEXT("キックできるのはホストだけです。") : TEXT("ホストが参加者を管理します。"), 30,
		     70, .85f);
		int32 Row = 0;
		for (APlayerState* Player : State->PlayerArray)
		{
			if (!Player || Row >= 8)
			{
				continue;
			}
			const float PY = 112 + Row++ * 48;
			Text(Player->GetPlayerName().Left(28), 30, PY + 6, .95f);
			if (Player == PC->PlayerState)
			{
				Text(TEXT("あなた"), 540, PY + 6, .85f);
			}
			else if (PC->HasAuthority() && !State->bTravelPending)
			{
				Button(FName(*FString::Printf(TEXT("Kick_%d"), Player->GetPlayerId())), TEXT("キック"), 540, PY, 120);
			}
		}
		Text(PC->Notice.Left(65), 30, 505, .75f);
		Button(TEXT("ClosePlayers"), TEXT("もどる"), 450, 545, 210);
	}
}
