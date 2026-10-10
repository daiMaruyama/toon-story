#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBSession.h"
#include "Core/TBTitleGameMode.h"
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Two real processes, each starting on Title. A per-run advertisement prevents
// this regression from joining any unrelated LAN room.
class FTitleNetworkPeer : public IAutomationLatentCommand
{
public:
	FTitleNetworkPeer(FAutomationTestBase* InTest, UGameInstance* Instance, bool Host, FString Run, FString Address)
	    : Test(InTest), GI(Instance), bHost(Host), RunId(MoveTemp(Run)), DirectAddress(MoveTemp(Address)), Started(FPlatformTime::Seconds()) {}
	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (!GI.IsValid() || Now - Started > 360)
		{
			Test->AddError(FString::Printf(TEXT("Network Title flow timed out: %s step %d"), bHost ? TEXT("Host") : TEXT("Client"), Step));
			return true;
		}
		UWorld* World = GI->GetWorld();
		if (!World || !World->HasBegunPlay()) return false;
		auto* Session = GI->GetSubsystem<UTBSession>();
		auto* State = World->GetGameState<ATBGameState>();
		auto* Controller = Cast<ATBController>(World->GetFirstPlayerController());
		if (bHost)
		{
			if (Step == 0) { Session->Host(); Step = 1; }
			else if (Step == 1 && State && Controller && !Session->IsBusy())
			{
				auto* Mode = World->GetAuthGameMode<ATBGameMode>();
				FTBSettings Rules;
				Rules.Humans = 1; Rules.Toys = 1; Rules.Duration = 10; Rules.RequiredItems = 5;
				Mode->SetRules(Controller, Rules);
				auto Settings = *Session->Sessions->GetSessionSettings(NAME_GameSession);
				Settings.Set(FName(TEXT("TITLE_TEST_RUN")), RunId, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
				Session->Sessions->UpdateSession(NAME_GameSession, Settings, true);
				Controller->ServerReady();
				Step = 2;
			}
			else if ((Step == 2 || Step == 5) && State && Controller && State->PlayerArray.Num() == 2)
			{
				bool Ready = true;
				for (APlayerState* Player : State->PlayerArray)
				{
					const auto* Info = Cast<ATBPlayerState>(Player);
					Ready &= Info && Info->bReady;
				}
				if (Ready)
				{
					Controller->ServerStart();
					Test->TestEqual(TEXT("Two network players started"), State->Phase, ETBPhase::Playing);
					Step = Step == 2 ? 3 : 6;
				}
			}
			else if ((Step == 3 || Step == 6) && State && State->Phase == ETBPhase::Results)
			{
				NextAction = Now + 2; // Allow the client to observe the replicated result.
				Step = Step == 3 ? 4 : 7;
			}
			else if (Step == 4 && Now >= NextAction && Controller)
			{
				Controller->TBLobby();
				Test->TestTrue(TEXT("Rematch retains network room"), Session->HasSession() && State->PlayerArray.Num() == 2);
				Controller->ServerReady();
				Step = 5;
			}
			else if (Step == 7 && Now >= NextAction)
			{
				Session->Leave();
				Step = 8;
			}
			else if (Step == 8 && World->GetAuthGameMode<ATBTitleGameMode>() && !Session->IsBusy())
			{
				Test->TestFalse(TEXT("Host closed session"), Session->HasSession());
				return true;
			}
		}
		else
		{
			if (Step == 0 && !Session->IsBusy())
			{
				if (!DirectAddress.IsEmpty())
				{
					World->GetFirstPlayerController()->ClientTravel(DirectAddress, TRAVEL_Absolute);
					Step = 1;
					return false;
				}
				for (int32 Index = 0; Index < Session->Results.Num(); ++Index)
				{
					FString Tag;
					if (Session->Results[Index].Session.SessionSettings.Get(FName(TEXT("TITLE_TEST_RUN")), Tag) && Tag == RunId)
					{
						Session->Join(Index);
						Step = 1;
						return false;
					}
				}
				if (Now >= NextAction) { Session->Find(); NextAction = Now + 3; }
			}
			else if (Step == 1 && State && Controller && !Session->IsBusy())
			{
				Test->TestEqual(TEXT("Joined a real remote host"), World->GetNetMode(), NM_Client);
				Controller->ServerReady();
				Step = 2;
			}
			else if (Step == 2 && State && State->Phase == ETBPhase::Results) Step = 3;
			else if (Step == 3 && State && State->Phase == ETBPhase::Lobby && Controller)
			{
				Test->TestFalse(TEXT("Ready reset after rematch"), Controller->GetPlayerState<ATBPlayerState>()->bReady);
				Test->TestEqual(TEXT("Connection kept through rematch"), World->GetNetMode(), NM_Client);
				Controller->ServerReady();
				Step = 4;
			}
			else if (Step == 4 && State && State->Phase == ETBPhase::Playing)
			{
				// Only the deliberate host close after the second match is expected.
				// Earlier/unrelated connection failures must still fail the test.
				Test->AddExpectedError(TEXT("UEngine::BroadcastNetworkFailure: FailureType = FailureReceived, ErrorString = Host closed the connection."), EAutomationExpectedErrorFlags::Contains, 1, false);
				Test->AddExpectedError(TEXT("UEngine::BroadcastNetworkFailure: FailureType = ConnectionLost, ErrorString = Your connection to the host has been lost."), EAutomationExpectedErrorFlags::Contains, 1, false);
				Step = 5;
			}
			else if (Step == 5 && World->GetAuthGameMode<ATBTitleGameMode>() && !Session->IsBusy())
			{
				Test->TestFalse(TEXT("Host departure cleans client session"), Session->HasSession());
				Test->TestNull(TEXT("No pawn after disconnect"), World->GetFirstPlayerController()->GetPawn());
				return true;
			}
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<UGameInstance> GI;
	bool bHost;
	FString RunId, DirectAddress;
	double Started, NextAction = 0;
	int32 Step = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTitleNetworkTest, "ToonStory.Title.NetworkPeer",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBTitleNetworkTest::RunTest(const FString& Parameters)
{
	FString Role, Run, Address;
	FParse::Value(FCommandLine::Get(), TEXT("TitlePeer="), Role);
	FParse::Value(FCommandLine::Get(), TEXT("TitleRun="), Run);
	FParse::Value(FCommandLine::Get(), TEXT("TitleDirectAddress="), Address);
	if (!Address.IsEmpty() && Address != TEXT("127.0.0.1:7777"))
	{
		AddError(TEXT("The direct-transport diagnostic only permits 127.0.0.1:7777."));
		return false;
	}
	if ((Role != TEXT("Host") && Role != TEXT("Client")) || Run.IsEmpty())
	{
		AddError(TEXT("Launch two Title game processes with -TitlePeer=Host/Client and the same unique -TitleRun= value."));
		return false;
	}
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType != EWorldType::Game || !World || !World->GetAuthGameMode<ATBTitleGameMode>()) continue;
		auto* Subsystem = Online::GetSubsystem(World);
		if (!Subsystem || Subsystem->GetSubsystemName() != FName(TEXT("NULL"))) break;
		ADD_LATENT_AUTOMATION_COMMAND(FTitleNetworkPeer(this, World->GetGameInstance(), Role == TEXT("Host"), Run, Address));
		return true;
	}
	AddError(TEXT("NetworkPeer requires Title -game with LAN/Null configuration."));
	return false;
}
#endif
