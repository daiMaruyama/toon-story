#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Core/ToonMatchTypes.h"
#include "ToonStoryGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToonMatchStatusChanged);

UCLASS()
class TOONSTORY_API AToonStoryGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Match")
	FToonMatchStatus GetMatchStatus() const { return MatchStatus; }

	UFUNCTION(BlueprintPure, Category = "Match")
	double GetRemainingSeconds() const;

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnToonMatchStatusChanged OnMatchStatusChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend class AToonStoryGameMode;
	void SetMatchStatus(const FToonMatchStatus& NewStatus);

	UPROPERTY(ReplicatedUsing = OnRep_MatchStatus)
	FToonMatchStatus MatchStatus;

	UFUNCTION()
	void OnRep_MatchStatus();
};
