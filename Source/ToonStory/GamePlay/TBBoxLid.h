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
	// 開くときに天井を持ち上げる角度。
	UPROPERTY(EditAnywhere, Category = "Lid", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float OpenAngle = 30.f;
	UPROPERTY(EditAnywhere, Category = "Lid", meta = (ClampMin = "0.0"))
	float PeakIntensity = 20000.f;
	UPROPERTY(EditAnywhere, Category = "Lid", meta = (ClampMin = "0.01"))
	float OpenSeconds = .25f;
	UPROPERTY(EditAnywhere, Category = "Lid", meta = (ClampMin = "0.0"))
	float HoldSeconds = .9f;
	UPROPERTY(EditAnywhere, Category = "Lid", meta = (ClampMin = "0.01"))
	float CloseSeconds = .8f;
	// 開いた瞬間だけ部屋全体を明るくする。光が引いてから開いた蓋が見えるよう、蓋より早く戻す。
	UPROPERTY(EditInstanceOnly, Category = "Lid")
	TObjectPtr<APostProcessVolume> RoomVolume;
	UPROPERTY(EditAnywhere, Category = "Lid", meta = (ClampMin = "0.0"))
	float FlashExposure = 2.5f;
	UPROPERTY(EditAnywhere, Category = "Lid", meta = (ClampMin = "0.01"))
	float FlashSeconds = .35f;

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
