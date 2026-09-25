// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ToonStoryGameMode.generated.h"

/**
 * Shared round lifecycle. Rule-specific modes decide winners and validate gameplay events.
 */
UCLASS(abstract)
class TOONSTORY_API AToonStoryGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AToonStoryGameMode();

	/** Called by authoritative lobby/ready logic, not automatically by BeginPlay. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match")
	bool TryStartRound();

	/** Cancels without awarding a win. Disconnect policy belongs to the rule. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match")
	bool AbortRound();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match", meta = (ClampMin = "0.1"))
	float RoundDurationSeconds = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match|Loop")
	bool bEnableMatchLoop = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match|Loop", meta = (ClampMin = "1"))
	int32 MinimumPlayers = 3;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match|Loop", meta = (ClampMin = "0.1"))
	float StartCountdownSeconds = 3.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match|Loop", meta = (ClampMin = "0.1"))
	float ResultDisplaySeconds = 8.0f;

	/** Called through the requesting player's owned PlayerController RPC. */
	bool SetPlayerReady(APlayerController* Player, bool bReady);
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool CanStartRound() const;
	virtual void PrepareRound();
	virtual void HandleTimeExpired();
	bool FinishRound(FName WinningTeam, FName Reason);
	/** All gameplay events check the deadline, even if the timer has not ticked yet. */
	bool CanAcceptRoundEvent();
	class AToonStoryGameState* GetToonGameState() const;
	virtual void RebuildLobbyRoles();
	const TArray<TWeakObjectPtr<APlayerController>>& GetLobbyPlayers() const { return LobbyPlayers; }

private:
	bool bStartingRound = false;
	bool bUpdatingLobby = false;
	TArray<TWeakObjectPtr<APlayerController>> LobbyPlayers;
	void RefreshLobby();
	void CancelCountdown();
	void CompleteCountdown();
	void ReturnToLobby();
	bool IsLobbyReady() const;
	FTimerHandle LobbyCountdownTimer;
	FTimerHandle ResultTimer;
	void CheckRoundDeadline();
	FTimerHandle RoundDeadlineTimer;
};
