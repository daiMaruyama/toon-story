#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBHumanEntrance.generated.h"

class APlayerStart;
class USoundBase;
class UStaticMeshComponent;
struct FCollisionQueryParams;

/** Optional authored entrance. No geometry is generated and no pawn is moved by this actor. */
UCLASS()
class TOONSTORY_API ATBHumanEntrance : public AActor
{
	GENERATED_BODY()
public:
	ATBHumanEntrance();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> ApproachStart;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> ApproachEnd;
	UPROPERTY(EditInstanceOnly, Category = "Entrance")
	TArray<TObjectPtr<APlayerStart>> DoorSlots;
	UPROPERTY(EditAnywhere, Category = "Entrance")
	TObjectPtr<USoundBase> DoorSound;
	UPROPERTY(EditAnywhere, Category = "Entrance", meta = (Units = "s", ClampMin = "0.1"))
	float FootstepIntervalSeconds = .65f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Entrance")
	TObjectPtr<UStaticMeshComponent> DoorLeaf;
	UPROPERTY(EditAnywhere, Category = "Entrance")
	FTransform ClosedDoorTransform;
	UPROPERTY(EditAnywhere, Category = "Entrance")
	FTransform OpenDoorTransform;
	void SetPreparing(bool Closed);
	bool Validate(int32 Capacity, FString& Error) const;
	bool CameraPose(float Alpha, FVector& Location, FRotator& Rotation) const;
	static ATBHumanEntrance* Find(const UWorld* World);
private:
	bool bDoorApplied = false;
	bool bDoorClosed = false;
};
