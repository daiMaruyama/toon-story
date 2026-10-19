#pragma once
#include "CoreMinimal.h"
#include "Character/TBToyPresentation.h"
#include "GameFramework/Actor.h"
#include "TBDummyToy.generated.h"
class ATBCharacter;
class UChildActorComponent;
class USphereComponent;
class UCapsuleComponent;
class ATBBox;
// Decoys share carrying/storage behavior but never contribute to player victory counters.
UCLASS()
class TOONSTORY_API ATBDummyToy : public AActor
{
	GENERATED_BODY()
public:
	ATBDummyToy();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Root;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCapsuleComponent> Collision;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UChildActorComponent> Visual;
	UPROPERTY(EditAnywhere, ReplicatedUsing = RefreshVisual, Category = "Toy")
	TSubclassOf<AActor> VisualClass;
	UPROPERTY(ReplicatedUsing = OnRep_Holder)
	TObjectPtr<ATBCharacter> Holder;
	UPROPERTY(ReplicatedUsing = OnRep_Holder)
	TObjectPtr<ATBBox> StoredBox;
	UPROPERTY(ReplicatedUsing = OnRep_Placement)
	bool bFalling = false;
	UFUNCTION()
	void OnRep_Placement();
	void Place(const FVector& Position, ATBBox* Box = nullptr);
	UFUNCTION()
	void RefreshVisual();
	UFUNCTION()
	void OnRep_Holder();
	static bool TryPickUpNearest(ATBCharacter* Human, float ClosestDistance);
	bool PickUp(ATBCharacter* Human);
	void PutDown();
	void ResetPlacement(bool bRandomizeAppearance = false);

private:
	void ChooseRandomToy();
	void Fall(float DeltaSeconds);
	float FallSpeed = 0;
	FTBGripCache GripCache;
	int32 SettleFrames = 12;
	FTransform InitialTransform;
};
