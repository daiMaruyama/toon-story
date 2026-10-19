#pragma once
#include "CoreMinimal.h"
class AActor;
class ATBCharacter;

struct FTBInteractionTarget
{
	AActor* Actor = nullptr;
	FName ActionId;
};

namespace TB
{
	// Server re-evaluates reach, visibility and state. Aim alignment wins, distance breaks ties.
	FTBInteractionTarget FindInteractionTarget(ATBCharacter* Character);
}
