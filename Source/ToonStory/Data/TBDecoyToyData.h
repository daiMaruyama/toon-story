#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBDecoyToyData.generated.h"

UCLASS(BlueprintType)
class TOONSTORY_API UTBDecoyToyData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ニセおもちゃ", meta=(DisplayName="移動速度", ClampMin="1", Units="cm/s"))
	float WalkSpeed = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ニセおもちゃ", meta=(DisplayName="操作距離", ClampMin="1", Units="cm"))
	float InteractionDistance = 180.f;
};
