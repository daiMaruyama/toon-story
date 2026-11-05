#pragma once
#include "CoreMinimal.h"
#include "GamePlay/Items/TBItemBase.h"
#include "TBDecoyToy.generated.h"

class UChildActorComponent;
class UTBDecoyToyData;

/** 見た目はプレイヤーと同じだが、視認によって止まらない配置アイテム。 */
UCLASS(Blueprintable)
class TOONSTORY_API ATBDecoyToy : public ATBItemBase
{
	GENERATED_BODY()
public:
	ATBDecoyToy();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ニセおもちゃ")
	TObjectPtr<UChildActorComponent> Visual;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ニセおもちゃ")
	TSoftObjectPtr<UTBDecoyToyData> DecoyData;
	UPROPERTY(ReplicatedUsing=OnRep_Visual, BlueprintReadOnly, Category="ニセおもちゃ")
	TSubclassOf<AActor> VisualClass;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="ニセおもちゃ")
	bool bWalking = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="ニセおもちゃ")
	bool bGrounded = false;
	const UTBDecoyToyData& Data() const;
protected:
	virtual bool CanPerformAction_Implementation(const FTBInteractionContext& Context) const override;
	virtual bool PerformAction_Implementation(const FTBInteractionContext& Context) override;
private:
	friend class FTBDecoyFootstepTest;
	UFUNCTION()
	void OnRep_Visual();
	UPROPERTY(Transient)
	TObjectPtr<UTBDecoyToyData> LoadedData;
	FVector PreviousLocation = FVector::ZeroVector;
	float StepDistance = 0.f;
	float FootstepSampleSeconds = 0.f;
	bool bHasSample = false;
	float VerticalSpeed = 0.f;
};
