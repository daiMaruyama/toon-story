#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Data/TBTypes.h"
#include "TBGameState.generated.h"

class ATBBox;

/** 試合設定・残り時間・勝敗など、全員が参照する情報を持つ。 */
UCLASS()
class TOONSTORY_API ATBGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadOnly)
	FString StageName;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 PlayerLimit = 0;
	UPROPERTY(Replicated, BlueprintReadOnly)
	FTBSettings Settings;
	UPROPERTY(Replicated, BlueprintReadOnly)
	ETBPhase Phase = ETBPhase::Lobby;
	UPROPERTY(Replicated, BlueprintReadOnly)
	ETBWinner Winner = ETBWinner::None;
	UPROPERTY(Replicated, BlueprintReadOnly)
	FString Reason;
	UPROPERTY(Replicated, BlueprintReadOnly)
	double EndTime = 0;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 Collected = 0;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 BoxedCount = 0;
	UPROPERTY(Replicated, BlueprintReadOnly)
	TObjectPtr<ATBBox> Box;
	UFUNCTION(BlueprintPure)
	float Remaining() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
