#pragma once

#include "CoreMinimal.h"
#include "Core/ToonStoryGameState.h"
#include "BatteryTagGameState.generated.h"

USTRUCT(BlueprintType)
struct FBatteryTagProgress
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	int32 DepositedBatteries = 0;
	UPROPERTY(BlueprintReadOnly)
	int32 RequiredBatteries = 7;
	UPROPERTY(BlueprintReadOnly)
	int32 TotalToys = 0;
	UPROPERTY(BlueprintReadOnly)
	int32 CapturedToys = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBatteryTagProgressChanged);

UCLASS()
class TOONSTORY_API ABatteryTagGameState : public AToonStoryGameState
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Battery Tag")
	FBatteryTagProgress GetProgress() const { return Progress; }
	UPROPERTY(BlueprintAssignable, Category = "Battery Tag")
	FOnBatteryTagProgressChanged OnProgressChanged;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	friend class ABatteryTagGameMode;
	void SetProgress(const FBatteryTagProgress& NewProgress, bool bNotify = true);
	UPROPERTY(ReplicatedUsing = OnRep_Progress)
	FBatteryTagProgress Progress;
	UFUNCTION()
	void OnRep_Progress();
};
