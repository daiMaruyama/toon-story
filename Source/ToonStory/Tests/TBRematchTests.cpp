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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBStorageHoldTest, "ToonStory.Match.StorageHold",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBStorageHoldTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game && Context.World()->GetNetMode() == NM_Standalone)
		{
			World = Context.World();
		}
	}
	if (!TestNotNull(TEXT("Standalone game world"), World))
	{
		return false;
	}
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	if (!Mode || !State || !Host || !State->Box)
	{
		return false;
	}
	const auto OriginalSettings = State->Settings;
	auto* Second = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!TestNotNull(TEXT("Toy player created"), Second))
	{
		return false;
	}
	FTBSettings Rules = OriginalSettings;
	Rules.Humans = 1;
	Rules.Toys = 1;
	Rules.Duration = 120;
	Mode->SetRules(Host, Rules);
	auto* HostInfo = Host->GetPlayerState<ATBPlayerState>();
	auto* ToyInfo = Second->GetPlayerState<ATBPlayerState>();
	HostInfo->Preference = ETBTeam::Human;
	ToyInfo->Preference = ETBTeam::Toy;
	HostInfo->bReady = true;
	ToyInfo->bReady = true;
	Mode->StartRound(Host);
	auto* Human = Cast<ATBCharacter>(Host->GetPawn());
	auto* Toy = Cast<ATBCharacter>(Second->GetPawn());
	auto* Box = State->Box.Get();
	// 箱の正面中央から収納する。
	const FVector Approach(-80, 0, 65);
	const FVector Near = Box->GetActorTransform().TransformPosition(Approach);
	Human->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
	Human->CarriedToy = Toy;
	Toy->Carrier = Human;
	Toy->SetToyState(ETBToyState::Carried);
	Toy->OnRep_Carrier();
	TestEqual(TEXT("Storage takes three seconds"), Box->StoreSeconds, 3.f);
	TestTrue(TEXT("Box front accepts storage"), Box->CanStoreFrom(Human));
	Human->RequestInteract();
	TestEqual(TEXT("Holding E starts storage"), ToyInfo->ToyState, ETBToyState::Storing);
	Human->StoreStarted = World->GetTimeSeconds() - 1;
	Human->ReleaseInteract();
	TestEqual(TEXT("Release cancels without dropping"), ToyInfo->ToyState, ETBToyState::Carried);
	TestTrue(TEXT("Toy remains attached"), Toy->Carrier == Human && Human->CarriedToy == Toy);
	TestEqual(TEXT("Cancelled progress cleared"), Human->StoreStarted, -1.0);
	World->Tick(LEVELTICK_All, .2f);
	Human->RequestInteract();
	Human->SetActorLocation(Box->GetActorTransform().TransformPosition(FVector(-500, 0, 65)), false, nullptr,
	                        ETeleportType::TeleportPhysics);
	Human->Tick(0);
	TestEqual(TEXT("Leaving box cancels storage"), ToyInfo->ToyState, ETBToyState::Carried);
	TestEqual(TEXT("Leaving box resets timer"), Human->StoreStarted, -1.0);
	World->Tick(LEVELTICK_All, .2f);
	Human->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
	Human->RequestInteract();
	Human->StoreStarted = World->GetTimeSeconds() - 2.9;
	Human->Tick(0);
	TestEqual(TEXT("Not stored before three seconds"), ToyInfo->ToyState, ETBToyState::Storing);
	Human->StoreStarted = World->GetTimeSeconds() - 3.01;
	Human->Tick(0);
	TestEqual(TEXT("Stored after continuous three-second hold"), ToyInfo->ToyState, ETBToyState::Boxed);
	TestFalse(TEXT("Successful storage clears carry links"), Human->CarriedToy || Toy->Carrier);
	const FVector RoomPosition =
	    Box->StorageRoom->GetComponentTransform().InverseTransformPosition(Toy->GetActorLocation());
	bool bInStorageSlot = false;
	for (int32 Slot = 0; Slot < 9; ++Slot)
	{
		// 回転した部屋の座標変換による丸め誤差を許容する。
		bInStorageSlot |= RoomPosition.Equals(FVector(200 + (Slot % 3) * 180, -170 + (Slot / 3) * 170, 60), .1);
	}
	TestTrue(TEXT("Storage teleports to the separate room"), bInStorageSlot);
	TestTrue(TEXT("Storage destination is separate from the visible chest"),
	         FVector::Dist(Toy->GetActorLocation(), Box->GetActorLocation()) > 800.f);
	TestFalse(TEXT("Stored toy cannot rescue itself through the chest wall"), Box->InRange(Toy));
	// この2人テストでは収納で試合終了する。救助は進行中の試合でのみ許可される。
	State->Phase = ETBPhase::Playing;
	Box->ReleasePrisoners();
	TestEqual(TEXT("Rescue moves stored toy outside the chest"), ToyInfo->ToyState, ETBToyState::Free);
	TestTrue(TEXT("Rescue destination is in front of the chest"),
	         Box->GetActorTransform().InverseTransformPosition(Toy->GetActorLocation()).X < 0);
	if (State->Phase == ETBPhase::Playing)
	{
		Mode->Finish(ETBWinner::Humans, TEXT("Storage test cleanup"), false);
	}
	Mode->ReturnToLobby(Host);
	UGameplayStatics::RemovePlayer(Second, true);
	State->Settings = OriginalSettings;
	return !HasAnyErrors();
}
#endif
