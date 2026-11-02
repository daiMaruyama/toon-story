// Copyright (c) 2026 Mamodou. All Rights Reserved.

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
	virtual void ReceivedPlayer() override;
	UFUNCTION(Exec) void TBHost();
	UFUNCTION(Exec) void TBFind();
	UFUNCTION(Exec) void TBJoin(int32 Index);
	UFUNCTION(Exec) void TBLeave();
	bool IsStoryPlaying() const;
	// カーソルの先にいるおもちゃのエモートを次へ。当たらなければ何もしない。
	bool ClickToy();
	// レイの先で、いちばん手前に見えているおもちゃ。
	class UTBTitleEmote* PickToy(const FVector& Origin, const FVector& Direction) const;
protected:
	virtual void SetupInputComponent() override;
private:
	void ClickToyPressed() { ClickToy(); }
	UPROPERTY(Transient) TObjectPtr<UTBTitleMenu> Menu;
};
