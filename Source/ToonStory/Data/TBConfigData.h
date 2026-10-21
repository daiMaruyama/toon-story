// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBConfigData.generated.h"

/** プレイヤーが変えられる音量の種類 */
UENUM()
enum class ETBVolume : uint8
{
	Master,
	Music,
	Effects,
	Voice
};

namespace TBSettings
{
	// Origin から Step ごとの目盛りにそろえて、Steps 個ぶん動かした値。目盛りの間にあっても、1回で隣の目盛りに止まる。
	inline float StepFrom(float Value, float Origin, float Step, int32 Steps)
	{
		const float Size = FMath::Max(Step, KINDA_SMALL_NUMBER);
		const float Position = (Value - Origin) / Size;
		const float Next = Steps > 0 ? FMath::FloorToFloat(Position + .001f) + Steps : FMath::CeilToFloat(Position - .001f) + Steps;
		return Origin + Next * Size;
	}
}

/** プレイヤーが変えられる数値1つ分の、初期値・範囲・刻み */
USTRUCT(BlueprintType)
struct FTBNumberSetting
{
	GENERATED_BODY()
	FTBNumberSetting() = default;
	FTBNumberSetting(float InDefault, float InMin, float InMax, float InStep)
	    : Default(InDefault), Min(InMin), Max(InMax), Step(InStep)
	{
	}
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "設定", meta = (DisplayName = "初期値"))
	float Default = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "設定", meta = (DisplayName = "下限"))
	float Min = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "設定", meta = (DisplayName = "上限"))
	float Max = 1.f;
	/** 設定画面で「<」「>」を1回押したときに変わる量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "設定", meta = (DisplayName = "刻み", ClampMin = "0.001"))
	float Step = .1f;

	// 範囲に収めた値。数でない値は初期値にする。
	float Clamp(float Value) const
	{
		return FMath::Clamp(FMath::IsFinite(Value) ? Value : Default, Min, FMath::Max(Min, Max));
	}
	float StepFrom(float Value, int32 Steps) const
	{
		return Clamp(TBSettings::StepFrom(Value, Min, Step, Steps));
	}
};

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
	/** 設定画面で「-」「+」を1回押したときに変わる量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量", meta = (DisplayName = "刻み", ClampMin = "0.01", ClampMax = "0.5"))
	float Step = .1f;

	const FTBVolumeSetting& Get(ETBVolume Kind) const
	{
		switch (Kind)
		{
			case ETBVolume::Music:
				return Music;
			case ETBVolume::Effects:
				return Effects;
			case ETBVolume::Voice:
				return Voice;
			default:
				return Master;
		}
	}
};

USTRUCT(BlueprintType)
struct FTBDisplayConfig
{
	GENERATED_BODY()
	/** 0 低 / 1 中 / 2 高 / 3 最高 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面",
	          meta = (DisplayName = "画質の初期値", ClampMin = "0", ClampMax = "3"))
	int32 DefaultQuality = 3;
	/** 画質を下げても、これより下げない。1（中）より下だと Lumen が切れて部屋が真っ暗になる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面",
	          meta = (DisplayName = "GI の下限", ClampMin = "0", ClampMax = "3"))
	int32 MinGlobalIlluminationQuality = 1;
	/** 影がなくなると、物陰や暗がりに隠れられなくなる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面",
	          meta = (DisplayName = "影の下限", ClampMin = "0", ClampMax = "3"))
	int32 MinShadowQuality = 1;
	/** 下げると遠くの小物が消えたり簡略化されたりして、隠れているおもちゃを見つけやすくなる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面",
	          meta = (DisplayName = "描画距離の下限", ClampMin = "0", ClampMax = "3"))
	int32 MinViewDistanceQuality = 2;
	/** 描画解像度（%）。上げると遠くの小さいおもちゃがくっきり見えるので、100 より上にはできない */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面", meta = (DisplayName = "描画解像度（%）"))
	FTBNumberSetting ResolutionScale = FTBNumberSetting(100.f, 50.f, 100.f, 5.f);
	/** 表示のガンマ（標準は 2.2）。暗がりを明るくしすぎると隠れているおもちゃが見えるので、上げる幅は小さくする */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面", meta = (DisplayName = "明るさ"))
	FTBNumberSetting Brightness = FTBNumberSetting(2.2f, 1.8f, 2.5f, .1f);
	/** 0 は上限なし */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面", meta = (DisplayName = "fps の上限の選択肢"))
	TArray<int32> FrameRateLimits = {30, 60, 120, 144, 0};
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面", meta = (DisplayName = "fps の上限の初期値"))
	int32 DefaultFrameRateLimit = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面", meta = (DisplayName = "VSync の初期値"))
	bool bDefaultVSync = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面", meta = (DisplayName = "モーションブラーの初期値"))
	bool bDefaultMotionBlur = true;
	/** 光の計算（Lumen）にレイトレーシングを使うか。対応している GPU だけ選べる。
	    切ると暗がりが暗くなる（自分が見えにくくなるだけ）かわりに、動作が軽くなる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面", meta = (DisplayName = "レイトレーシングの初期値"))
	bool bDefaultRayTracing = true;
};

USTRUCT(BlueprintType)
struct FTBControlConfig
{
	GENERATED_BODY()
	/** 視点を動かす速さの倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "操作", meta = (DisplayName = "マウス感度"))
	FTBNumberSetting MouseSensitivity = FTBNumberSetting(1.f, .2f, 3.f, .1f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "操作", meta = (DisplayName = "上下反転の初期値"))
	bool bDefaultInvertY = false;
};

/** プレイヤーが変えられる設定の、初期値と範囲。プレイヤーが選んだ値は UTBGameUserSettings に保存する */
UCLASS(BlueprintType)
class TOONSTORY_API UTBConfigData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "音量", meta = (ShowOnlyInnerProperties))
	FTBAudioConfig Audio;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "画面", meta = (ShowOnlyInnerProperties))
	FTBDisplayConfig Display;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "操作", meta = (ShowOnlyInnerProperties))
	FTBControlConfig Controls;

#if WITH_EDITOR
	// 1つずつの範囲は ClampMin/Max で守り、ここでは「下限 ≤ 初期値 ≤ 上限」など値どうしの関係を確かめる。
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

namespace TB
{
	// UTBDataSettings で選んだコンフィグのデータ。未設定なら C++ の初期値。
	TOONSTORY_API const UTBConfigData& ConfigData();
}
