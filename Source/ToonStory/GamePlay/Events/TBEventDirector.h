#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBEventDirector.generated.h"

class UTBRandomEvent;

/** Driven by GameMode after win checks. Never rolls on clients or outside Playing. */
UCLASS()
class TOONSTORY_API UTBEventDirector : public UActorComponent
{
	GENERATED_BODY()
public:
	void Advance(float DeltaSeconds);
	void ResetRound();

	/** Explicit server triggers also share cooldown and eligibility checks. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Events")
	bool TryTrigger(TSubclassOf<UTBRandomEvent> EventClass);

private:
	void PrepareRound();
	bool CanRun() const;
	bool ExecuteEvent(UTBRandomEvent* Event);
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTBRandomEvent>> Events;
	double Elapsed = 0;
	double NextRollAt = 1;
	double CooldownUntil = 0;
	bool bPrepared = false;
	bool bExecuting = false;
};
