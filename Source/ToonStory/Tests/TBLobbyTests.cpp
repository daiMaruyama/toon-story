#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLobbyCountsTest, "ToonStory.Lobby.Counts",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBLobbyCountsTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game && Context.World()->GetAuthGameMode<ATBGameMode>())
		{
			World = Context.World();
			break;
		}
	}
	if (!TestNotNull(TEXT("Standalone game world"), World)) return false;
	if (!TestEqual(TEXT("Isolated standalone test"), World->GetNetMode(), NM_Standalone)) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	if (!TestNotNull(TEXT("State"), State) || !TestNotNull(TEXT("Host"), Host)) return false;
	auto Check = [&](int32 Humans, int32 Toys)
	{
		TestEqual(TEXT("Children count"), State->Settings.Humans, Humans);
		TestEqual(TEXT("Toy count"), State->Settings.Toys, Toys);
		TestEqual(TEXT("Total matches connected players"), State->Settings.Humans + State->Settings.Toys,
		          State->PlayerArray.Num());
	};
	Check(1, 0);
	Host->GetPlayerState<ATBPlayerState>()->bReady = true;
	Mode->StartRound(Host);
	TestEqual(TEXT("Cannot start alone"), State->Phase, ETBPhase::Lobby);
	auto* Second = UGameplayStatics::CreatePlayer(World, -1, true);
	if (!TestNotNull(TEXT("Second player"), Second)) return false;
	Check(1, 1);
	TestFalse(TEXT("Joining resets ready"), Host->GetPlayerState<ATBPlayerState>()->bReady);
	auto* Third = UGameplayStatics::CreatePlayer(World, -1, true);
	if (!TestNotNull(TEXT("Third player"), Third))
	{
		UGameplayStatics::RemovePlayer(Second, true);
		return false;
	}
	Check(1, 2);
	auto Rules = State->Settings;
	Rules.Humans = 2;
	Mode->SetRules(Host, Rules);
	Check(2, 1);
	Rules = State->Settings;
	Rules.Toys = 2;
	Mode->SetRules(Host, Rules);
	Check(1, 2);
	Rules = State->Settings;
	Rules.Toys = 1;
	Mode->SetRules(Host, Rules);
	Check(2, 1);
	Rules = State->Settings;
	Rules.Humans = 1;
	Mode->SetRules(Host, Rules);
	Check(1, 2);
	Rules = State->Settings;
	Rules.Toys = 3;
	Mode->SetRules(Host, Rules);
	Check(1, 2); // Emptying the child team is rejected.
	Rules.Humans = 3;
	Rules.Toys = 3;
	Mode->SetRules(Host, Rules);
	Check(1, 2); // Explicit mismatched pairs are rejected.
	Rules = State->Settings;
	Rules.Humans = 2;
	Mode->SetRules(Host, Rules);
	Check(2, 1);
	UGameplayStatics::RemovePlayer(Third, true);
	Check(1, 1); // Children decrease only when needed to retain a toy.
	UGameplayStatics::RemovePlayer(Second, true);
	Check(1, 0);
	Second = UGameplayStatics::CreatePlayer(World, -1, true);
	if (!TestNotNull(TEXT("Rejoining player"), Second)) return false;
	Check(1, 1);
	Mode->Finish(ETBWinner::Toys, TEXT("test"));
	UGameplayStatics::RemovePlayer(Second, true);
	Mode->ReturnToLobby(Host);
	Check(1, 0);
	return true;
}
#endif
