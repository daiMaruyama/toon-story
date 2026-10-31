// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TBHUD.generated.h"

/** 同期された状態を読み取り、操作説明と進捗を画面に表示する。 */
UCLASS()
class TOONSTORY_API ATBHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	bool IsLobbySettingsOpen() const { return bLobbySettingsOpen; }
	bool IsStagePickerOpen() const { return bStagePickerOpen; }
	// 設定・参加者一覧・オプションを開いている間は、チャットを出さない。
	bool IsChatBlocked() const;
	virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;

private:
 bool bPlayersOpen = false;
 TWeakObjectPtr<class APlayerState> KickSelection;
 void DrawPlayers();
 bool HandleSafetyClick(FName Name);
	// ロビーと試合中のメニューから開いたオプション画面。試合の切り替わりで閉じる。
	TWeakObjectPtr<class UTBOptionsMenu> Options;
 FName LastClickedButton;
	double LastButtonClickTime = -1;
	bool bLobbySettingsOpen = false;
	bool bStagePickerOpen = false;
	bool HandleLobbyPanelClick(FName Name);
	UPROPERTY(Transient)
	TObjectPtr<class UFont> PaperFont;
	UPROPERTY(Transient)
	TObjectPtr<class UTexture2D> PaperTexture;
	void PaperPanel(FLinearColor Tint, float X, float Y, float W, float H);
	void DrawText(const FString& Text, FLinearColor Color, float X, float Y, UFont* Font = nullptr, float Scale = 1.f,
	              bool ScalePosition = false);
	void DrawResults(const class ATBGameState& MatchState, class ATBController& Controller);
	void DrawMatch(const class ATBGameState& MatchState, class ATBController& Controller);
	void DrawLobby();
	void DrawDebugHUD();
};
