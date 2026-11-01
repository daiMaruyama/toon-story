// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBTitleGaze.generated.h"

class USpotLightComponent;
class UTBTitleMenu;

USTRUCT()
struct FTBFrozenToy
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AActor> Toy;
	// 固まった瞬間の姿を写し取った人形。本物は隠したままシーケンスで踊り続ける。
	UPROPERTY() TObjectPtr<AActor> Proxy;
	float UnlitSeconds = 0.f;
};

/** タイトル画面の「人間の視線」。カーソルの先を懐中電灯で照らし、光が当たったおもちゃだけ固める。 */
UCLASS()
class TOONSTORY_API ATBTitleGaze : public AActor
{
	GENERATED_BODY()
public:
	ATBTitleGaze();
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	// カメラからカーソルへ向かう線。テストからも直接与える。
	void SetGazeRay(const FVector& Origin, const FVector& Direction);
	void ClearGaze();
	void UpdateGaze(float DeltaTime);
	bool IsFrozen(const AActor* Toy) const;
	int32 NumFrozen() const
	{
		return Frozen.Num();
	}
	TWeakObjectPtr<UTBTitleMenu> Menu;

private:
	void Freeze(AActor* Toy);
	void Release(int32 Index);
	void ReleaseAll();
	AActor* MakeProxy(AActor* Toy);
	bool IsBlocked(const FVector& Target) const;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> Light;
	UPROPERTY(Transient) TArray<FTBFrozenToy> Frozen;
	bool bHasRay = false;
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ForwardVector;
};
