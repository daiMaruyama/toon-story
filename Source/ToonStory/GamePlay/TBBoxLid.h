// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBBoxLid.generated.h"

class APostProcessVolume;
class UMaterialInstanceDynamic;
class USpotLightComponent;

/** 箱の蓋。誰かが入れられる・助け出されるたびに、天井が光って開いたように見せる。各PCで再生する。 */
UCLASS()
class TOONSTORY_API ATBBoxLid : public AActor
{
	GENERATED_BODY()
public:
	ATBBoxLid();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Lid")
	TObjectPtr<USpotLightComponent> Light;
	// 蓋にあたる天井。開き具合に合わせてマテリアルの Glow を上げる。
	UPROPERTY(EditInstanceOnly, Category = "Lid")
	TObjectPtr<AActor> Ceiling;
	// 開いた瞬間だけ部屋全体を明るくする。
	UPROPERTY(EditInstanceOnly, Category = "Lid")
	TObjectPtr<APostProcessVolume> RoomVolume;
	// 開く角度・光の強さ・時間はマスターデータ（TB::PresentationData().Lid）にある。

private:
	bool bBoundToBox = false;
	float Elapsed = -1.f;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CeilingMaterial;
	FTransform CeilingRest;
	FVector Hinge = FVector::ZeroVector;
	float OpenSign = 1.f;
	float RestExposure = 0.f;
	bool bRestExposureOverride = false;
	void Open();
	void SetOpen(float Alpha);
	void SetFlash(float Alpha);
};
