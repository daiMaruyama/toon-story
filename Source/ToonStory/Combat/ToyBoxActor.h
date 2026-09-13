#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ToyBoxTypes.h"
#include "ToyBoxActor.generated.h"

class AToyCharacter;
class USceneComponent;
class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRescueProgressChanged, float, NewProgress);

/**
 * おもちゃ箱。マップに1つ。
 *
 * 収容と救助の進行を管理する。開錠は 10 秒、人数が増えると短縮、
 * 中断で 0 に戻る、回数無制限（仕様書「5. おもちゃ箱と救助」）。
 *
 * 進行度を持つのはサーバーだけ。クライアントが独自にカウントすると
 * 必ず表示と実際がずれる。
 */
UCLASS()
class AToyBoxActor : public AActor
{
	GENERATED_BODY()

public:
	AToyBoxActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** レベルに置かれた最初の箱。マップに1つという前提なのでキャッシュしてよい。 */
	static AToyBoxActor* GetPrimaryBox(const UObject* WorldContext);

	/** 救助の進行度 0.0〜1.0。UI は補間に使うだけにすること。 */
	UPROPERTY(ReplicatedUsing = OnRep_RescueProgress, BlueprintReadOnly, Category = "ToyBox")
	float RescueProgress = 0.f;

	/** 今この箱を開けにきている人数。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ToyBox")
	int32 NumRescuers = 0;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox")
	FOnRescueProgressChanged OnRescueProgressChanged;

	/** 1人で開けたときの所要時間（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.1"))
	float BaseRescueTime = 10.f;

	/**
	 * 人数による短縮の逓減係数。
	 *
	 * 所要時間 = BaseRescueTime / (1 + RescueSpeedupPerExtra * (N - 1))
	 * 0.5 なら 1人=10.0秒 / 2人=6.7秒 / 3人=5.0秒。
	 * 線形に 10/N とすると3人で3.3秒になり、「全員で箱に行くのが常に正解」に
	 * なってアイテム収集が死ぬ（仕様書「加速の設計」）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.0"))
	float RescueSpeedupPerExtra = 0.5f;

	/** 救助に参加できる距離。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.0"))
	float RescueRadius = 250.f;

	/** 人間が箱に張り付いていると見なす距離。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.0"))
	float HumanCampingRadius = 700.f;

	/**
	 * 人間が箱の近くに居座っていた累計秒数（サーバーのみ）。
	 *
	 * 中断で 0 リセットは緊張感を生むが、人間が箱の周りをうろつくだけで
	 * 救助が永久に成立しなくなる。これを別のリソースで罰するための計測値で、
	 * M7 でアイテム収集速度に効かせる（仕様書「中断で0リセットが強すぎないか」）。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ToyBox")
	float HumanCampingSeconds = 0.f;

	/** 収容する。Boxed になったおもちゃを箱の中へ移す。 */
	void StoreToy(AToyCharacter* Toy);

	/** 箱の中にいるおもちゃを全員解放する。 */
	void ReleaseAllBoxedToys();

	/** 箱の内側の出現位置。部屋として作るなら入口側に置く。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ToyBox")
	TObjectPtr<USceneComponent> InteriorAnchor;

	/** 救助されたおもちゃが出てくる位置。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ToyBox")
	TObjectPtr<USceneComponent> ExitAnchor;

protected:
	/** サーバー専用。救助の進行を進める・中断する。 */
	void TickRescue(float DeltaSeconds);

	/** サーバー専用。人間の張り付き時間を計測する。 */
	void TickHumanCamping(float DeltaSeconds);

	/** 今この瞬間に救助を成立させている人数。 */
	int32 CountValidRescuers() const;

	/**
	 * 開錠の開始。大きな音が鳴り、人間に「今そこで誰かが動いている」と伝わる。
	 * 箱の位置は既知なので、正確すぎる位置表示にはしないこと（仕様書「大きな音の実装」）。
	 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastRescueStarted();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRescueAborted();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRescueCompleted();

	UFUNCTION()
	void OnRep_RescueProgress();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ToyBox")
	TObjectPtr<USceneComponent> SceneRoot;
};
