#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Data/TBTypes.h"
#include "TBMovement.generated.h"

/** 凍結中は横移動だけを止め、重力による落下を継続する。 */
UCLASS()
class TOONSTORY_API UTBMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	virtual void PerformMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags,
	                            const FVector& NewAccel) override;
};
