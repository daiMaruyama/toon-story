#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/TBTypes.h"
#include "TBLobbySettings.generated.h"
class ATBController;
class ATBGameMode;

// Server-only handoff between the dedicated lobby and a selected match map.
UCLASS()
class TOONSTORY_API UTBLobbySettings : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	FTBSettings PendingRules;
	FString PendingMap;
	int32 SelectedStage = 1;
	int32 ExpectedPlayers = 0;
	bool bDedicatedLobby = false;
	bool bStartOnArrival = false;
	double ArrivalDeadline = 0;

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
	bool IsLobby(const UWorld* World);
	bool Travel(ATBGameMode* Mode, bool StartMatch);
	void TickArrival(ATBGameMode* Mode);
	bool HandleClick(ATBController* Controller, FName Name);
} // namespace TBLobbySettings
