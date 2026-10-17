#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/TBTypes.h"
#include "TBLobbySettings.generated.h"
class ATBController;
class ATBGameMode;

// Server-only handoff: rules survive a lobby map change without changing the session.
UCLASS()
class TOONSTORY_API UTBLobbySettings : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	FTBSettings PendingRules;
	FString PendingMap;

private:
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle MapLoadedHandle;
};
namespace TBLobbySettings
{
	const TCHAR* StagePath(int32 Index);
	int32 StageIndex(const UWorld* World);
	int32 AvailableItems(UWorld* World);
	void SelectStage(ATBController* Host, int32 Index);
	void Restore(ATBGameMode* Mode);
	bool HandleClick(ATBController* Controller, FName Name);
} // namespace TBLobbySettings
