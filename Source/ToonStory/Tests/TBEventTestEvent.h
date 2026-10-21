#pragma once
#include "GamePlay/Events/TBRandomEvent.h"
#include "GamePlay/Events/TBEventDirector.h"
#include "TBEventTestEvent.generated.h"

UCLASS(Transient)
class UTBEventTestEvent : public UTBRandomEvent
{
	GENERATED_BODY()
public:
	inline static int32 Executions = 0;
	inline static bool bEligible = true;
	inline static bool bSucceed = true;
	inline static UTBEventDirector* ReentrantDirector = nullptr;
	inline static bool bNestedSucceeded = false;
	virtual bool CanExecute_Implementation(UWorld* World) const override { return bEligible; }
	virtual bool Execute_Implementation(UWorld* World) override
	{
		++Executions;
		if (ReentrantDirector)
			bNestedSucceeded = ReentrantDirector->TryTrigger(StaticClass());
		return bSucceed;
	}
};
