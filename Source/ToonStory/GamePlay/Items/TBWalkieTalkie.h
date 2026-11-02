#pragma once
#include "CoreMinimal.h"
#include "GamePlay/Items/TBItemBase.h"
#include "TBWalkieTalkie.generated.h"

class UTBWalkieTalkieData;
class ATBCharacter;

/** サーバーが2台を組にして送信者を管理する。 */
UCLASS(Blueprintable)
class TOONSTORY_API ATBWalkieTalkie : public ATBItemBase
{
	GENERATED_BODY()
public:
	ATBWalkieTalkie();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	bool PairWith(ATBWalkieTalkie* Other);
	void StopTransmitting(ATBCharacter* Character);
	const UTBWalkieTalkieData& Data() const;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="トランシーバー")
	TObjectPtr<ATBWalkieTalkie> Partner;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="トランシーバー")
	TObjectPtr<ATBCharacter> Transmitter;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="トランシーバー")
	TSoftObjectPtr<UTBWalkieTalkieData> WalkieData;
protected:
	virtual bool CanPerformAction_Implementation(const FTBInteractionContext& Context) const override;
	virtual bool PerformAction_Implementation(const FTBInteractionContext& Context) override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UTBWalkieTalkieData> LoadedData;
};
