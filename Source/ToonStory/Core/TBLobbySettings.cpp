#include "TBLobbySettings.h"
#include "TBController.h"
#include "TBGameMode.h"
#include "TBGameState.h"
#include "TBPlayerState.h"
#include "GamePlay/TBPickup.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/PackageName.h"

void UTBLobbySettings::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		TravelFailureHandle =
		    GEngine->OnTravelFailure().AddWeakLambda(this,
		                                             [this](UWorld* World, ETravelFailure::Type, const FString&)
		                                             {
			                                             if (World && World->GetGameInstance() == GetGameInstance())
			                                             {
				                                             PendingMap.Reset();
			                                             }
		                                             });
	}
	MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddWeakLambda(
	    this,
	    [this](UWorld* World)
	    {
		    if (World && World->GetGameInstance() == GetGameInstance() && World->GetMapName().Contains(TEXT("Title")))
		    {
			    PendingMap.Reset();
		    }
	    });
}
void UTBLobbySettings::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
	Super::Deinitialize();
}

const TCHAR* TBLobbySettings::StagePath(int32 Index)
{
	return Index == 0   ? TEXT("/Game/Maps/TB_KidsRoom")
	       : Index == 1 ? TEXT("/Game/Maps/TB_ToyWorldDemoExpanded")
	       : Index == 2 ? TEXT("/Game/Maps/TB_ArchViz")
	                    : nullptr;
}
int32 TBLobbySettings::StageIndex(const UWorld* World)
{
	for (int32 Index = 0; StagePath(Index); ++Index)
	{
		if (World && World->GetMapName().EndsWith(FPackageName::GetShortName(StagePath(Index))))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
int32 TBLobbySettings::AvailableItems(UWorld* World)
{
	int32 Count = 0;
	for (TActorIterator<ATBPickup> It(World); It; ++It)
	{
		if (!It->bTaken)
		{
			++Count;
		}
	}
	return Count;
}
void TBLobbySettings::SelectStage(ATBController* Host, int32 Index)
{
	if (!Host)
	{
		return;
	}
	auto* World = Host->GetWorld();
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	const auto* Path = StagePath(Index);
	auto* Pending = World->GetGameInstance()->GetSubsystem<UTBLobbySettings>();
	if (!Mode || !Mode->IsHost(Host) || !State || State->Phase != ETBPhase::Lobby || !Path ||
	    !Pending->PendingMap.IsEmpty())
	{
		return;
	}
	if (World->GetMapName().EndsWith(FPackageName::GetShortName(Path)))
	{
		return;
	}
	if (!FPackageName::DoesPackageExist(Path))
	{
		Host->ClientNotice(TEXT("ステージが見つかりません。"));
		return;
	}
	Pending->PendingRules = State->Settings;
	// Restore clamps against the destination's saved pickups, including later editor changes.
	Pending->PendingMap = Path;
	Mode->bUseSeamlessTravel = true;
	if (!World->ServerTravel(Path))
	{
		Pending->PendingMap.Reset();
		Host->ClientNotice(TEXT("ステージを切り替えられませんでした。"));
	}
}
void TBLobbySettings::Restore(ATBGameMode* Mode)
{
	auto* World = Mode->GetWorld();
	auto* State = World->GetGameState<ATBGameState>();
	if (!State)
	{
		return;
	}
	if (World->GetMapName().Contains(TEXT("DemoExpanded")))
	{
		State->StageName = TEXT("Demo ＋ こども部屋");
	}
	auto* Pending = World->GetGameInstance()->GetSubsystem<UTBLobbySettings>();
	if (!Pending->PendingMap.IsEmpty() && World->GetMapName().EndsWith(FPackageName::GetShortName(Pending->PendingMap)))
	{
		State->Settings = Pending->PendingRules;
		State->Settings.RequiredItems =
		    FMath::Clamp(State->Settings.RequiredItems, 1, FMath::Max(1, AvailableItems(World)));
		Pending->PendingMap.Reset();
		State->ForceNetUpdate();
	}
}
bool TBLobbySettings::HandleClick(ATBController* Controller, FName Name)
{
	auto* State = Controller->GetWorld()->GetGameState<ATBGameState>();
	if (!State || State->Phase != ETBPhase::Lobby)
	{
		return false;
	}
	auto Rules = State->Settings;
	if (Name == TEXT("LessTime"))
	{
		Rules.Duration = FMath::Max(60.f, Rules.Duration - 60.f);
	}
	else if (Name == TEXT("MoreTime"))
	{
		Rules.Duration = FMath::Min(3600.f, Rules.Duration + 60.f);
	}
	else if (Name == TEXT("FewerBatteries"))
	{
		Rules.RequiredItems = FMath::Max(1, Rules.RequiredItems - 1);
	}
	else if (Name == TEXT("MoreBatteries"))
	{
		Rules.RequiredItems = FMath::Min(AvailableItems(Controller->GetWorld()), Rules.RequiredItems + 1);
	}
	else
	{
		return false;
	}
	Controller->ServerRules(Rules);
	return true;
}
void ATBController::ServerSelectStage_Implementation(int32 Index)
{
	TBLobbySettings::SelectStage(this, Index);
}
void ATBController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	LobbyCamera.Reset();
	bLobbyCameraSearched = false;
	bLobbyCameraActive = false;
}
