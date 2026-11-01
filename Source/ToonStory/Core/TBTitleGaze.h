// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Components/SkinnedMeshComponent.h"
#include "GameFramework/Actor.h"
#include "TBTitleGaze.generated.h"

class UPoseableMeshComponent;
class USkeletalMeshComponent;
class USpotLightComponent;
class UTBTitleMenu;

// 人形の1パーツと、写し元の本物。
USTRUCT()
struct FTBFrozenMesh
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Source;
	// 骨の姿勢の写し元。毛並みの層は本体。
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> PoseSource;
	UPROPERTY() TObjectPtr<UPoseableMeshComponent> Pose;
	// 踊りへ戻り始めたときの姿。
	FTransform StartWorld;
	TArray<FTransform> StartBones;
};

USTRUCT()
struct FTBFrozenToy
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AActor> Toy;
	// 固まった瞬間の姿を写し取った人形。本物は隠したままシーケンスで踊り続ける。
	UPROPERTY() TObjectPtr<AActor> Proxy;
	UPROPERTY() TArray<FTBFrozenMesh> Meshes;
	// 隠している間も本物の骨を動かすため、元の設定を覚えておく。
	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Ticked;
	TArray<EVisibilityBasedAnimTickOption> OldTicks;
	float UnlitSeconds = 0.f;
	// 0 以上なら、固まった姿から今の踊りへ溶け込ませている途中。
	float BlendSeconds = -1.f;
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
	void MakeProxy(FTBFrozenToy& F);
	void StartBlend(FTBFrozenToy& F);
	void Blend(FTBFrozenToy& F, float Alpha);
	bool IsBlocked(const FVector& Target) const;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> Light;
	UPROPERTY(Transient) TArray<FTBFrozenToy> Frozen;
	bool bHasRay = false;
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ForwardVector;
};
