#pragma once
#include "Animation/AnimInstance.h"
#include "TBHumanPose.generated.h"

// Small post-process: two leg chains for crouching and a relaxed carrying grip.
UCLASS()
class TOONSTORY_API UTBHumanPose : public UAnimInstance
{
	GENERATED_BODY()
public:
	UTBHumanPose();
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UAnimSequence>> GestureBases;
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};

// Reproduce gameplay rotations on the MetaHuman's own proportions.
UCLASS()
class TOONSTORY_API UTBHumanBodyPose : public UAnimInstance
{
	GENERATED_BODY()
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
