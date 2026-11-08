#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBEscapeVehicleData.generated.h"

/** 脱出車両の調整値。DA_EscapeVehicle が未設定なら初期値を使う。 */
UCLASS(BlueprintType)
class TOONSTORY_API UTBEscapeVehicleData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="脱出車両", meta=(DisplayName="走行速度", ClampMin="0", Units="cm/s"))
	float DriveSpeed = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="脱出車両", meta=(DisplayName="旋回速度", ClampMin="0", Units="deg/s"))
	float TurnSpeed = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="脱出車両", meta=(DisplayName="降車に必要な長押し", ClampMin="0.1", Units="s"))
	float ExitHoldSeconds = .6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="脱出車両", meta=(DisplayName="搭乗の操作距離", ClampMin="1", Units="cm"))
	float InteractionDistance = 180.f;
};
