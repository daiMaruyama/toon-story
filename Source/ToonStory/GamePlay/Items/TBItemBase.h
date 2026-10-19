#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GamePlay/Items/TBInteractable.h"
#include "TBItemBase.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** Placeable foundation. Carrying and item-specific effects belong to derived actors/components. */
UCLASS(Blueprintable)
class TOONSTORY_API ATBItemBase : public AActor, public ITBInteractable
{
	GENERATED_BODY()
public:
	ATBItemBase();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<USceneComponent> InteractionPoint;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;
	// Unique, non-empty IDs. Duplicate IDs are rejected rather than choosing an arbitrary action.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	TArray<FTBItemAction> Actions;

	virtual TArray<FTBItemAction> GetInteractionActions() const override;
	virtual FVector GetInteractionLocation() const override;
	virtual bool CanInteract(const FTBInteractionContext& Context) const override final;
	virtual bool TryInteract(const FTBInteractionContext& Context) override final;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Side-effect-free additional conditions. Common validation cannot be bypassed by this hook.
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	bool CanPerformAction(const FTBInteractionContext& Context) const;
	// Server only, synchronous. Return true only when an effect actually succeeded.
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "Interaction")
	bool PerformAction(const FTBInteractionContext& Context);
	// Local visual refresh on the server and on clients receiving enabled-state replication.
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void InteractionEnabledChanged();

private:
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_InteractionEnabled, Category = "Interaction",
		meta = (AllowPrivateAccess = "true"))
	bool bInteractionEnabled = true;
	UFUNCTION()
	void OnRep_InteractionEnabled();
	// Prevent recursive effects from re-entering this item while it is executing.
	bool bExecutingAction = false;
};
