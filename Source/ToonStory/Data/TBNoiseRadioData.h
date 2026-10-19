// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBNoiseRadioData.generated.h"

class USoundBase;

/** ラジオの音と、近くの人形の動作音を小さくする効果 */
UCLASS(BlueprintType)
class TOONSTORY_API UTBNoiseRadioData : public UDataAsset
{
	GENERATED_BODY()
public:
	/** 空なら仮の電子音を作って鳴らす。普通の波形は繰り返し、ループ用の音源やCueなら途切れずに鳴る */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ラジオ", meta = (DisplayName = "鳴らす音"))
	TObjectPtr<USoundBase> LoopSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ラジオ", meta = (DisplayName = "音量", ClampMin = "0", ClampMax = "4"))
	float RadioVolume = .6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ラジオ",
	          meta = (DisplayName = "聞こえる距離", Units = "cm", ClampMin = "1"))
	float AudibleDistance = 1800.f;
	/** この範囲の聞き手には、人形の動作音（足音・ジャンプ・着地）が小さく聞こえる。端に近いほど元の大きさに戻る */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ラジオ",
	          meta = (DisplayName = "動作音を小さくする範囲", Units = "cm", ClampMin = "1"))
	float MaskRadius = 1200.f;
	/** ラジオのすぐそばでの、人形の動作音の大きさ（元の大きさに対する割合） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ラジオ",
	          meta = (DisplayName = "そばでの動作音の大きさ", ClampMin = "0", ClampMax = "1"))
	float ToyActionVolume = .25f;
};
