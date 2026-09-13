#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HumanCharacter.generated.h"

class AToyCharacter;

/**
 * 人間側の Pawn。
 *
 * 視界に入れたおもちゃを止める（仕様書「2. 視線凍結システム」）。
 * 掴む・運ぶ・収納は UCarryComponent が持つ（M6 で追加）。
 */
UCLASS()
class AHumanCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHumanCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 視線の始点と向き。
	 *
	 * GetViewRotation() はサーバー（Controller あり）でも、
	 * クライアント上の他人の Pawn（RemoteViewPitch から復元）でも動く。
	 * おかげでサーバー判定とクライアント先行凍結で同じコードが使える。
	 */
	void GetGazeOrigin(FVector& OutLocation, FVector& OutDirection) const;

	/**
	 * 掴める距離。
	 * 遅延ぶんの余裕は GrabRangeTolerance で別に持つ。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox")
	float GrabRange = 180.f;

	/**
	 * 距離判定の許容値。
	 *
	 * 0 にすると ping の高いプレイヤーはほぼ掴めなくなる。
	 * 逆に大きすぎると「触れていないのに掴まれた」と感じられる（仕様書「4.」）。
	 * 100cm 程度から始めて実測で詰めること。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox")
	float GrabRangeTolerance = 100.f;

	/** 掴むときに要求する正面度（内積の下限）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ToyBox")
	float GrabFacingDot = 0.5f;

	/** サーバーから見て、この相手を掴める位置関係にあるか。 */
	bool CanReachToy(const AToyCharacter* Target) const;

	/** 入力から呼ぶ。成立させるのはサーバー。 */
	UFUNCTION(BlueprintCallable, Category = "ToyBox")
	void RequestCapture(AToyCharacter* Target);

protected:
	/**
	 * M3 の簡易版。触れたら即 Boxed にする。
	 * M6 で掴む → 運ぶ → 収納の3段階に置き換わる。
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTryCapture(AToyCharacter* Target);
};
