#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBBoxEscapeCourse.generated.h"

class ATBBox;
class ATBCharacter;
class ATriggerBox;
class ATargetPoint;
class UBoxComponent;
class UPrimitiveComponent;

/** Server-owned height-based escape. No checkpoint sequence or automatic fall reset. */
UCLASS()
class TOONSTORY_API ATBBoxEscapeCourse : public AActor
{
	GENERATED_BODY()
public:
	ATBBoxEscapeCourse();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> CourseBounds;
	UPROPERTY(EditInstanceOnly, Category = "Course")
	TObjectPtr<ATBBox> Box;
	/** The bottom of this volume defines the finish height anywhere inside CourseBounds. */
	UPROPERTY(EditInstanceOnly, Category = "Course", meta = (DisplayName = "最終到達領域"))
	TObjectPtr<ATriggerBox> Goal;
	// Resolve component tags in the loaded shared level; no cross-level actor/component hard reference.
	UPROPERTY(EditInstanceOnly, Category = "Course")
	FName FinalSupportTag = TEXT("TBBoxEscapeFinalSupport");
	UPROPERTY(EditInstanceOnly, Category = "Course")
	FName FinalSupportLevelPackage = TEXT("/Game/Maps/TB_ToyBoxRoom");
	UPROPERTY(EditInstanceOnly, Category = "Course", meta = (ClampMin = "1", ClampMax = "2"))
	int32 ExpectedFinalSupportCount = 2;
	UPROPERTY(EditInstanceOnly, Category = "Course", meta = (ClampMin = "0", ClampMax = "5"))
	float GoalMarginCm = 1.f;
	UPROPERTY(EditInstanceOnly, Category = "Course")
	TArray<TObjectPtr<ATargetPoint>> ExitPoints;
	bool Validate(FString& Error, bool CheckSafety = true) const;
	bool Protects(const ATBCharacter* Toy) const;
	bool ReachedGoal(const ATBCharacter* Toy) const;
	static ATBBoxEscapeCourse* Find(const UWorld* World, const ATBBox* ForBox);
private:
	struct FCaptureAttempt
	{
		uint32 Generation = 0;
		bool bDescendedBelowGoal = false;
	};
	TMap<TWeakObjectPtr<ATBCharacter>, FCaptureAttempt> Attempts;
	bool Contains(const UBoxComponent* Volume, const ATBCharacter* Toy, float Margin = 0) const;
	bool ValidateWithSupports(FString& Error, bool CheckSafety, TArray<UPrimitiveComponent*>& Supports) const;
	bool ResolveFinalSupports(TArray<UPrimitiveComponent*>& Out, FString& Error) const;
	bool IsAtGoalHeight(const ATBCharacter* Toy) const;
	bool Eligible(const ATBCharacter* Toy) const;
	bool SafeExitPoint(FVector Position, const ATBCharacter* Toy = nullptr) const;
};
