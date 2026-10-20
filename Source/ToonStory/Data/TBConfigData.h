// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBConfigData.generated.h"

/** プレイヤーが変えられる音量1つ分の、初期値と範囲 */
USTRUCT(BlueprintType)
struct FTBVolumeSetting
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量",
	          meta = (DisplayName = "初期値", ClampMin = "0", ClampMax = "1"))
	float Default = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量",
	          meta = (DisplayName = "下限", ClampMin = "0", ClampMax = "1"))
	float Min = 0.f;
	/** 上げて得をしないよう、1 より上にはできない */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量",
	          meta = (DisplayName = "上限", ClampMin = "0", ClampMax = "1"))
	float Max = 1.f;

	// 範囲に収めた値。数でない値は初期値にする。データの値が壊れていても 0〜1 からは出さない。
	float Clamp(float Value) const
	{
		const float Low = FMath::Clamp(Min, 0.f, 1.f);
		const float High = FMath::Clamp(Max, Low, 1.f);
		return FMath::Clamp(FMath::IsFinite(Value) ? Value : Default, Low, High);
	}
};

USTRUCT(BlueprintType)
struct FTBAudioConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量", meta = (DisplayName = "全体"))
	FTBVolumeSetting Master;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量", meta = (DisplayName = "BGM"))
	FTBVolumeSetting Music;
	/** 足音も含む。足音だけを上げて索敵が有利にならないよう、別の項目にはしない */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量", meta = (DisplayName = "効果音"))
	FTBVolumeSetting Effects;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量", meta = (DisplayName = "ボイスチャット"))
	FTBVolumeSetting Voice;
};

/** プレイヤーが変えられる設定の、初期値と範囲。プレイヤーが選んだ値は UTBGameUserSettings に保存する */
UCLASS(BlueprintType)
class TOONSTORY_API UTBConfigData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量", meta = (ShowOnlyInnerProperties))
	FTBAudioConfig Audio;

#if WITH_EDITOR
	// 1つずつの範囲は ClampMin/Max で守り、ここでは「下限 ≤ 初期値 ≤ 上限」を確かめる。
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

namespace TB
{
	// UTBDataSettings で選んだコンフィグのデータ。未設定なら C++ の初期値。
	TOONSTORY_API const UTBConfigData& ConfigData();
}
