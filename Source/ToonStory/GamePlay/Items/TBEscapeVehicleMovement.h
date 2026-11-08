#pragma once
#include "CoreMinimal.h"
#include "GameFramework/MovementComponent.h"
#include "TBEscapeVehicleMovement.generated.h"

/** サーバー側の車両移動。プレイヤーと同じ安全移動・めり込み解消を使う。 */
UCLASS()
class TOONSTORY_API UTBEscapeVehicleMovement : public UMovementComponent
{
	GENERATED_BODY()
public:
	UTBEscapeVehicleMovement();
	void Advance(const FVector& HorizontalDelta, const FRotator& Rotation, float DeltaSeconds);
	bool IsGrounded() const { return bGrounded; }
	void ResetMotion() { bGrounded = false; VerticalSpeed = 0.f; }
private:
	bool TryStep(const FVector& Delta, float StepHeight, const FQuat& Rotation);
	bool bGrounded = false;
	float VerticalSpeed = 0.f;
};
