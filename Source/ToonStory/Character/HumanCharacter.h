#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HumanCharacter.generated.h"

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
};
