#pragma once

#include "CoreMinimal.h"
#include "GamePlay/Items/TBItemBase.h"
#include "TBLightSwitch.generated.h"

class ATBRoomLighting;

/** Place within reach of both teams; E toggles the linked room through the character's server RPC. */
UCLASS()
class TOONSTORY_API ATBLightSwitch : public ATBItemBase
{
	GENERATED_BODY()
public:
	ATBLightSwitch();
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Switch")
	TObjectPtr<ATBRoomLighting> Room;
protected:
	virtual bool CanPerformAction_Implementation(const FTBInteractionContext& Context) const override;
	virtual bool PerformAction_Implementation(const FTBInteractionContext& Context) override;
};
