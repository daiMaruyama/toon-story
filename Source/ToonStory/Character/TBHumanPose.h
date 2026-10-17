#pragma once
#include "Animation/AnimInstance.h"
#include "TBHumanPose.generated.h"

// Small post-process: two leg chains for crouching and a relaxed carrying grip.
UCLASS()
class TOONSTORY_API UTBHumanPose : public UAnimInstance
{
	GENERATED_BODY()
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
