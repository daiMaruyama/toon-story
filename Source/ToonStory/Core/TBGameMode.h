// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/TBTypes.h"
#include "TBGameMode.generated.h"

class ATBCharacter;
class ATBBox;
class UTBEventDirector;
class UTBItemSpawnDirector;
class ATBHumanEntrance;

/** サーバー専用の試合管理。開始条件・視線凍結・勝敗を判断する。 */
UCLASS()
class TOONSTORY_API ATBGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ATBGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
	                      FString& Error) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	bool PlaceInLobby(AController* Player);
	void SyncLobbyCounts();
	virtual void PostSeamlessTravel() override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	UPROPERTY(EditDefaultsOnly)
	bool bBuildTestArena = true;
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<ATBBox> BoxClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bAllowInterference = false;
	bool IsHost(const APlayerController* PlayerController) const;
	void SetRules(APlayerController* PlayerController, const FTBSettings& Rules);
	void StartRound(APlayerController* PlayerController);
	void ReturnToLobby(APlayerController* PlayerController);
	void CheckWin();
	void Finish(ETBWinner Winner, const FString& Why, bool bAbort = false);
	bool Watched(ATBCharacter* Toy, ATBCharacter* Human) const;
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnPoints;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTBRoundStartTest;
	friend class FTBMirrorGazeTest;
	friend class FTBHidePreparationTest;
	friend class FTBHumanEntranceTest;
	friend class FTBBoxEscapeTest;
	friend class FTBRematchTest;
	friend class FTBStorageHoldTest;
	friend class FTBAuthoredLightingTest;
	friend class FTBLobbyVisibleTest;
	friend class FTBNPCDummyTest;
	friend class FTBNPCFieldTransportTest;
	friend class FTBNPCMatchTest;
	friend class FTBNPCSoloStartTest;
	friend class FTBLobbyPreferenceTest;
	void AdvancePreparationClockForTest(double SecondsRemaining);
	int32 TestFailNPCAfter = INDEX_NONE;
	int32 TestSuccessfulNPCs = 0;
	bool bTestNPCFailureReached = false;
	bool bTestFailRollback = false;
#endif
	bool bStartingRound = false;
	uint32 PreparationRound = 0;
	uint32 PreparationViewAttempt = 0;
	bool bObservedPreparationArrival = false;
	TWeakObjectPtr<ATBHumanEntrance> RoundEntrance;
	int32 EntranceCursor = 0;
	bool AdvanceHidePreparation();
	void FailRoundStart(const FString& Reason, bool bRecovered);
	UPROPERTY(VisibleAnywhere, Category = "Events")
	TObjectPtr<UTBEventDirector> EventDirector;
	UPROPERTY(VisibleAnywhere, Category="Items")
	TObjectPtr<UTBItemSpawnDirector> ItemSpawns;
	int32 LobbyPlayerCount() const;
	void DestroyRoundCharacters();
	void ResetStageInteractions();
	void BuildArena();
	void EvaluateGaze();
	bool SpawnNPCs();
	void RemoveNPCs();
	int32 SpawnCursor = 0;
};
