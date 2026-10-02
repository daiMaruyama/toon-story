#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/TBTypes.h"
#include "TBGameMode.generated.h"

class ATBCharacter;
class ATBBox;

/** サーバー専用の試合管理。開始条件・視線凍結・勝敗を判断する。 */
UCLASS()
class TOONSTORY_API ATBGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ATBGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
	                      FString& Error) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	UPROPERTY(EditDefaultsOnly)
	bool bBuildTestArena = true;
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<ATBBox> BoxClass;
	UPROPERTY(EditDefaultsOnly)
	float GazeHalfAngle = 40.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bAllowInterference = false;
	bool IsHost(const APlayerController* PlayerController) const;
	void SetRules(APlayerController* PlayerController, const FTBSettings& Rules);
	void StartRound(APlayerController* PlayerController);
	void CheckWin();
	void Finish(ETBWinner Winner, const FString& Why, bool bAbort = false);
	bool Watched(ATBCharacter* Toy, ATBCharacter* Human) const;
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnPoints;

private:
	void BuildArena();
	void EvaluateGaze();
	int32 SpawnCursor = 0;
};
