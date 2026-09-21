#pragma once

#include "CoreMinimal.h"
#include "ToonStoryCharacter.h"
#include "ToyCharacter.generated.h"

/** Player character for the toy role. */
UCLASS()
class TOONSTORY_API AToyCharacter : public AToonStoryCharacter
{
	GENERATED_BODY()

public:
	AToyCharacter();
};
