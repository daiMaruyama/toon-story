#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "TBTVCamera.generated.h"

/** 箱内テレビの固定カメラ。レベルに置いて番号を決めるだけで、テレビがそのチャンネルに登録する。 */
UCLASS()
class TOONSTORY_API ATBTVCamera : public ACameraActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Television", meta = (ClampMin = "0", ClampMax = "2"))
	int32 Channel = 0;
};
