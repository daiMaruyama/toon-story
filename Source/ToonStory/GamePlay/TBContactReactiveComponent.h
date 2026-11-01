#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "TBContactReactiveComponent.generated.h"

class ATBCharacter;
class ATBGameState;
class UBoxComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class ETBContactMotionStatus : uint8
{
	Idle, Moving, Held, Blocked, ResetPending, Disabled
};

/** A complete approved pose, rather than a command to run a client-side timeline. */
USTRUCT(BlueprintType)
struct FTBContactMotionSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bInitialized = false;
	UPROPERTY() uint32 Generation = 0;
	UPROPERTY() uint32 Round = 0;
	UPROPERTY(BlueprintReadOnly) ETBContactMotionStatus Status = ETBContactMotionStatus::Disabled;
	UPROPERTY(BlueprintReadOnly) float Angle = 0;
	UPROPERTY(BlueprintReadOnly) FTransform Baseline = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) FTransform Pose = FTransform::Identity;
	UPROPERTY(BlueprintReadOnly) FString Problem;
};

/** Contact-only bounded rotation. One movable box proxy owns collision; attached visuals do not. */
UCLASS(ClassGroup = (ToonStory), meta = (BlueprintSpawnableComponent))
class TOONSTORY_API UTBContactReactiveComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTBContactReactiveComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, Category = "Contact", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.BoxComponent"))
	FComponentReference MotionTarget;
	UPROPERTY(EditAnywhere, Category = "Contact", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.PrimitiveComponent"))
	FComponentReference ContactTrigger;
	// Axis and pivot are expressed in the fixed parent's coordinates, not the mesh's changing coordinates.
	UPROPERTY(EditAnywhere, Category = "Motion") FVector LocalAxis = FVector::UpVector;
	UPROPERTY(EditAnywhere, Category = "Motion") FVector LocalPivot = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Motion", meta = (ClampMin = "-110", ClampMax = "110")) float AngleDegrees = 60;
	UPROPERTY(EditAnywhere, Category = "Motion", meta = (ClampMin = "0.1", ClampMax = "10")) float MotionSeconds = .75f;
	UPROPERTY(EditAnywhere, Category = "Motion") bool bReturnAfterContact = false;
	UPROPERTY(EditAnywhere, Category = "Contact") bool bOncePerRound = true;
	UPROPERTY(EditAnywhere, Category = "Contact", meta = (ClampMin = "0.05", ClampMax = "30")) float CooldownSeconds = 1;
	UPROPERTY(EditAnywhere, Category = "Contact", meta = (ClampMin = "0.1", ClampMax = "5")) float MaxContactGapCm = 2;
	UPROPERTY(EditAnywhere, Category = "Safety", meta = (ClampMin = "0.5", ClampMax = "30")) float TimeoutSeconds = 5;

	UFUNCTION(BlueprintPure, Category = "Contact") FTBContactMotionSnapshot GetMotionSnapshot() const { return Snapshot; }
	UFUNCTION(BlueprintPure, Category = "Contact") FString GetMotionProblem() const { return Snapshot.Problem.IsEmpty() ? Problem : Snapshot.Problem; }
	bool ValidateSetup(FString& Error) const;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTBContactReactiveTest;
#endif
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot) FTBContactMotionSnapshot Snapshot;
	UPROPERTY(Transient) TObjectPtr<UBoxComponent> Target;
	UPROPERTY(Transient) TObjectPtr<UPrimitiveComponent> Trigger;
	UPROPERTY(Transient) FString Problem;
	TWeakObjectPtr<USceneComponent> FixedParent;
	FTransform ParentAtRest;
	FVector AxisAtRest, PivotAtRest, ExtentAtRest;
	float LimitAtRest = 0, DurationAtRest = 0, CooldownAtRest = 0, TimeoutAtRest = 0, GapAtRest = 0;
	bool bReturnAtRest = false, bOnceAtRest = true, bReady = false, bArmed = true;
	bool bReactedThisRound = false, bReturning = false, bResetting = false;
	float GoalAngle = 0;
	double Deadline = 0, NextContactTime = 0;
	TSet<TWeakObjectPtr<ATBCharacter>> Contacts;
	TWeakObjectPtr<ATBGameState> ObservedState;
	uint32 ObservedRound = MAX_uint32;
	uint8 ObservedPhase = MAX_uint8;

	UFUNCTION() void OnRep_Snapshot();
	UFUNCTION() void ContactBegan(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	    int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
	UFUNCTION() void ContactEnded(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 BodyIndex);
	bool BindSetup();
	bool RuntimeSetupValid() const;
	bool NearSurface(const ATBCharacter* Body) const;
	bool SyncRound(double Now, ATBCharacter* Entering = nullptr);
	void RebuildContacts(ATBCharacter* Entering);
	void AdvanceServer(float DeltaTime, double Now);
	void Publish();
	void Disable(const FString& Why);
	FTransform PoseAt(float Angle) const;
	bool SegmentClear(float From, float To) const;
	bool ApplyApproved(float Angle);
};
