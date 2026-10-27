// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class ATBGameState;
class ATBCharacter;
class AActor;
class UWorld;
struct FCollisionQueryParams;

namespace TB
{
	ATBGameState* GS(const UWorld* World);
	bool Playing(const UWorld* World);
	bool ClearPath(const AActor* From, const AActor* To, const FVector& Start, const FVector& End);
	void Notice(ATBCharacter* ToyCharacter, const FString& Text);
	bool SupportsCapsule(const UWorld* World, FVector Position, float Radius, float HalfHeight,
	    ECollisionChannel Channel, const FCollisionQueryParams& Params, float WalkableZ = .71f, float MaxDrop = 12.f);
}
