#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBMirrorData.generated.h"

/** 配置型の鏡の共通設定。反射は1回まで、寸法の単位はcm。 */
UCLASS(BlueprintType)
class TOONSTORY_API UTBMirrorData : public UDataAsset
{
	GENERATED_BODY()
public:
	/** 鏡面の幅。描画と視認判定で共用する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror", meta=(DisplayName="鏡面の幅", ClampMin="10", Units="cm"))
	float Width = 100.f;
	/** 鏡面の高さ。描画と視認判定で共用する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror", meta=(DisplayName="鏡面の高さ", ClampMin="10", Units="cm"))
	float Height = 160.f;
	/** 枠の幅と本体の厚み。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror", meta=(DisplayName="枠の幅", ClampMin="1", Units="cm"))
	float FrameWidth = 4.f;
	/** 視点から鏡の中央までの有効距離。描画と視認判定で共用する。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror", meta=(DisplayName="有効距離", ClampMin="100", Units="cm"))
	float ViewDistance = 1500.f;
	/** 反射映像の横解像度。縦はプレイヤーの画面比から決める。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering", meta=(DisplayName="反射映像の横解像度", ClampMin="128", ClampMax="2048"))
	int32 CaptureWidth = 512;
	/** 反射映像の最大更新回数。視認判定の頻度には影響しない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering", meta=(DisplayName="反射映像の毎秒更新回数", ClampMin="1", ClampMax="60"))
	float FramesPerSecond = 30.f;
	/** 反射テクスチャを表示する鏡面用のマテリアル。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering", meta=(DisplayName="鏡面のマテリアル"))
	TSoftObjectPtr<class UMaterialInterface> MirrorMaterial;
	/** 枠と本体に使うマテリアル。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering", meta=(DisplayName="枠のマテリアル"))
	TSoftObjectPtr<class UMaterialInterface> FrameMaterial;
};
