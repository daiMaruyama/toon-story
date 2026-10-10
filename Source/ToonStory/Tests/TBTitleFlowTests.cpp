#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBTitleGameMode.h"
#include "Core/TBSession.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBController.h"
#include "UI/TBTitleMenu.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

namespace
{
// Real map travel + a real Null session. A second local player exercises the
// existing match/rematch logic; separate-machine transport is tested separately.
class FTitleFlow : public IAutomationLatentCommand
{
public:
	explicit FTitleFlow(FAutomationTestBase* InTest, UGameInstance* Instance)
	    : Test(InTest), GI(Instance), Deadline(FPlatformTime::Seconds() + 180) {}
	virtual bool Update() override
	{
		if (!GI.IsValid()) { Test->AddError(TEXT("Game instance lost during travel")); return true; }
		if (FPlatformTime::Seconds() > Deadline) { Test->AddError(FString::Printf(TEXT("Title flow timed out at step %d"), Step)); return true; }
		UWorld* World = GI->GetWorld();
		if (!World || !World->HasBegunPlay()) return false;
		auto* Session = GI->GetSubsystem<UTBSession>();
		if (Step == 0)
		{
			CheckTitle(World);
			Session->Host();
			Step = 1;
		}
		else if (Step == 1)
		{
			auto* Mode = World->GetAuthGameMode<ATBGameMode>();
			if (!Mode || Session->IsBusy()) return false;
			Test->TestTrue(TEXT("Hosted session survives Title -> stage"), Session->HasSession());
			auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
			auto* Second = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
			if (!Host || !Second) { Test->AddError(TEXT("Two controllers required")); return true; }
			FTBSettings Rules;
			Rules.Humans = 1; Rules.Toys = 1; Rules.Duration = 10; Rules.RequiredItems = 5;
			Mode->SetRules(Host, Rules);
			for (auto* PC : {Host, Second}) PC->GetPlayerState<ATBPlayerState>()->bReady = true;
			Mode->StartRound(Host);
			auto* State = World->GetGameState<ATBGameState>();
			Test->TestEqual(TEXT("Starts playing"), State->Phase, ETBPhase::Playing);
			Mode->Finish(ETBWinner::Humans, TEXT("Title flow regression"));
			Test->TestEqual(TEXT("Shows results"), State->Phase, ETBPhase::Results);
			Mode->ReturnToLobby(Host);
			Test->TestEqual(TEXT("Rematch returns to lobby"), State->Phase, ETBPhase::Lobby);
			Test->TestTrue(TEXT("Rematch retains world and session"), GI->GetWorld() == World && Session->HasSession());
			Test->TestFalse(TEXT("Ready reset"), Host->GetPlayerState<ATBPlayerState>()->bReady);
			UGameplayStatics::RemovePlayer(Second, true);
			Session->Leave();
			Step = 2;
		}
		else if (Step == 2)
		{
			if (!World->GetAuthGameMode<ATBTitleGameMode>() || Session->IsBusy()) return false;
			CheckTitle(World);
			Test->TestFalse(TEXT("Leaving destroys session"), Session->HasSession());
			Session->Host(); // Must be possible to host again after a complete round trip.
			Step = 3;
		}
		else if (Step == 3)
		{
			if (!World->GetAuthGameMode<ATBGameMode>() || Session->IsBusy()) return false;
			GEngine->OnTravelFailure().Broadcast(World, ETravelFailure::InvalidURL, TEXT("Title flow simulated travel failure"));
			Step = 4;
		}
		else if (Step == 4)
		{
			if (!World->GetAuthGameMode<ATBTitleGameMode>() || Session->IsBusy()) return false;
			Test->TestFalse(TEXT("Travel failure cleans session"), Session->HasSession());
			Test->TestTrue(TEXT("Failure reason survives map load"), Session->Status.Contains(TEXT("simulated travel failure")));
			CheckTitle(World);
			return true;
		}
		return false;
	}
private:
	void CheckTitle(UWorld* World)
	{
		Test->TestNotNull(TEXT("Title game mode"), World->GetAuthGameMode<ATBTitleGameMode>());
		auto* PC = World->GetFirstPlayerController();
		Test->TestTrue(TEXT("Title controller has no pawn"), PC && PC->IsA<ATBTitleController>() && !PC->GetPawn());
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UTBTitleMenu::StaticClass(), true);
		Test->TestEqual(TEXT("Exactly one title menu"), Widgets.Num(), 1);
	}
	FAutomationTestBase* Test;
	TWeakObjectPtr<UGameInstance> GI;
	double Deadline;
	int32 Step = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTitleFlowTest, "ToonStory.Title.RoundTrip",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBTitleFlowTest::RunTest(const FString& Parameters)
{
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType != EWorldType::Game || !World || !World->GetAuthGameMode<ATBTitleGameMode>()) continue;
		auto* Online = Online::GetSubsystem(World);
		if (!Online || Online->GetSubsystemName() != FName(TEXT("NULL")))
		{
			AddError(TEXT("Run Title.RoundTrip with the Null online subsystem, never a public Steam session."));
			return false;
		}
		ADD_LATENT_AUTOMATION_COMMAND(FTitleFlow(this, World->GetGameInstance()));
		return true;
	}
	AddError(TEXT("Run with /Game/Maps/Title -game and LAN/Null settings."));
	return false;
}
#endif
