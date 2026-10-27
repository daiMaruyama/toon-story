// Copyright (c) 2026 Mamodou. All Rights Reserved.

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

bool TB::SupportsCapsule(const UWorld* World, FVector Position, float Radius, float HalfHeight,
    ECollisionChannel Channel, const FCollisionQueryParams& Params, float WalkableZ, float MaxDrop)
{
	if (!World || Position.ContainsNaN() || Radius <= 0 || HalfHeight < Radius) return false;
	const auto Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	if (World->OverlapBlockingTestByChannel(Position, FQuat::Identity, Channel, Shape, Params)) return false;
	FHitResult Floor;
	return World->SweepSingleByChannel(Floor, Position + FVector(0, 0, 1), Position - FVector(0, 0, MaxDrop),
	    FQuat::Identity, Channel, Shape, Params) && !Floor.bStartPenetrating && Floor.ImpactNormal.Z >= WalkableZ;
}
