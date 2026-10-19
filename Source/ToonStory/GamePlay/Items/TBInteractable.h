#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GamePlay/Items/TBInteractionTypes.h"
#include "TBInteractable.generated.h"

// Native implementations keep the validated entry point; item hooks remain Blueprint extensible.
UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class TOONSTORY_API UTBInteractable : public UInterface
{
	GENERATED_BODY()
};

class TOONSTORY_API ITBInteractable
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual TArray<FTBItemAction> GetInteractionActions() const = 0;
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual FVector GetInteractionLocation() const = 0;
	// A local query is only a UI hint; TryInteract revalidates on the server.
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual bool CanInteract(const FTBInteractionContext& Context) const = 0;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Interaction")
	virtual bool TryInteract(const FTBInteractionContext& Context) = 0;
};
