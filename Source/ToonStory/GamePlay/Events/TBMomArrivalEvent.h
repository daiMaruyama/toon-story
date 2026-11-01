#pragma once
#include "TBRandomEvent.h"
#include "TBMomArrivalEvent.generated.h"

/** 指定エリアがあるステージで、サーバーがママ到着イベントを開始する。 */
UCLASS()
class TOONSTORY_API UTBMomArrivalEvent : public UTBRandomEvent
{
	GENERATED_BODY()
public:
	virtual bool CanExecute_Implementation(UWorld* World) const override;
	virtual bool Execute_Implementation(UWorld* World) override;
};
