// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class ATBGameState;
class ATBCharacter;
class AActor;
class UCameraComponent;
class UWorld;

namespace TB
{
	ATBGameState* GS(const UWorld* World);
	bool Playing(const UWorld* World);
	bool ClearPath(const AActor* From, const AActor* To, const FVector& Start, const FVector& End);
	void Notice(ATBCharacter* ToyCharacter, const FString& Text);
	// 見えている範囲と視界判定を揃えるため、ゲームのカメラはすべてルールの画面比率に固定する。
	void ConstrainToRuleAspect(UCameraComponent* Camera);
}
