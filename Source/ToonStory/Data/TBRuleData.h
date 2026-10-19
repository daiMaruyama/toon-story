// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBRuleData.generated.h"

USTRUCT(BlueprintType)
struct FTBVisionRules
{
	GENERATED_BODY()
	/** 横の視野角。カメラと視界判定の両方が使う */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "視野角", Units = "Degrees", ClampMin = "60", ClampMax = "120"))
	float FieldOfView = 90.f;
	/** カメラはこの比率に固定され、判定の縦の広さもここから決まる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "画面比率（幅）", ClampMin = "1", ClampMax = "32"))
	int32 AspectWidth = 16;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "画面比率（高さ）", ClampMin = "1", ClampMax = "32"))
	int32 AspectHeight = 9;
	/** 判定を画面の上下左右の端より、それぞれ内側にする角度。端にわずかに映った人形は止めない */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "判定の余白（片側）", Units = "Degrees", ClampMin = "0", ClampMax = "15"))
	float EdgeMargin = 5.f;
};

/** 作る側が決めるルールの値。試合中は読むだけ */
UCLASS(BlueprintType)
class TOONSTORY_API UTBRuleData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界", meta = (ShowOnlyInnerProperties))
	FTBVisionRules Vision;
};
