#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBCharacter.h"
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBPickup.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRematchTest, "ToonStory.Match.Rematch",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBRematchTest::RunTest(const FString& Parameters)
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
	if (!TestNotNull(TEXT("Run in a standalone game world"), World))
	{
		return false;
	}
	if (!TestEqual(TEXT("Isolated test requires standalone"), World->GetNetMode(), NM_Standalone))
	{
		return false;
	}
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	if (!TestNotNull(TEXT("Game state"), State) || !TestNotNull(TEXT("Host"), Host))
	{
		return false;
	}
	auto* Second = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!TestNotNull(TEXT("Second player"), Second))
	{
		return false;
	}
	auto* HostInfo = Host->GetPlayerState<ATBPlayerState>();
	auto* ToyInfo = Second->GetPlayerState<ATBPlayerState>();
	if (!HostInfo || !ToyInfo)
	{
		AddError(TEXT("Both players require PlayerState"));
		UGameplayStatics::RemovePlayer(Second, true);
		return false;
	}
	const auto OriginalSettings = State->Settings;
	FTBSettings Rules;
	Rules.Humans = 1;
	Rules.Toys = 1;
	Rules.Duration = 60;
	Rules.RequiredItems = 5;
	Mode->SetRules(Host, Rules);
	HostInfo->Preference = ETBTeam::Human;
	ToyInfo->Preference = ETBTeam::Toy;
	const FString MapName = World->GetMapName();
	for (int32 Round = 0; Round < 3; ++Round)
	{
		Mode->StartRound(Host);
		TestEqual(TEXT("Ready is required each round"), State->Phase, ETBPhase::Lobby);
		HostInfo->bReady = true;
		ToyInfo->bReady = true;
		Mode->StartRound(Host);
		TestEqual(TEXT("Round starts"), State->Phase, ETBPhase::Playing);
		Mode->ReturnToLobby(Host);
		TestEqual(TEXT("Cannot reset an active match"), State->Phase, ETBPhase::Playing);
		auto* Human = Cast<ATBCharacter>(Host->GetPawn());
		auto* Toy = Cast<ATBCharacter>(Second->GetPawn());
		if (!Human || !Toy || !State->Box)
		{
			AddError(TEXT("Round requires both characters and a box"));
			break;
		}
		// Cover collection, boxed/frozen toys, and an attached toy at match end.
		ToyInfo->ToyState = Round == 0 ? ETBToyState::Boxed : ETBToyState::Carried;
		ToyInfo->bFrozen = true;
		ToyInfo->Items = 3;
		if (Round > 0)
		{
			Human->CarriedToy = Toy;
			Toy->Carrier = Human;
			Toy->OnRep_Carrier();
		}
		Toy->ItemProgress = .75f;
		Human->GrabEnds = World->GetTimeSeconds() + 100;
		Human->StoreStarted = World->GetTimeSeconds();
		for (TActorIterator<ATBPickup> It(World); It; ++It)
		{
			It->bTaken = true;
			It->OnRep_Taken();
			Toy->ContactItem = *It;
		}
		State->Collected = 3;
		State->BoxedCount = 1;
		Mode->Finish(ETBWinner::Humans, TEXT("Rematch regression"), Round == 2);
		State->Box->RescueProgress = .8f;
		State->Box->Rescuers = 2;
		State->Box->AlarmUntil = World->GetTimeSeconds() + 100;
		const auto FinishedPhase = State->Phase;
		Mode->ReturnToLobby(nullptr);
		TestEqual(TEXT("Unauthorised reset rejected"), State->Phase, FinishedPhase);
		Host->TBLobby();
		TestEqual(TEXT("Returns to lobby"), State->Phase, ETBPhase::Lobby);
		TestEqual(TEXT("Map retained"), World->GetMapName(), MapName);
		TestEqual(TEXT("Members retained"), State->PlayerArray.Num(), 2);
		TestTrue(TEXT("Host identity retained"), Host->GetPlayerState<ATBPlayerState>() == HostInfo);
		TestTrue(TEXT("Toy identity retained"), Second->GetPlayerState<ATBPlayerState>() == ToyInfo);
		TestEqual(TEXT("Rules retained"), State->Settings.RequiredItems, Rules.RequiredItems);
		TestEqual(TEXT("Preference retained"), ToyInfo->Preference, ETBTeam::Toy);
		TestEqual(TEXT("Winner cleared"), State->Winner, ETBWinner::None);
		TestTrue(TEXT("Reason cleared"), State->Reason.IsEmpty());
		TestEqual(TEXT("Timer cleared"), State->EndTime, 0.0);
		TestEqual(TEXT("Collected count cleared"), State->Collected, 0);
		TestEqual(TEXT("Boxed count cleared"), State->BoxedCount, 0);
		TestEqual(TEXT("Rescue progress cleared"), State->Box->RescueProgress, 0.f);
		TestEqual(TEXT("Rescuers cleared"), State->Box->Rescuers, 0);
		TestEqual(TEXT("Alarm cleared"), State->Box->AlarmUntil, 0.0);
		for (auto* Controller : {Host, Second})
		{
			const auto* Info = Controller->GetPlayerState<ATBPlayerState>();
			auto* Pawn = Cast<ATBCharacter>(Controller->GetPawn());
			TestFalse(TEXT("Ready cleared"), Info->bReady);
			TestFalse(TEXT("Frozen cleared"), Info->bFrozen);
			TestEqual(TEXT("Team unassigned"), Info->Team, ETBTeam::None);
			TestEqual(TEXT("Toy free"), Info->ToyState, ETBToyState::Free);
			TestEqual(TEXT("Player items cleared"), Info->Items, 0);
			if (TestNotNull(TEXT("Pawn respawned"), Pawn))
			{
				TestTrue(TEXT("Fresh pawn"), Pawn != Human && Pawn != Toy);
				TestTrue(TEXT("Carry and contact cleared"), !Pawn->Carrier && !Pawn->CarriedToy && !Pawn->ContactItem);
				TestEqual(TEXT("Collection progress cleared"), Pawn->ItemProgress, 0.f);
				TestEqual(TEXT("Grab timer cleared"), Pawn->GrabEnds, 0.0);
				TestFalse(TEXT("Interaction cleared"), Pawn->bHoldingInteract || Pawn->bRescuing);
				Controller->PlayerTick(0.f);
				TestTrue(TEXT("Lobby camera restored"),
				         Controller->GetViewTarget()->ActorHasTag(TEXT("TBLobbyCamera")));
			}
		}
		for (TActorIterator<ATBPickup> It(World); It; ++It)
		{
			TestFalse(TEXT("Pickup available again"), It->bTaken || It->IsHidden());
			TestEqual(TEXT("Pickup contact restored"), It->Contact->GetCollisionEnabled(),
			          ECollisionEnabled::QueryOnly);
		}
		APawn* LobbyPawn = Host->GetPawn();
		Host->TBLobby();
		TestTrue(TEXT("Duplicate return is ignored"), Host->GetPawn() == LobbyPawn);
	}
	UGameplayStatics::RemovePlayer(Second, true);
	State->Settings = OriginalSettings;
	return !HasAnyErrors();
}
#endif
