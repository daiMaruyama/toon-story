#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/ToyBoxTypes.h"
#include "ToyCharacter.generated.h"

/**
 * おもちゃ側の Pawn。
 *
 * 人間の視界に入っている間まったく動けない。凍結の判定そのものは
 * UGazeFreezeSubsystem がサーバーで一括して回し、ここは「止める」責務だけを持つ
 * （仕様書「2. 視線凍結システム」）。
 */
UCLASS()
class AToyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AToyCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// PlayerState は Pawn より後に届くことがあるので、両方から bind を試みる。
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** 遮蔽判定に打つ3点（頭・胴・足）。1点でも通れば「見えている」。 */
	void GetGazeSamplePoints(FVector (&OutPoints)[3]) const;

	/**
	 * サーバーが最後に「見られている」と判定した時刻。
	 * 解除を 0.25 秒遅らせるヒステリシスに使う。サーバーだけが読み書きする。
	 */
	double LastWatchedTime = 0.0;

	/** 実際に動けない状態か（サーバー判定とクライアント先行凍結の論理和）。 */
	UFUNCTION(BlueprintPure, Category = "ToyBox")
	bool IsEffectivelyFrozen() const { return bServerFrozen || bPredictedFrozen; }

	/** サーバーの判定を反映する。PlayerState の OnRep から呼ばれる。 */
	void ApplyServerFreeze(bool bFrozen);

	/**
	 * クライアント側の先行凍結。
	 *
	 * サーバー判定だけだと往復の遅延ぶん「見られているのにまだ動ける」時間が生まれる。
	 * 成立したらサーバーの許可を待たずに自分を止める。
	 * 逆に「もう見られていない」と思っても、動き出すのはサーバーの解除を待つ。
	 * 予測が外れて損をするのは常におもちゃ側なので、不公平な巻き戻しが起きない。
	 */
	void ApplyPredictedFreeze(bool bFrozen);

	/**
	 * サーバー専用。凍結中に閾値以上動いていたら、凍結した位置へ戻す。
	 *
	 * 先行凍結は改造クライアントなら無効化できるので、これが無いと凍結が守られない
	 * （仕様書「クライアントの視線情報は信用できない」）。
	 */
	void EnforceFreezeAnchor();

	/** 凍結中に禁止する行動。v1 は完全静止。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox", meta = (Bitmask, BitmaskEnum = "/Script/ToonStory.EFreezeBlock"))
	uint8 FreezeBlocks = static_cast<uint8>(ToyBox::FreezeBlocksV1);

	bool IsFreezeBlocking(EFreezeBlock Block) const
	{
		return (FreezeBlocks & static_cast<uint8>(Block)) != 0;
	}

protected:
	/** PlayerState の bFrozen を自分に反映させる。 */
	UFUNCTION()
	void HandleFrozenChanged(bool bNewFrozen);

	/** PlayerState の通知に bind する。二重 bind はしない。 */
	void BindToPlayerState();

	/** 凍結の反映。移動の入力を捨て、速度と加速度を 0 にする。 */
	void RefreshFrozenState();

	/** 歩行速度の既定値。凍結解除時に戻すため保持する。 */
	float DefaultMaxWalkSpeed = 0.f;
	float DefaultMaxAcceleration = 0.f;

	/** 凍結が成立した位置。巻き戻しの基準。 */
	FVector FreezeAnchorLocation = FVector::ZeroVector;

	bool bServerFrozen = false;
	bool bPredictedFrozen = false;
	bool bFrozenApplied = false;
};
