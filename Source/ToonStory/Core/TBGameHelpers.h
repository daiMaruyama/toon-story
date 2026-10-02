#pragma once
#include "CoreMinimal.h"

class ATBGameState;
class ATBCharacter;
class AActor;
class UWorld;

namespace TB
{
	ATBGameState* GS(const UWorld* World);
	bool Playing(const UWorld* World);
	bool ClearPath(const AActor* From, const AActor* To, const FVector& Start, const FVector& End);
	void Notice(ATBCharacter* ToyCharacter, const FString& Text);
}
