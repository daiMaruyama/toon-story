#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBItemSpawnDirector.generated.h"

class ATBItemBase;
class ATBItemSpawnPoint;
struct FTBItemSpawnRules;
struct FTBItemSpawnEntry;

/** 試合開始時の配置とロビー復帰時の片付け。手置きのアイテムは管理しない。 */
UCLASS()
class TOONSTORY_API UTBItemSpawnDirector : public UActorComponent
{
	GENERATED_BODY()
public:
	void StartRound();
	void ResetRound();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	int32 GetSpawnedCount() const;
private:
	bool HasFloorAt(const FVector& Location, const FTBItemSpawnRules& Rules, const AActor* Ignore) const;
	bool CanSpawnAt(const ATBItemSpawnPoint& Point, const FTBItemSpawnRules& Rules) const;
	bool FitOnFloor(ATBItemBase& Item, double ProbeHeight, const FTBItemSpawnRules& Rules) const;
	ATBItemBase* TrySpawn(const ATBItemSpawnPoint& Point, const FTBItemSpawnEntry& Entry, const FTBItemSpawnRules& Rules);
	TArray<TWeakObjectPtr<ATBItemBase>> SpawnedItems;
	bool bStarted = false;
	bool bSpawning = false;
	bool bResetRequested = false;
};
