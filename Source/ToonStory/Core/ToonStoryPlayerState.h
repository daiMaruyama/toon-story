#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ToonStoryPlayerState.generated.h"

UENUM(BlueprintType)
enum class EToonTeam : uint8 { Unassigned, Human, Toy };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnToonPlayerLobbyChanged);

/** Minimal lobby identity. Character/body state still belongs to the character. */
UCLASS()
class TOONSTORY_API AToonStoryPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsMatchReady() const { return bMatchReady; }
	UFUNCTION(BlueprintPure, Category = "Match")
	EToonTeam GetTeam() const { return Team; }
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnToonPlayerLobbyChanged OnLobbyChanged;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	friend class AToonStoryGameMode;
	friend class ABatteryTagGameMode;
	void SetMatchReady(bool bReady);
	void SetTeam(EToonTeam NewTeam);
	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	bool bMatchReady = false;
	UPROPERTY(ReplicatedUsing = OnRep_Lobby)
	EToonTeam Team = EToonTeam::Unassigned;
	UFUNCTION()
	void OnRep_Lobby();
};
