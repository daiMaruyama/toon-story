#pragma once
#include "TBRandomEvent.h"
#include "TBBlackoutEvent.generated.h"

/** Turns off one randomly selected lit room. Players restore it using its switch. */
UCLASS()
class TOONSTORY_API UTBBlackoutEvent : public UTBRandomEvent
{
	GENERATED_BODY()
public:
	virtual bool CanExecute_Implementation(UWorld* World) const override;
	virtual bool Execute_Implementation(UWorld* World) override;
};
