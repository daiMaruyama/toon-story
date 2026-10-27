#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBBoxEscapeCourse.generated.h"

class ATBBox;
class ATBCharacter;
class ATriggerBox;
class ATargetPoint;
class UBoxComponent;

/** Server-owned height escape. No checkpoint sequence or automatic fall reset. */
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
	/** The marker's world Z is the escape line. Its XY and trigger extent do not gate escape. */
	UPROPERTY(EditInstanceOnly, Category = "Course", meta = (DisplayName = "脱出高さマーカー"))
	TObjectPtr<ATriggerBox> Goal;
	UPROPERTY(EditInstanceOnly, Category = "Course")
	TArray<TObjectPtr<ATargetPoint>> ExitPoints;
	bool Validate(FString& Error, bool CheckSafety = true) const;
	bool Protects(const ATBCharacter* Toy) const;
	bool AboveEscapeHeight(const ATBCharacter* Toy) const;
	static ATBBoxEscapeCourse* Find(const UWorld* World, const ATBBox* ForBox);
private:
	struct FCaptureAttempt
	{
		uint32 Generation = 0;
		bool bDescendedBelowLine = false;
	};
	TMap<TWeakObjectPtr<ATBCharacter>, FCaptureAttempt> Attempts;
	bool Contains(const UBoxComponent* Volume, const ATBCharacter* Toy) const;
	bool Eligible(const ATBCharacter* Toy) const;
	bool SafeExitPoint(FVector Position, const ATBCharacter* Toy = nullptr) const;
};
