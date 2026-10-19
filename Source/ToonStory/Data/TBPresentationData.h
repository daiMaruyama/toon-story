// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBPresentationData.generated.h"

USTRUCT(BlueprintType)
struct FTBLidFeel
{
	GENERATED_BODY()
	/** 開くときに天井（蓋）を持ち上げる角度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱のふた",
	          meta = (DisplayName = "開く角度", Units = "Degrees", ClampMin = "0", ClampMax = "80"))
	float OpenAngle = 60.f;
	/** 開いた隙間から差し込む光の、いちばん明るいときの強さ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱のふた", meta = (DisplayName = "光の強さ", ClampMin = "0"))
	float PeakIntensity = 30000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱のふた",
	          meta = (DisplayName = "開くのにかかる時間", Units = "s", ClampMin = "0.01"))
	float OpenSeconds = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱のふた",
	          meta = (DisplayName = "開いたままの時間", Units = "s", ClampMin = "0"))
	float HoldSeconds = .9f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱のふた",
	          meta = (DisplayName = "閉じるのにかかる時間", Units = "s", ClampMin = "0.01"))
	float CloseSeconds = .8f;
	/** 開いた瞬間だけ部屋全体を明るくする量（露出の補正） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱のふた", meta = (DisplayName = "部屋が明るくなる量", ClampMin = "0"))
	float FlashExposure = 2.5f;
	/** 光が引いてから開いた蓋が見えるよう、蓋より早く戻す */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱のふた",
	          meta = (DisplayName = "部屋の明るさが戻るまで", Units = "s", ClampMin = "0.01"))
	float FlashSeconds = .35f;
};

USTRUCT(BlueprintType)
struct FTBTelevisionFeel
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "テレビ", meta = (DisplayName = "映像の解像度"))
	FIntPoint CaptureResolution = FIntPoint(512, 288);
	/** 1秒に何回、映像を撮り直すか */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "テレビ",
	          meta = (DisplayName = "映像の更新回数（毎秒）", ClampMin = "1", ClampMax = "30"))
	float CaptureRate = 15.f;
	/** チャンネルを変えたときに砂嵐を挟む時間 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "テレビ",
	          meta = (DisplayName = "切り替えの砂嵐", Units = "s", ClampMin = "0", ClampMax = "2"))
	float NoiseSeconds = .4f;
	/** 箱に入れられた瞬間に見る位置の、画面中心からの高さ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "テレビ",
	          meta = (DisplayName = "入ったときに見上げる高さ", Units = "cm"))
	float ArrivalLookAbove = 100.f;
};

USTRUCT(BlueprintType)
struct FTBRemoteFeel
{
	GENERATED_BODY()
	/** 立ち位置の揺れや乗り直しで、同じ操作が続けて起きないようにする */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "リモコン",
	          meta = (DisplayName = "ボタンを続けて押せるまで", Units = "s", ClampMin = "0"))
	float PressCooldown = .5f;
};

/** 箱のふた・部屋の光、テレビ、リモコンの演出 */
UCLASS(BlueprintType)
class TOONSTORY_API UTBPresentationData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱のふた", meta = (ShowOnlyInnerProperties))
	FTBLidFeel Lid;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "テレビ", meta = (ShowOnlyInnerProperties))
	FTBTelevisionFeel Television;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "リモコン", meta = (ShowOnlyInnerProperties))
	FTBRemoteFeel Remote;
};

namespace TB
{
	// UTBDataSettings で選んだ演出のデータ。未設定なら C++ の初期値。
	TOONSTORY_API const UTBPresentationData& PresentationData();
}
