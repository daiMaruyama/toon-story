#pragma once

#include "TBCharacter.h"
#include "TBToyCharacter.generated.h"

class UChildActorComponent;

// おもちゃの選択結果を同期する。モデル・材質・アニメーションはBlueprint側で管理。
UCLASS()
class TOONSTORY_API ATBToyCharacter : public ATBCharacter
{
	GENERATED_BODY()
public:
	ATBToyCharacter(const FObjectInitializer& Init);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 表示専用BPの候補。ゲーム処理や独自の通信同期は持たせない。
	UPROPERTY(EditDefaultsOnly, Category = "Toy")
	TArray<TSubclassOf<AActor>> ToyCatalogue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Toy")
	TObjectPtr<UChildActorComponent> ToyVisual;
	UPROPERTY(ReplicatedUsing = RefreshToyPresentation, BlueprintReadOnly, Category = "Toy")
	int32 SelectedToyIndex = INDEX_NONE;

	// チーム確定後、プレイ開始前にサーバーから呼ぶ。
	void ChooseRandomToy();
	UFUNCTION()
	void RefreshToyPresentation();

private:
	bool bShowingToy = false;
};
