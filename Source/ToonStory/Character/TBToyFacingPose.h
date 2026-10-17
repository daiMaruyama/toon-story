#pragma once
#include "Animation/AnimInstance.h"
#include "TBToyFacingPose.generated.h"

// Reuse the toy's walk cycle: steer the hips toward the step while keeping its torso facing the camera yaw.
UCLASS()
class TOONSTORY_API UTBToyFacingPose : public UAnimInstance
{
	GENERATED_BODY()
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
