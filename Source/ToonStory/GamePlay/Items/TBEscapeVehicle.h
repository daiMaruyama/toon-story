#pragma once

#include "CoreMinimal.h"
#include "GamePlay/Items/TBItemBase.h"
#include "TBEscapeVehicle.generated.h"

class ATBCharacter;
class UBoxComponent;
class UTBEscapeVehicleData;

/** 仮の脱出車両。状態の決定と移動はサーバーのみで行う。 */
UCLASS(Blueprintable)
class TOONSTORY_API ATBEscapeVehicle : public ATBItemBase
{
	GENERATED_BODY()
public:
	ATBEscapeVehicle();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	bool Board(ATBCharacter* Toy);
	bool Leave(ATBCharacter* Toy);
	void SetDrivingInput(ATBCharacter* Toy, FVector2D Input);
	const UTBEscapeVehicleData& Data() const;
	bool IsDriver(const ATBCharacter* Toy) const;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="脱出車両")
	TObjectPtr<UBoxComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category="脱出車両")
	TObjectPtr<UBoxComponent> BoardingArea;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="脱出車両")
	TArray<TObjectPtr<ATBCharacter>> Passengers;
	UPROPERTY(EditAnywhere, Category="脱出車両")
	TSoftObjectPtr<UTBEscapeVehicleData> VehicleData;
protected:
	virtual bool CanPerformAction_Implementation(const FTBInteractionContext& Context) const override;
	virtual bool PerformAction_Implementation(const FTBInteractionContext& Context) override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UTBEscapeVehicleData> LoadedData;
	FVector2D DriveInput = FVector2D::ZeroVector;
	double LastInputAt = 0;
	float VerticalVelocity = 0.f;
	UFUNCTION()
	void OnBoardingOverlap(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent,
	                       int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);
};
