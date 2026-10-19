#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBRoomLighting.generated.h"

class UBoxComponent;
class ALight;

/** Explicit room bounds and server-owned lighting. Unassigned space keeps the original gaze rules. */
UCLASS()
class TOONSTORY_API ATBRoomLighting : public AActor
{
	GENERATED_BODY()
public:
	ATBRoomLighting();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Room")
	TObjectPtr<UBoxComponent> Bounds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
	bool bInitiallyLightsOn = true;
	UPROPERTY(ReplicatedUsing = OnRep_LightsOn, BlueprintReadOnly, Category = "Room")
	bool bLightsOn = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room", meta = (ClampMin = "0.0", Units = "m"))
	float DarkProtectionDistanceMeters = 3.f;
	// Each light should belong to exactly one room. Use Movable local lights.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Room")
	TArray<TObjectPtr<ALight>> Lights;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Room")
	void SetLightsOn(bool bOn);
	void ResetLighting();
	bool Contains(const FVector& Position) const;
	// Overlapping volumes use the most restrictive rule: a lit room always wins.
	static bool ProtectsFromGaze(const UWorld* World, const FVector& ToyPosition, const FVector& HumanPosition);
	UFUNCTION()
	void OnRep_LightsOn();
};
