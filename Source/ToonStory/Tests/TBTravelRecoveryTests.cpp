#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBLobbySettings.h"
#include "Core/TBSession.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

namespace
{
UWorld* RecoveryWorld()
{
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) return Context.World();
	return nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTravelRecovery, "ToonStory.Review.TravelRecovery",
                                EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBTravelRecovery::RunTest(const FString&)
{
	auto* World = RecoveryWorld();
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
	auto* Pending = World->GetGameInstance()->GetSubsystem<UTBLobbySettings>();
	auto* Session = World->GetGameInstance()->GetSubsystem<UTBSession>();
	if (!Mode || !State || !PC || !Session) return false;
	const auto Rules = State->Settings;
	const auto Phase = State->Phase;
	const int32 Selection = State->SelectedStage;
	const auto StageName = State->StageName;
	ON_SCOPE_EXIT
	{
		TBLobbySettings::CancelTravel(World);
		Pending->bStageSelected = false;
		Pending->SelectedStage = Selection;
		State->SelectedStage = Selection;
		State->StageName = StageName;
		State->Settings = Rules;
		State->Phase = Phase;
	};
	State->Phase = ETBPhase::Lobby;
	Pending->bStageSelected = false;
	TestFalse(TEXT("Direct arena startup remains available for rule tests"), TBLobbySettings::ShouldTravelToStage(World));
	TBLobbySettings::SelectStage(PC, 2);
	TestTrue(TEXT("Explicit stage selection from Arena schedules selected stage at start"), TBLobbySettings::ShouldTravelToStage(World));
	TestEqual(TEXT("Selected stage item limit is used before travel"), TBLobbySettings::AvailableItems(World), 8);
	Pending->bStartOnArrival = true;
	Pending->PendingMap = TEXT("/Game/Maps/Loading");
	Pending->ArrivalDeadline = 0;
	State->bTravelPending = true;
	TBLobbySettings::TickArrival(Mode);
	TestEqual(TEXT("Loading does not consume arrival timeout"), Pending->ArrivalDeadline, 0.0);
	Pending->PendingMap.Reset();
	Pending->ExpectedPlayers = State->PlayerArray.Num() + 1;
	TBLobbySettings::TickArrival(Mode);
	TestTrue(TEXT("Arrival receives a fresh sixty seconds"), Pending->ArrivalDeadline > FPlatformTime::Seconds() + 59);
	GEngine->BroadcastTravelFailure(World, ETravelFailure::LoadMapFailure, TEXT("Review test missing stage"));
	TestFalse(TEXT("Host failure preserves session instead of closing"), Session->IsBusy());
	TestFalse(TEXT("Host failure unlocks lobby"), State->bTravelPending);
	TestFalse(TEXT("Host failure clears arrival start"), Pending->bStartOnArrival);
	TestTrue(TEXT("Host failure preserves the original reason"), Session->Status.Contains(TEXT("Review test missing stage")));
	const TWeakObjectPtr<UWorld> OriginalWorld(World);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, OriginalWorld]()
	{
		auto* Current = RecoveryWorld();
		TestTrue(TEXT("Host survives the frame after engine travel failure"), OriginalWorld.IsValid() && Current == OriginalWorld.Get());
		if (Current) TestTrue(TEXT("Host failure clears engine scheduled disconnect"),
		                     GEngine->GetWorldContextFromWorldChecked(Current).TravelURL.IsEmpty());
		return true;
	}));
	TestTrue(TEXT("Host remains in same world with its pawn"), World == PC->GetWorld() && PC->GetPawn());
	auto* Other = UGameplayStatics::CreatePlayer(World, -1, true);
	if (!TestNotNull(TEXT("Second player"), Other)) return false;
	Pending->bStartOnArrival = true;
	State->bTravelPending = true;
	UGameplayStatics::RemovePlayer(Other, true);
	TestTrue(TEXT("Disconnect during loading cancels arrival without waiting for timeout"), Pending->bArrivalCancelled);
	return true;
}
#endif
