#include "TBHUD.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Core/TBSession.h"
#include "Core/TBGameHelpers.h"
#include "Character/TBCharacter.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBPickup.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"

// HUDは状態の表示だけを行う。勝敗や取得の判定はサーバー側で行う。
void ATBHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	float Y = 20;
	auto Line = [this, &Y](const FString& Text, FLinearColor Color = FLinearColor::White)
	{
		DrawText(Text, Color, 20, Y, nullptr, 1.f);
		Y += 22;
	};
	Line(TEXT("TOYBOX | WASD Mouse Space | E grab/hold rescue/store | Q drop | R ready | F5 start | F10 console"));
	Line(TEXT("Console: TBHost / TBFind / TBJoin 0 / TBLeave / TBRules 1 1 600 5 / TBPrefer 1(human),2(toy),0(any)"));
	auto* PlayerController = Cast<ATBController>(GetOwningPlayerController());
	if (PlayerController)
	{
		if (auto* Session = PlayerController->GetGameInstance()->GetSubsystem<UTBSession>())
		{
			Line(Session->Status, FLinearColor::Yellow);
			for (int32 Index = 0; Index < Session->Rooms.Num(); ++Index)
			{
				Line(FString::Printf(TEXT("[%d] %s"), Index, *Session->Rooms[Index]));
			}
		}
		Line(PlayerController->Notice, FLinearColor::Yellow);
	}
	if (auto* MatchInfo = TB::GS(GetWorld()))
	{
		Line(FString::Printf(TEXT("Phase %d | Humans %d / Toys %d | Time %.0f | Items %d/%d | Boxed %d"),
		                     int32(MatchInfo->Phase), MatchInfo->Settings.Humans, MatchInfo->Settings.Toys,
		                     MatchInfo->Remaining(), MatchInfo->Collected, MatchInfo->Settings.RequiredItems,
		                     MatchInfo->BoxedCount));
		if (MatchInfo->Phase == ETBPhase::Results || MatchInfo->Phase == ETBPhase::Aborted)
		{
			Line(FString::Printf(TEXT("Winner: %s | %s | TBLeave to return"),
			                     MatchInfo->Winner == ETBWinner::Humans ? TEXT("HUMANS")
			                     : MatchInfo->Winner == ETBWinner::Toys ? TEXT("TOYS")
			                                                            : TEXT("NONE"),
			                     *MatchInfo->Reason),
			     FLinearColor::Green);
		}
		for (APlayerState* LocalPlayerState : MatchInfo->PlayerArray)
		{
			if (auto* ToyPlayerInfo = Cast<ATBPlayerState>(LocalPlayerState))
			{
				Line(FString::Printf(TEXT("%s | Team:%d Pref:%d Ready:%d State:%d Frozen:%d"),
				                     *ToyPlayerInfo->GetPlayerName(), int32(ToyPlayerInfo->Team),
				                     int32(ToyPlayerInfo->Preference), ToyPlayerInfo->bReady,
				                     int32(ToyPlayerInfo->ToyState), ToyPlayerInfo->bFrozen));
			}
		}
		if (MatchInfo->Box)
		{
			Line(FString::Printf(TEXT("Rescue:%.0f%% (%d rescuers)"), MatchInfo->Box->RescueProgress * 100,
			                     MatchInfo->Box->Rescuers));
			if (MatchInfo->GetServerWorldTimeSeconds() < MatchInfo->Box->AlarmUntil)
			{
				Line(TEXT("ALARM: RESCUE STARTED AT THE BOX!"), FLinearColor::Red);
			}
		}
	}
	if (PlayerController)
	{
		if (auto* LocalPlayerState = PlayerController->GetPlayerState<ATBPlayerState>())
		{
			if (LocalPlayerState->bFrozen)
			{
				DrawText(TEXT("WATCHED - FROZEN"), FLinearColor::Red, Canvas->ClipX * .4f, Canvas->ClipY * .7f, nullptr,
				         2.f);
			}
		}
	}
	if (PlayerController)
	{
		if (auto* ToyCharacter = Cast<ATBCharacter>(PlayerController->GetPawn()))
		{
			if (ToyCharacter->ContactItem)
			{
				Line(FString::Printf(TEXT("Item: %.0f%% %s"), ToyCharacter->ItemProgress * 100,
				                     ToyCharacter->IsGazeFrozen() ? TEXT("PAUSED") : TEXT("TOUCHING")),
				     FLinearColor::Yellow);
			}
		}
	}
	DrawText(TEXT("+"), FLinearColor::White, Canvas->ClipX * .5f, Canvas->ClipY * .5f);
}
