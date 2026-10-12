#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "TBTitleGameMode.generated.h"

class UTBTitleMenu;

/** Offline entry point. No match state, pawn or stage actors. */
UCLASS()
class TOONSTORY_API ATBTitleGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ATBTitleGameMode();
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override { return nullptr; }
};

UCLASS()
class TOONSTORY_API ATBTitleController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(Exec) void TBHost();
	UFUNCTION(Exec) void TBFind();
	UFUNCTION(Exec) void TBJoin(int32 Index);
	UFUNCTION(Exec) void TBLeave();
private:
	UPROPERTY(Transient) TObjectPtr<UTBTitleMenu> Menu;
};
