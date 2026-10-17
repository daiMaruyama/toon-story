// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBHUD.h"
#include "Core/TBLobbySettings.h"
#include "TBPaperStyle.h"
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Core/TBSession.h"
#include "Core/TBGameHelpers.h"
#include "Character/TBCharacter.h"
#include "GamePlay/TBBox.h"
#include "Components/SceneComponent.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

void ATBHUD::NotifyHitBoxClick(FName Name)
{
	Super::NotifyHitBoxClick(Name);
	auto* PC = Cast<ATBController>(GetOwningPlayerController());
	const auto* State = TB::GS(GetWorld());
	if (!PC || !State)
	{
		return;
	}
	if (auto* Audio = PC->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>())
	{
		if (Name != TEXT("Ready") && Name != TEXT("Child") && Name != TEXT("Toy") && Name != TEXT("Either") &&
		    Name != TEXT("MoreChildren") && Name != TEXT("FewerChildren") && Name != TEXT("MoreToys") && Name != TEXT("FewerToys") &&
		    Name != TEXT("LessTime") && Name != TEXT("MoreTime") && Name != TEXT("FewerBatteries") && Name != TEXT("MoreBatteries"))
			Audio->PlaySoundEvent(Name == TEXT("Resume") || Name == TEXT("Leave") || Name == TEXT("CloseSettings") || Name == TEXT("CancelStage") ? ETBSoundEvent::UIBack : ETBSoundEvent::UIClick);
	}
	if (Name == TEXT("Resume"))
	{
		PC->ToggleLeaveMenu();
		return;
	}
	if (Name == TEXT("Leave"))
	{
		PC->TBLeave();
		return;
	}
	if (Name == TEXT("ReturnToLobby"))
	{
		PC->TBLobby();
		return;
	}
	if (State->Phase != ETBPhase::Lobby)
	{
		return;
	}
	if (HandleLobbyPanelClick(Name) || TBLobbySettings::HandleClick(PC, Name)) return;
	if (Name == TEXT("Ready"))
	{
		PC->TBReady();
	}
	else if (Name == TEXT("Start"))
	{
		PC->TBStart();
	}
	else if (Name == TEXT("Child"))
	{
		PC->TBPrefer(1);
	}
	else if (Name == TEXT("Toy"))
	{
		PC->TBPrefer(2);
	}
	else if (Name == TEXT("Either"))
	{
		PC->TBPrefer(0);
	}
	else if (Name == TEXT("MoreChildren") || Name == TEXT("FewerChildren"))
	{
		PC->TBRules(State->Settings.Humans + (Name == TEXT("MoreChildren") ? 1 : -1), State->Settings.Toys,
		            State->Settings.Duration, State->Settings.RequiredItems);
	}
	else if (Name == TEXT("MoreToys") || Name == TEXT("FewerToys"))
	{
		PC->TBRules(State->Settings.Humans, State->Settings.Toys + (Name == TEXT("MoreToys") ? 1 : -1),
		            State->Settings.Duration, State->Settings.RequiredItems);
	}
}

// HUDは状態の表示だけを行う。勝敗や取得の判定はサーバー側で行う。
void ATBHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	const auto* State = TB::GS(GetWorld());
	auto* PC = Cast<ATBController>(GetOwningPlayerController());
	if (!State || !PC)
	{
		return;
	}
	if (State->Phase == ETBPhase::Lobby)
	{
		DrawLobby();
		return;
	}
	if (State->Phase == ETBPhase::Results || State->Phase == ETBPhase::Aborted)
	{
		DrawResults(*State, *PC);
	}
	else
	{
		DrawMatch(*State, *PC);
		if (PC->IsLeaveMenuOpen())
		{
			const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
			const float X = Canvas->ClipX * .5f - 260 * S, Y = Canvas->ClipY * .5f - 150 * S;
			DrawRect(FLinearColor(0, 0, 0, .45f), 0, 0, Canvas->ClipX, Canvas->ClipY);
			PaperPanel(FLinearColor::White, X, Y, 520 * S, 250 * S);
			DrawText(TEXT("ひとやすみ"), TBPaper::Accent, X + 32 * S, Y + 24 * S, PaperFont, 1.5f * S);
			DrawText(TEXT("オンラインの試合は進み続けます。"), TBPaper::Muted, X + 32 * S, Y + 69 * S, PaperFont,
			         .85f * S);
			const TCHAR* Labels[] = {TEXT("あそびにもどる [Esc]"), TEXT("タイトルにもどる")};
			const FName Names[] = {TEXT("Resume"), TEXT("Leave")};
			for (int32 I = 0; I < 2; ++I)
			{
				PaperPanel(TBPaper::Cream, X + 25 * S, Y + (105 + I * 60) * S, 470 * S, 50 * S);
				DrawText(Labels[I], TBPaper::Ink, X + 45 * S, Y + (117 + I * 60) * S, PaperFont, S);
				AddHitBox(FVector2D(X + 25 * S, Y + (105 + I * 60) * S), FVector2D(470 * S, 50 * S), Names[I], true);
			}
		}
	}
}

// 通常HUDに内部状態だけを追加する。ロビー・結果画面の操作は置き換えない。
void ATBHUD::DrawDebugHUD()
{
	const auto* State = TB::GS(GetWorld());
	if (!State)
	{
		return;
	}
	const float Scale = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	float Y = 155.f * Scale;
	auto Line = [this, Scale, &Y](const FString& Text)
	{
		DrawText(Text, FLinearColor::Yellow, 24.f * Scale, Y, PaperFont, Scale);
		Y += 22.f * Scale;
	};
	for (const APlayerState* Player : State->PlayerArray)
	{
		if (const auto* Info = Cast<ATBPlayerState>(Player))
		{
			Line(FString::Printf(TEXT("%s | Team:%d State:%d Frozen:%d"), *Info->GetPlayerName(), int32(Info->Team),
			                     int32(Info->ToyState), Info->bFrozen));
		}
	}
	const auto* Viewer =
	    GetOwningPlayerController() ? GetOwningPlayerController()->GetPlayerState<ATBPlayerState>() : nullptr;
	if (Viewer && Viewer->Team == ETBTeam::Toy && State->Box)
	{
		Line(FString::Printf(TEXT("Rescue: %.0f%% | Rescuers: %d"), State->Box->RescueProgress * 100,
		                     State->Box->Rescuers));
	}
}

void ATBHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
    Super::NotifyHitBoxBeginCursorOver(BoxName);
    if (auto* PC = GetOwningPlayerController())
        if (auto* Audio = PC->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>()) Audio->PlaySoundEvent(ETBSoundEvent::UIHover);
}
