#include "TBLobbySettings.h"
#include "TBController.h"
#include "TBSession.h"
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

namespace
{
	struct FStageDefinition
	{
		const TCHAR* Path;
		const TCHAR* Name;
		int32 ItemCount;
	};
	// ロビーでの表示・上限。実際のアイテム数はステージ到着時に再確認する。
	constexpr FStageDefinition Stages[] = {{TEXT("/Game/Maps/TB_KidsRoom"), TEXT("Kids' Rooms"), 7},
	                                       {TEXT("/Game/Maps/TB_ToyWorldDemoExpanded"), TEXT("Demo ＋ こども部屋"), 10},
	                                       {TEXT("/Game/Maps/TB_ArchViz"), TEXT("ArchViz Apartment"), 8}};
	const FStageDefinition* FindStage(int32 Index)
	{
		return Index >= 0 && Index < UE_ARRAY_COUNT(Stages) ? &Stages[Index] : nullptr;
	}
} // namespace

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
				                                             bStartOnArrival = false;
				                                             if (auto* State = World->GetGameState<ATBGameState>())
				                                             {
					                                             State->bTravelPending = false;
				                                             }
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
			    bStartOnArrival = false;
			    bDedicatedLobby = false;
			    SelectedStage = 1;
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
	const auto* Stage = FindStage(Index);
	return Stage ? Stage->Path : nullptr;
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
	// Authored pickup counts for KidsRoom, Expanded and ArchViz. Revalidated on arrival
	// so a later map edit cannot produce an impossible collection target.
	if (IsLobby(World))
	{
		const auto* State = World->GetGameState<ATBGameState>();
		const auto* Stage = State ? FindStage(State->SelectedStage) : nullptr;
		return Stage ? Stage->ItemCount : 1;
	}
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
	if (!Mode || !Mode->IsHost(Host) || !State || State->Phase != ETBPhase::Lobby || State->bTravelPending || !Path ||
	    !Pending->PendingMap.IsEmpty())
	{
		return;
	}
	if (!FPackageName::DoesPackageExist(Path))
	{
		return;
	}
	Pending->SelectedStage = Index;
	State->SelectedStage = Index;
	State->Settings.RequiredItems = FMath::Min(State->Settings.RequiredItems, AvailableItems(World));
	State->StageName = FindStage(Index)->Name;
	State->ForceNetUpdate();
}
bool TBLobbySettings::IsLobby(const UWorld* World)
{
	return World && World->GetMapName().EndsWith(TEXT("TB_Lobby"));
}
bool TBLobbySettings::Travel(ATBGameMode* Mode, bool StartMatch)
{
	auto* World = Mode->GetWorld();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Pending = World->GetGameInstance()->GetSubsystem<UTBLobbySettings>();
	if (!State || State->bTravelPending || !Pending->PendingMap.IsEmpty())
	{
		return false;
	}
	const TCHAR* Destination = StartMatch ? StagePath(State->SelectedStage) : TEXT("/Game/Maps/TB_Lobby");
	if (!Destination)
	{
		return false;
	}
	const FString Path(Destination);
	if (!FPackageName::DoesPackageExist(Path))
	{
		return false;
	}
	Pending->PendingRules = State->Settings;
	Pending->SelectedStage = State->SelectedStage;
	Pending->ExpectedPlayers = State->PlayerArray.Num();
	Pending->PendingMap = Path;
	Pending->bStartOnArrival = StartMatch;
	Pending->ArrivalDeadline = FPlatformTime::Seconds() + 60;
	State->bTravelPending = true;
	Mode->bUseSeamlessTravel = true;
	if (!World->ServerTravel(Path))
	{
		State->bTravelPending = false;
		Pending->PendingMap.Reset();
		Pending->bStartOnArrival = false;
		return false;
	}
	if (auto* Session = World->GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->RefreshLobbyAvailability();
	}
	return true;
}
void TBLobbySettings::TickArrival(ATBGameMode* Mode)
{
	auto* World = Mode->GetWorld();
	auto* Pending = World->GetGameInstance()->GetSubsystem<UTBLobbySettings>();
	if (!Pending->bStartOnArrival || !Pending->PendingMap.IsEmpty() || World->IsInSeamlessTravel())
	{
		return;
	}
	auto* State = World->GetGameState<ATBGameState>();
	if (!State)
	{
		return;
	}
	bool Ready = State->PlayerArray.Num() == Pending->ExpectedPlayers;
	for (APlayerState* Player : State->PlayerArray)
	{
		auto* Info = Cast<ATBPlayerState>(Player);
		auto* Controller = Info ? Cast<APlayerController>(Info->GetOwner()) : nullptr;
		Ready &= Info && Info->bReady && Info->GetPawn() && Controller && Controller->HasClientLoadedCurrentWorld();
	}
	if (!Ready && FPlatformTime::Seconds() < Pending->ArrivalDeadline)
	{
		return;
	}
	Pending->bStartOnArrival = false;
	State->bTravelPending = false;
	if (Ready)
	{
		for (auto It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (Mode->IsHost(It->Get()))
			{
				Mode->StartRound(It->Get());
				break;
			}
		}
	}
	if (State->Phase != ETBPhase::Playing)
	{
		Travel(Mode, false);
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
	auto* Pending = World->GetGameInstance()->GetSubsystem<UTBLobbySettings>();
	if (IsLobby(World))
	{
		Pending->bDedicatedLobby = true;
	}
	const int32 CurrentStage = StageIndex(World);
	if (!Pending->bDedicatedLobby && CurrentStage != INDEX_NONE)
	{
		Pending->SelectedStage = CurrentStage;
	}
	State->SelectedStage = Pending->SelectedStage;
	// Arenaなどの直接起動時は、GameModeが設定したマップ名を維持する。
	if (Pending->bDedicatedLobby || CurrentStage != INDEX_NONE)
	{
		if (const auto* Stage = FindStage(State->SelectedStage))
		{
			State->StageName = Stage->Name;
		}
	}
	if (!Pending->PendingMap.IsEmpty() && World->GetMapName().EndsWith(FPackageName::GetShortName(Pending->PendingMap)))
	{
		State->Settings = Pending->PendingRules;
		State->Settings.RequiredItems =
		    FMath::Clamp(State->Settings.RequiredItems, 1, FMath::Max(1, AvailableItems(World)));
		Pending->PendingMap.Reset();
		State->bTravelPending = Pending->bStartOnArrival;
		State->ForceNetUpdate();
		if (auto* Session = World->GetGameInstance()->GetSubsystem<UTBSession>())
		{
			Session->RefreshLobbyAvailability();
		}
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
