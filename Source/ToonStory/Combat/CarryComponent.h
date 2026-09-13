#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ToyBoxTypes.h"
#include "CarryComponent.generated.h"

class AHumanCharacter;
class AToyBoxActor;
class AToyCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStoreProgressChanged, float, NewProgress);

/**
 * 掴む → 運ぶ → 収納 の状態機械。人間に付く。
 *
 * 遷移はすべてサーバーで行う。クライアントは要求するだけ
 * （仕様書「4. 捕獲の3段階」）。
 */
UCLASS(ClassGroup = (ToyBox), meta = (BlueprintSpawnableComponent))
class UCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCarryComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 今運んでいるおもちゃ。運んでいなければ nullptr。 */
	UPROPERTY(ReplicatedUsing = OnRep_CarriedToy, BlueprintReadOnly, Category = "ToyBox")
	TObjectPtr<AToyCharacter> CarriedToy;

	/** 収納の進行度 0.0〜1.0。 */
	UPROPERTY(ReplicatedUsing = OnRep_StoreProgress, BlueprintReadOnly, Category = "ToyBox")
	float StoreProgress = 0.f;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox")
	FOnStoreProgressChanged OnStoreProgressChanged;

	/**
	 * 収納にかかる秒数。
	 * ここはおもちゃ側が妨害するための時間として機能させる（仕様書「4. 収納」）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.1"))
	float StoreDuration = 3.f;

	/**
	 * 取り付け先のソケット名。
	 * 相対トランスフォームの直指定は他クライアントでズレやすいので、必ずソケットを使う。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox")
	FName CarrySocketName = TEXT("CarrySocket");

	/** 運搬中の移動速度の倍率。おもちゃ側に妨害の余地を作る。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float CarryingSpeedScale = 0.6f;

	/** 収納できる箱までの距離。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox", meta = (ClampMin = "0.0"))
	float StoreRange = 250.f;

	UFUNCTION(BlueprintPure, Category = "ToyBox")
	bool IsCarrying() const { return CarriedToy != nullptr; }

	UFUNCTION(BlueprintPure, Category = "ToyBox")
	bool IsStoring() const { return bStoring; }

	/** 以下はサーバー専用。 */
	bool BeginGrab(AToyCharacter* Target);
	bool BeginStore();

	/**
	 * 運搬・収納をやめる。
	 *
	 * 中断できないと、掴まれた時点で結果が確定してしまい、
	 * 救助の緊張感が前倒しで消える（仕様書「4. 収納」）。
	 * 中断したおもちゃはその場に落ちる。
	 */
	void ReleaseCarried();

protected:
	UFUNCTION()
	void OnRep_CarriedToy();

	UFUNCTION()
	void OnRep_StoreProgress();

	void TickStore(float DeltaTime);

	/** 運搬状態に合わせて人間の歩行速度を切り替える。 */
	void RefreshCarrierSpeed();

	AHumanCharacter* GetHumanOwner() const;

	UPROPERTY(Replicated)
	bool bStoring = false;

	float DefaultMaxWalkSpeed = 0.f;
	bool bCachedDefaultSpeed = false;
};
