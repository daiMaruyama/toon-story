#pragma once

#include "TBCharacter.h"
#include "TBToyCharacter.generated.h"

class UTBToyDefinition;
class UTBCharacterAppearanceComponent;
class UTBToyAnimationComponent;

/** Adds data-driven toys without changing the shared movement, carry or camera implementation. */
UCLASS()
class TOONSTORY_API ATBToyCharacter : public ATBCharacter
{
	GENERATED_BODY()
public:
	ATBToyCharacter(const FObjectInitializer& Init);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnRep_PlayerState() override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Toy")
	TSoftObjectPtr<UTBToyDefinition> ToyDefinition;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Toy")
	TArray<TSoftObjectPtr<UTBToyDefinition>> ToyCatalogue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Toy")
	TObjectPtr<UTBCharacterAppearanceComponent> Appearance;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Toy")
	TObjectPtr<UTBToyAnimationComponent> ToyAnimation;
	UPROPERTY(ReplicatedUsing = OnRep_SelectedToyIndex, BlueprintReadOnly, Category = "Toy")
	int32 SelectedToyIndex = INDEX_NONE;

	// Debug requests are also rejected by the server in Shipping builds.
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Debug", meta = (DevelopmentOnly))
	void ServerSelectToy(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "Debug", meta = (DevelopmentOnly))
	void NextToy();
	void RefreshToyPresentation();
	UTBToyDefinition* GetToyDefinition() const;

private:
	UFUNCTION()
	void OnRep_SelectedToyIndex();
	bool LoadToyDefinition();
	bool bPresentationReady = false;
};
