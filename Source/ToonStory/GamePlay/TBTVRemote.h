// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBTVRemote.generated.h"

class ATBBoxTV;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTBTVButton;

/** 箱内テレビのリモコン。ボタンはBPで、アクタのスケールで全体の大きさを変えられる。 */
UCLASS(Blueprintable)
class TOONSTORY_API ATBTVRemote : public AActor
{
	GENERATED_BODY()
public:
	ATBTVRemote();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Remote")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditInstanceOnly, Category = "Remote")
	TObjectPtr<ATBBoxTV> TV;
	// 立ち位置の揺れや乗り直しで同じ操作が続けて起きないようにする。
	UPROPERTY(EditAnywhere, Category = "Remote", meta = (ClampMin = "0.0"))
	float PressCooldown = .5f;

private:
	double LastPressTime = -1;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTBTVButton>> Buttons;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterial;
	UFUNCTION()
	void OnButtonOverlap(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	                     int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnButtonEndOverlap(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	                        int32 OtherBodyIndex);
	// 押せる・踏まれている・押せない をボタンごとにマテリアルへ反映する。
	void UpdateGlow();
};
