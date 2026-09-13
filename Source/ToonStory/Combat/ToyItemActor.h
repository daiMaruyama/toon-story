#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ToyItemActor.generated.h"

class AToyCharacter;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCollectProgressChanged, float, NewProgress);

/**
 * おもちゃ側が集めるアイテム。
 *
 * 収集数は GameState に持たせる（全員が進捗を見られるべき情報）。
 * 2人が同時に拾った場合はサーバーが先着1人だけ成立させ、
 * 負けた側には失敗を返す（仕様書「8. アイテムの実装メモ」）。
 */
UCLASS()
class AToyItemActor : public AActor
{
	GENERATED_BODY()

public:
	AToyItemActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 取得済みか。取得されたら見た目も当たり判定も消す。 */
	UPROPERTY(ReplicatedUsing = OnRep_Collected, BlueprintReadOnly, Category = "ToyBox")
	bool bCollected = false;

	/** 取得の進行度 0.0〜1.0。 */
	UPROPERTY(ReplicatedUsing = OnRep_CollectProgress, BlueprintReadOnly, Category = "ToyBox")
	float CollectProgress = 0.f;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox")
	FOnCollectProgressChanged OnCollectProgressChanged;

	/** 取得にかかる基本秒数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.1"))
	float BaseCollectDuration = 3.f;

	/** 取得に必要な距離。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.0"))
	float CollectRadius = 180.f;

	/**
	 * 人間の箱への張り付き 1 秒あたり、収集がどれだけ速くなるか。
	 *
	 * 所要時間 = BaseCollectDuration / (1 + CampingBonusPerSecond * 張り付き秒数)
	 *
	 * 救助の「中断で0リセット」は、人間が箱の周りをうろつくだけで救助が
	 * 永久に成立しなくなる危険がある。ルールを増やさずに張り付きを罰するため、
	 * 別のリソース（アイテム収集速度）で返す（仕様書「中断で0リセットが強すぎないか」）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.0"))
	float CampingBonusPerSecond = 0.02f;

	/** 張り付きボーナスでもこれより速くはならない（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.1"))
	float MinCollectDuration = 1.f;

protected:
	/** サーバー専用。取得を進める・中断する。 */
	void TickCollect(float DeltaSeconds);

	/** 今この瞬間に取得を進めている 1 人。いなければ nullptr。 */
	AToyCharacter* FindCollector() const;

	/** 張り付き時間を織り込んだ実際の所要秒数。 */
	float GetEffectiveCollectDuration() const;

	/** サーバー専用。先着 1 人だけ成立させる。 */
	void CompleteCollect(AToyCharacter* Collector);

	UFUNCTION()
	void OnRep_Collected();

	UFUNCTION()
	void OnRep_CollectProgress();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ToyBox")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 進行中の取得者。途中で別の人に変わったら 0 に戻す。 */
	TWeakObjectPtr<AToyCharacter> CurrentCollector;
};
