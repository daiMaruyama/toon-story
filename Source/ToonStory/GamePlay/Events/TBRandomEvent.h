#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TBRandomEvent.generated.h"

/** Server-only, instantaneous event. A fresh instance is created each round.
 * Persistent effects must be owned/replicated/reset by their gameplay actors. */
UCLASS(Abstract, Blueprintable, BlueprintType, meta = (ShowWorldContextPin))
class TOONSTORY_API UTBRandomEvent : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category = "Event")
	bool CanExecute(UWorld* World) const;
	virtual bool CanExecute_Implementation(UWorld* World) const { return false; }

	/** Return true only when an effect was applied. False does not consume cooldown. */
	UFUNCTION(BlueprintNativeEvent, Category = "Event")
	bool Execute(UWorld* World);
	virtual bool Execute_Implementation(UWorld* World) { return false; }
};
