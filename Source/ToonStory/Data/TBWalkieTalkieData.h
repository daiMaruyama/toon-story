#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBWalkieTalkieData.generated.h"

/** トランシーバーの受信音と操作範囲。 */
UCLASS(BlueprintType)
class TOONSTORY_API UTBWalkieTalkieData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="トランシーバー", meta=(DisplayName="操作距離", ClampMin="1", Units="cm"))
	float InteractionDistance = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="トランシーバー", meta=(DisplayName="受信音の届く距離", ClampMin="1", Units="cm"))
	float AudibleDistance = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="トランシーバー", meta=(DisplayName="受信音の高音域制限", ClampMin="100", ClampMax="20000", Units="Hz"))
	float FilterFrequency = 2200.f;
};
