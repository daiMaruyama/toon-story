#pragma once
#include "GamePlay/Items/TBItemBase.h"
#include "TBItemTestActor.generated.h"

// Native success fixture: rejected requests must never reach PerformAction.
UCLASS(NotPlaceable, Transient)
class ATBItemTestActor : public ATBItemBase
{
	GENERATED_BODY()
public:
	int32 Executions = 0;
	int32 VisualUpdates = 0;
	bool bLastVisualEnabled = true;
protected:
	virtual bool PerformAction_Implementation(const FTBInteractionContext& Context) override
	{
		++Executions;
		return true;
	}
	virtual void InteractionEnabledChanged_Implementation() override
	{
		++VisualUpdates;
		bLastVisualEnabled = IsInteractionEnabled();
	}
};
