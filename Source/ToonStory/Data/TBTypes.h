#pragma once
#include "CoreMinimal.h"
#include "TBTypes.generated.h"
#include <sys/param.h>

UENUM(BlueprintType)
enum class ETBTeam : uint8
{
	None,
	Human,
	Toy
};
UENUM(BlueprintType)
enum class ETBToyState : uint8
{
	Free,
	Grabbed,
	Carried,
	Storing,
	Boxed
};
UENUM(BlueprintType)
enum class ETBPhase : uint8
{
	Lobby,
	Playing,
	Results,
	Aborted
};
UENUM(BlueprintType)
enum class ETBWinner : uint8
{
	None,
	Humans,
	Toys
};
UENUM(BlueprintType)
enum class ETBRemoteKey : uint8
{
	Digit,
	Next,
	Prev
};

USTRUCT(BlueprintType)
struct FTBSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Humans = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Toys = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredItems = 5;
};
