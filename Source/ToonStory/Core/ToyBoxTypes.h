// おもちゃ箱 非対称マルチプレイの共通型。
// 仕様書「1. 全体構成とクラス設計」に対応する。

#pragma once

#include "CoreMinimal.h"
#include "ToyBoxTypes.generated.h"

/** おもちゃ箱のゲームロジック全般のログ。 */
DECLARE_LOG_CATEGORY_EXTERN(LogToyBox, Log, All);

/** 陣営。 */
UENUM(BlueprintType)
enum class ETeamId : uint8
{
	Unassigned,
	Human,
	Toy
};

/** おもちゃの捕獲状態。掴む → 運ぶ → 収納 の3段階（仕様書「4. 捕獲の3段階」）。 */
UENUM(BlueprintType)
enum class EToyState : uint8
{
	Free,     // 自由
	Grabbed,  // 掴まれた
	Carried,  // 運搬中
	Storing,  // 収納処理中
	Boxed     // 箱の中（救助待ち）
};

/** マッチの進行フェーズ。 */
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Lobby,
	InProgress,
	PostMatch
};

/** 決着の理由（仕様書「6. タイマーと勝敗判定」の判定一覧に対応）。 */
UENUM(BlueprintType)
enum class EMatchResult : uint8
{
	None,
	HumanWin_AllToysBoxed,   // 全おもちゃが Boxed
	ToyWin_ItemsCollected,   // アイテムが必要数に到達
	ToyWin_TimeUp,           // 制限時間切れ
	ToyWin_HumansLeft        // 人間が全員離脱
};

/**
 * 凍結中に禁止する行動。
 *
 * v1 は完全静止だが、将来「凍結中でもアイテム操作と救助継続を許可する」方向へ
 * 緩められるよう、単一の bool ではなくフラグで持つ（仕様書「8. アイテムと未決事項」）。
 */
UENUM(meta = (Bitflags))
enum class EFreezeBlock : uint8
{
	None     = 0,
	Movement = 1 << 0,
	Camera   = 1 << 1,
	Interact = 1 << 2,   // アイテム取得
	Rescue   = 1 << 3
};
ENUM_CLASS_FLAGS(EFreezeBlock);

namespace ToyBox
{
	/** v1 = 完全静止。Interact と Rescue を外せば、仕様変更はこの1行で済む。 */
	constexpr EFreezeBlock FreezeBlocksV1 =
		EFreezeBlock::Movement | EFreezeBlock::Camera | EFreezeBlock::Interact | EFreezeBlock::Rescue;
}

/**
 * マッチ設定。ホストがロビーで変更し、GameState 経由で全員に配る。
 * クライアントが自分で書き換えても意味がない構造にしておく（仕様書「3. チーム編成と人数比」）。
 */
USTRUCT(BlueprintType)
struct FMatchSettings
{
	GENERATED_BODY()

	/** 人間の人数。基本は 人間1 : おもちゃ4。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "1"))
	int32 NumHumans = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "1"))
	int32 NumToys = 4;

	/** 制限時間（秒）。切れたらおもちゃ側の勝利。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "1.0"))
	float MatchDuration = 600.f;

	/** おもちゃ側の勝利に必要なアイテム数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "1"))
	int32 RequiredItems = 5;
};
