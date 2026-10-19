#pragma once
#include "CoreMinimal.h"
#include "TBInteractionTypes.generated.h"

class ATBCharacter;
class AActor;

UENUM(BlueprintType)
enum class ETBInteractionTeams : uint8
{
	Both,
	HumanOnly,
	ToyOnly
};

/** Static action settings. Runtime state belongs to the item/component, not this definition. */
USTRUCT(BlueprintType)
struct TOONSTORY_API FTBItemAction
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FName Id = TEXT("Use");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	ETBInteractionTeams AllowedTeams = ETBInteractionTeams::Both;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "1"))
	float MaxDistance = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bRequiresLineOfSight = true;
};

/** Target is the receiver of the call. SourceItem is optional context, never proof of ownership. */
USTRUCT(BlueprintType)
struct TOONSTORY_API FTBInteractionContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<ATBCharacter> InstigatorCharacter = nullptr;
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<AActor> SourceItem = nullptr;
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FName ActionId = TEXT("Use");
};
