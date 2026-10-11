#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Data/TBTypes.h"
#include "TBTVButton.generated.h"

/** リモコンのボタン1個。おもちゃが踏むと、持ち主のリモコンが種類に応じてテレビを操作する。 */
UCLASS(ClassGroup = (ToonStory), meta = (BlueprintSpawnableComponent))
class TOONSTORY_API UTBTVButton : public UBoxComponent
{
	GENERATED_BODY()
public:
	UTBTVButton();

	UPROPERTY(EditAnywhere, Category = "Remote")
	ETBRemoteKey CurrentKey = ETBRemoteKey::Digit;
	UPROPERTY(EditAnywhere, Category = "Remote",
	          meta = (ClampMin = "0", ClampMax = "9", EditCondition = "CurrentKey == ETBRemoteKey::Digit",
	                  EditConditionHides))
	int32 Digit = 0;
};
