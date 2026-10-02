#include "TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Character/TBCharacter.h"
#include "Engine/World.h"

namespace TB
{
	ATBGameState* GS(const UWorld* World)
	{
		return World ? World->GetGameState<ATBGameState>() : nullptr;
	}

	bool Playing(const UWorld* World)
	{
		const auto* MatchInfo = GS(World);
		return MatchInfo && MatchInfo->Phase == ETBPhase::Playing;
	}

	// 判定元と判定先自身を除外し、間に壁などがあるか調べる。
	bool ClearPath(const AActor* From, const AActor* To, const FVector& Start, const FVector& End)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TBInteract), false, From);
		QueryParams.AddIgnoredActor(To);
		return !From->GetWorld()->LineTraceTestByChannel(Start, End, ECC_Visibility, QueryParams);
	}

	void Notice(ATBCharacter* ToyCharacter, const FString& Text)
	{
		if (auto* PlayerController = Cast<ATBController>(ToyCharacter->GetController()))
		{
			PlayerController->ClientNotice(Text);
		}
	}
}
