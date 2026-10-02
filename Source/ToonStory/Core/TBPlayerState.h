#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Data/TBTypes.h"
#include "TBPlayerState.generated.h"

class ATBPlayerState;

/** プレイヤーごとの陣営・Ready・捕獲状態を全員へ同期する。 */
UCLASS()
class TOONSTORY_API ATBPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly)
	ETBTeam Team = ETBTeam::None;
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly)
	ETBToyState ToyState = ETBToyState::Free;
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly)
	bool bFrozen = false;
	UPROPERTY(Replicated, BlueprintReadOnly)
	bool bReady = false;
	UPROPERTY(Replicated, BlueprintReadOnly)
	ETBTeam Preference = ETBTeam::None;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 Items = 0;
	UFUNCTION()
	void OnRep_State();
	virtual void CopyProperties(APlayerState* NewState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
