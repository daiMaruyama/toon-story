#pragma once
#include "CoreMinimal.h"
#include "TBItemSpawnTypes.generated.h"

class ATBItemBase;

/** 1クラスにつき1件の配置設定。効果・音量などは各アイテムのDAで調整する。 */
USTRUCT(BlueprintType)
struct FTBItemSpawnEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="アイテムクラス"))
	TSubclassOf<ATBItemBase> ItemClass;
	/** オフなら、このクラスの全配置種別を抽選対象から外す。手置きのActorには影響しない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="ランダム配置する"))
	bool bEnabled = true;
	/** 0なら出現しない。重み2は重み1の2倍選ばれやすい。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="選ばれやすさ", ClampMin="0", ClampMax="1000"))
	float Weight = 1;
	/** モデル底面を床へ合わせ、底面の中心・四隅・辺の中央に支持する床があるか確認する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="モデル底面を床に合わせる"))
	bool bPlaceOnFloor = true;
	/** キーは地点の配置種別、値は地点からの変換。完全一致を優先し、なければNoneを使用。空なら出現しない。
	 * +Xを壁から部屋に向かう方向として置く。配置種別を増やしても抽選の重みは増えない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="配置種別ごとの変換"))
	TMap<FName, FTransform> Placements;

	const FTransform* FindPlacement(FName Type) const
	{
		if (const FTransform* Exact = Placements.Find(Type)) return Exact;
		return Placements.Find(NAME_None);
	}
};

USTRUCT(BlueprintType)
struct FTBItemSpawnRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="ランダム配置を有効化"))
	bool bEnabled = true;
	/** 候補地点がないマップには出現しない。安全な候補が不足する場合は配置可能な個数まで。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="試合ごとの出現数", ClampMin="0", ClampMax="100"))
	int32 SpawnCount = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="アイテムの外形間の最短距離", ClampMin="0", Units="cm"))
	float MinimumSeparation = 120;
	/** 外形の左右・前後・上に確保する余白。壁や家具に接する候補を除外する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="障害物との余白", ClampMin="0", Units="cm"))
	float ObstacleClearance = 30;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="設置面の許容高低差", ClampMin="0", Units="cm"))
	float FloorHeightTolerance = 2;
	/** 接触の数値誤差を避けるための微小な隙間。原点ではなくモデル底面に適用する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="底面と床の隙間", ClampMin="0.1", ClampMax="2", Units="cm"))
	float FloorGap = .5f;
	/** 候補地点から下向きに床を調べる距離。候補地点は床の少し上に置く。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="床を確認する距離", ClampMin="1", Units="cm"))
	float FloorProbeDistance = 40;
	/** クラスごとに1件登録する。同じクラスの重複登録は無効。BP派生クラスは別のクラスとして設定する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="アイテム別設定", TitleProperty="ItemClass"))
	TArray<FTBItemSpawnEntry> Entries;
};
