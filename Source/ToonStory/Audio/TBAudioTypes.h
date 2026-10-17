#pragma once
#include "CoreMinimal.h"
#include "TBAudioTypes.generated.h"

UENUM(BlueprintType)
enum class ETBMusicState : uint8
{
	None, Title, Lobby, Match, Results
};

UENUM(BlueprintType)
enum class ETBSoundEvent : uint8
{
	UIClick, Ready, MatchStart, Victory, Defeat, Pickup, Capture, Store,
	RescueComplete, Frozen, Thawed, Footstep, Jump, Land, RescueLoop, StoreLoop,
	// Append new events to preserve existing serialized enum values.
	HumanFootstep
};

UENUM(BlueprintType)
enum class ETBAudioCategory : uint8
{
	Music, Effects, Voice
};
