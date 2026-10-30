// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/TBTypes.h"
#include "TBChat.generated.h"

/** ロビーと結果画面のチャット。遊んでいる最中は声だけにする（声は敵にも聞こえるのが駆け引きなので）。数値はTB::Rules().Chat。 */
namespace TB::Chat
{
	inline constexpr int32 MaxLines = 100;
	// 2回の発言が通信の揺れで近づいて届いても弾かないよう、サーバーは送る側の間隔にこれを掛けて受ける。
	inline constexpr double ServerSlack = .75;

	inline bool IsAvailable(ETBPhase Phase)
	{
		return Phase != ETBPhase::Playing;
	}
	inline bool IsTooSoon(double Last, double Now, double Interval)
	{
		return Last >= 0 && Now - Last < Interval;
	}
	/** 1回の文字数で切る。サロゲートペアの途中では切らない。 */
	TOONSTORY_API FString Limit(const FString& Text);
	/** 改行と制御文字を空白にして前後を詰め、Limitで切る。 */
	TOONSTORY_API FString Clean(const FString& Text);
}

struct FTBChatLine
{
	FString Name;
	FString Text;
	bool bSelf = false;
	// FPlatformTime::Seconds。ワールドの時刻はマップを移ると戻るので使わない。
	double Time = 0;
};

/** その部屋に入ってから受け取った発言。マップを移っても残し、部屋を作る・入るときに消す。 */
UCLASS()
class TOONSTORY_API UTBChatLog : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	void Add(const FString& Name, const FString& Text, bool bSelf);
	void Clear();
	const TArray<FTBChatLine>& GetLines() const
	{
		return Lines;
	}
	// 中身が変わるたびに進む。画面はこれを見て作り直す。
	uint32 GetRevision() const
	{
		return Revision;
	}

private:
	TArray<FTBChatLine> Lines;
	uint32 Revision = 0;
};
