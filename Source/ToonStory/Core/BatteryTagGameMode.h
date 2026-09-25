#pragma once

#include "CoreMinimal.h"
#include "ToonStoryGameMode.h"
#include "BatteryTagGameMode.generated.h"

class APlayerState;

/** Server-side rule integration. These functions are NOT client RPCs. */
UCLASS()
class TOONSTORY_API ABatteryTagGameMode : public AToonStoryGameMode
{
	GENERATED_BODY()
public:
	ABatteryTagGameMode();

	/** Lobby supplies the authoritative Toy roster before starting; team assignment stays in PlayerState. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Battery Tag")
	bool RegisterToy(APlayerState* Player);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Battery Tag")
	bool UnregisterToy(APlayerState* Player);

	/** Call only after server-side possession/location/interaction checks. Reuse the battery's stable ID. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Battery Tag")
	bool TryRecordBatteryDeposit(APlayerState* Player, FGuid BatteryId);

	/** Capture/rescue system reports a validated transition, not an untrusted client claim. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Battery Tag")
	bool SetToyCaptured(APlayerState* Player, bool bCaptured);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Battery Tag", meta = (ClampMin = "1"))
	int32 RequiredBatteries = 7;

protected:
	virtual bool CanStartRound() const override;
	virtual void PrepareRound() override;
	virtual void HandleTimeExpired() override;
	virtual void Logout(AController* Exiting) override;
	virtual void RebuildLobbyRoles() override;

private:
	bool IsRosterValid() const;
	void PublishProgress(bool bNotify = true);
	bool bProcessingRuleEvent = false;
	TMap<TWeakObjectPtr<APlayerState>, bool> Toys;
	TSet<FGuid> DepositedBatteryIds;
	int32 RoundRequiredBatteries = 7;
};
