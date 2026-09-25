#pragma once

#include "CoreMinimal.h"
#include "ToonMatchTypes.generated.h"

UENUM(BlueprintType)
enum class EToonMatchPhase : uint8
{
	Waiting,
	Playing,
	Finished,
	Countdown
};

/** One replicated snapshot keeps phase, deadline and result together. */
USTRUCT(BlueprintType)
struct FToonMatchStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EToonMatchPhase Phase = EToonMatchPhase::Waiting;

	UPROPERTY(BlueprintReadOnly)
	double EndServerTime = 0.0;

	/** NAME_None means no winner (for example, an aborted round). */
	UPROPERTY(BlueprintReadOnly)
	FName WinningTeam;

	/** Rule-defined identifier; the common foundation does not know battery rules. */
	UPROPERTY(BlueprintReadOnly)
	FName EndReason;

	UPROPERTY(BlueprintReadOnly)
	int32 ConnectedPlayers = 0;
	UPROPERTY(BlueprintReadOnly)
	int32 ReadyPlayers = 0;
	UPROPERTY(BlueprintReadOnly)
	int32 MinimumPlayers = 0;
};
