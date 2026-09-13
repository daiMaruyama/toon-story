#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GazeFreezeSubsystem.generated.h"

class AToyCharacter;
class AHumanCharacter;

/**
 * 視線凍結の判定を一括で回す World Subsystem。
 *
 * 判定の流れ（仕様書「2. 視線凍結システム」）:
 *   1. 粗判定   人間の視線方向とおもちゃ方向の内積が閾値以上か
 *   2. 遮蔽判定 視野角内だったものだけ、頭・胴・足の3点にライントレース
 *   3. 集約     人間が複数いる場合、1人でも見ていれば凍結
 *   4. ヒステリシス 凍結は即時、解除は 0.25 秒待つ
 *
 * 成立させるのはサーバーだけ。クライアントでも同じ判定関数を呼べるようにしてあるのは、
 * 先行凍結（体感を良くするための自主的な停止）に使うため。
 */
UCLASS()
class UGazeFreezeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGazeFreezeSubsystem* Get(const UObject* WorldContext);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	void RegisterToy(AToyCharacter* Toy);
	void UnregisterToy(AToyCharacter* Toy);
	void RegisterHuman(AHumanCharacter* Human);
	void UnregisterHuman(AHumanCharacter* Human);

	/** 1人でも見ていれば true。クライアントの先行凍結からも呼ぶ。 */
	bool IsWatchedByAnyHuman(const AToyCharacter* Toy) const;

	/** 1対1の判定。視野角 → 遮蔽の順に見る。 */
	bool IsWatchedByHuman(const AToyCharacter* Toy, const AHumanCharacter* Human) const;

protected:
	/** サーバー専用。登録済みの全おもちゃについて凍結状態を更新する。 */
	void EvaluateAll();

	TArray<TWeakObjectPtr<AToyCharacter>> CachedToys;
	TArray<TWeakObjectPtr<AHumanCharacter>> CachedHumans;

	FTimerHandle EvaluateTimerHandle;
};
