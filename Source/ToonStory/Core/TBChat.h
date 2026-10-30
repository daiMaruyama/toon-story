// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/TBTypes.h"
#include "TBChat.generated.h"

class AController;
class APlayerState;

/** 部屋の出入りのお知らせ。ログに名前なしの1行で出す */
UENUM()
enum class ETBChatNotice : uint8
{
	Joined,
	Left,
	Kicked
};

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
	// ログとロビーの一覧に出す名前の長さ。
	inline constexpr int32 NameLength = 18;

	/** MaxLength文字で切る。サロゲートペアの途中では切らない。 */
	TOONSTORY_API FString Limit(const FString& Text, int32 MaxLength);
	/** 見えない文字を消し、改行と制御文字を空白にして前後を詰め、1回の文字数で切る。 */
	TOONSTORY_API FString Clean(const FString& Text);
	/** 画面に出す名前。Cleanと同じく見えない文字を消し、NameLengthで切る。 */
	TOONSTORY_API FString DisplayName(const APlayerState* Player);
	TOONSTORY_API FString NoticeText(ETBChatNotice Kind, const FString& Name);
	/** 受け取った側で呼ぶ。SenderNetIdは送った人のUniqueNetIdの文字列（SteamならSteamID）。 */
	TOONSTORY_API FString Filter(const UWorld* World, const FString& Text, const FString& SenderNetId);
	/** サーバーで呼ぶ。Except（抜けた本人）以外の全員に配る。 */
	void BroadcastNotice(UWorld* World, ETBChatNotice Kind, const FString& Name, const AController* Except = nullptr);
}

struct FTBChatLine
{
	FString Name;
	FString Text;
	bool bSelf = false;
	// 出入りのお知らせ。名前は出さず、色を変える。
	bool bNotice = false;
	// FPlatformTime::Seconds。ワールドの時刻はマップを移ると戻るので使わない。
	double Time = 0;
	// 足した順の通し番号。画面は、これで足された行と消えた行を見分ける。
	uint64 Id = 0;
};

/** その部屋に入ってから受け取った発言。マップを移っても残し、部屋を作る・入るときに消す。 */
UCLASS()
class TOONSTORY_API UTBChatLog : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	void Add(const FString& Name, const FString& Text, bool bSelf);
	void AddNotice(const FString& Text);
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
	void Push(FTBChatLine&& Line);
	TArray<FTBChatLine> Lines;
	uint32 Revision = 0;
	uint64 NextId = 1;
};
