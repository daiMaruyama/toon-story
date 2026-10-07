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
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

private:
	void DrawResults(const class ATBGameState& MatchState, class ATBController& Controller);
	void DrawMatch(const class ATBGameState& MatchState, class ATBController& Controller);
	void DrawLobby();
	void DrawDebugHUD();
	bool bBrowsingRooms = false;
	int32 RoomPage = 0;
};
