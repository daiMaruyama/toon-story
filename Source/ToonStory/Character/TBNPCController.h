// Copyright (c) 2026 Mamodou. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "TBNPCController.generated.h"

class ATBCharacter;
class ATBPickup;

/** Server-owned match participant. Uses the same interactions as a human player. */
UCLASS()
class TOONSTORY_API ATBNPCController : public AAIController
{
	GENERATED_BODY()
public:
	ATBNPCController();
	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
	bool CanSee(const AActor* Actor) const;
	bool IsTracking(const AActor* Actor) const { return Target.Get() == Actor && GetFocusActor() == Actor; }
	UPROPERTY(EditDefaultsOnly, Category = "NPC", meta = (ClampMin = "0", ClampMax = "1"))
	float OverlookChance = .3f;
	UPROPERTY(EditDefaultsOnly, Category = "NPC", meta = (ClampMin = "0"))
	float RecognitionSeconds = 1.f;
	UPROPERTY(EditDefaultsOnly, Category = "NPC", meta = (ClampMin = "0.1", ClampMax = "1"))
	float HumanSpeedScale = .5f;
	UPROPERTY(EditDefaultsOnly, Category = "NPC", meta = (ClampMin = "1400"))
	float HumanSightRangeCm = 2200.f;

private:
	void ThinkHuman(ATBCharacter* Body);
	void CarryToBox(ATBCharacter* Body, class ATBBox* Box);
	int32 BoxApproach = 0;
	double NextBoxPlan = 0;
	double HumanProgressAt = 0;
	FVector HumanProgressLocation = FVector::ZeroVector;
	void ThinkToy(ATBCharacter* Body);
	void ThinkHide(ATBCharacter* Body);
	static constexpr float DefaultAcceptanceCm = 20.f;
	void WalkTo(const FVector& Destination, float Acceptance = DefaultAcceptanceCm);
	void Wander();
	bool Exposed(const FVector& Position) const;
	bool PathExposed(const TArray<FVector>& Points) const;
	TArray<TWeakObjectPtr<ATBCharacter>> VisibleHumans;
	TWeakObjectPtr<ATBPickup> BatteryTarget;
	double BatteryVisibleUntil = 0;
	TArray<FVector> VisitedPlaces;
	double WanderUntil = 0;
	TWeakObjectPtr<ATBPickup> BlockedBattery;
	double RetryBatteryAt = 0;
	double BatteryProgressAt = 0;
	float ClosestBatteryDistance = 0;
	double ReplanAt = 0;
	int32 BatteryCursor = 0;
	FVector CoverGoal = FVector::ZeroVector;
	double CoverUntil = 0;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AActor> PendingTarget;
	double RecognizeAt = 0;
	TMap<TWeakObjectPtr<AActor>, double> OverlookUntil;
	FVector LastSeen = FVector::ZeroVector;
	double ForgetAt = 0;
	FVector ProgressLocation = FVector::ZeroVector;
	double CheckProgressAt = 0;
	double NextJumpAt = 0;
	double NextThink = 0;
	double NextWalk = 0;
	FVector WalkGoal = FVector::ZeroVector;
};
