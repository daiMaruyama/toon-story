#pragma once

#include "CoreMinimal.h"
#include "ToonStoryCharacter.h"
#include "HumanCharacter.generated.h"

/** Player character for the human role. */
UCLASS()
class TOONSTORY_API AHumanCharacter : public AToonStoryCharacter
{
	GENERATED_BODY()

public:
	AHumanCharacter();
};
