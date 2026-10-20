#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBLightSwitchData.generated.h"

/** 照明スイッチの調整値。対応する部屋は配置したスイッチ側で指定する。 */
UCLASS(BlueprintType)
class TOONSTORY_API UTBLightSwitchData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "照明スイッチ",
	          meta = (DisplayName = "操作できる距離", Units = "cm", ClampMin = "1"))
	float InteractionDistance = 180.f;
};
