// Copyright (c) 2026 Mamodou. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Core/TBPlayerState.h"
#include "Core/TBLobbySettings.h"
#include "Character/TBCharacter.h"
#include "Character/TBNPCController.h"
#include "Data/TBRuleData.h"
#include "GamePlay/TBPickup.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBHidePreparationTest, "ToonStory.Match.HidePreparation",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBHidePreparationTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World() &&
		    Context.World()->GetAuthGameMode<ATBGameMode>()) { World = Context.World(); break; }
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	if (!State || !Host || State->Phase != ETBPhase::Lobby || State->PlayerArray.Num() != 1 ||
	    World->GetGameInstance()->GetSubsystem<UTBLobbySettings>()->bDedicatedLobby ||
	    TBLobbySettings::ShouldTravelToStage(World))
	{
		AddError(TEXT("Run in a fresh standalone Arena lobby with one local player."));
		return false;
	}
	const FTBSettings Original = State->Settings;
	auto& TimingRules = const_cast<UTBRuleData&>(TB::Rules()).Match;
	const float OriginalHideSeconds = TimingRules.HidePreparationSeconds;
	TimingRules.HidePreparationSeconds = 6.f;
	ON_SCOPE_EXIT { TimingRules.HidePreparationSeconds = OriginalHideSeconds; };
	const ETBTeam Preference = Host->GetPlayerState<ATBPlayerState>()->Preference;
	auto* Second = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!Second) return false;
	const TWeakObjectPtr<UWorld> WeakWorld(World);
	auto Cleanup = [WeakWorld, Host, Second, Original, Preference]()
	{
		if (!WeakWorld.IsValid()) return;
		auto* Current = WeakWorld.Get();
		auto* GM = Current->GetAuthGameMode<ATBGameMode>();
		auto* Match = Current->GetGameState<ATBGameState>();
		if (!GM || !Match || !IsValid(Host)) return;
		if (Match->Phase == ETBPhase::Playing) GM->Finish(ETBWinner::None, TEXT("Preparation fixture cleanup"), true);
		if (Match->Phase == ETBPhase::Results || Match->Phase == ETBPhase::Aborted) GM->ReturnToLobby(Host);
		if (IsValid(Second)) UGameplayStatics::RemovePlayer(Second, true);
		Match->Settings = Original;
		Host->GetPlayerState<ATBPlayerState>()->Preference = Preference;
		GM->SyncLobbyCounts();
	};
	bool Deferred = false;
	ON_SCOPE_EXIT { if (!Deferred) Cleanup(); };
	// A transient missing pending snapshot must not reveal an in-flight travel.
	Host->PreClientTravel(TEXT("/Game/Maps/MissingPreparationFixture"), TRAVEL_Absolute, true);
	TestTrue(TEXT("Travel remains hidden before pending replication"), Host->ShouldHidePreparationView());
	State->bTravelPending = true;
	Host->ClientPreparationTravelCancelled();
	TestTrue(TEXT("Failure marker alone waits for terminal pending state"), Host->ShouldHidePreparationView());
	State->bTravelPending = false;
	TestFalse(TEXT("Terminal failed travel reveals the surviving stage lobby"), Host->ShouldHidePreparationView());
	Host->PreClientTravel(TEXT("/Game/Maps/MissingPreparationFixture"), TRAVEL_Absolute, true);
	Mode->bObservedPreparationArrival = true;
	Mode->Tick(0);
	TestFalse(TEXT("Arrival cancellation without a travel-failure callback releases the view"), Host->ShouldHidePreparationView());
	FTBSettings Rules = State->Settings;
	Rules.Humans = 1; Rules.Toys = 1; Rules.NPCHumans = 1; Rules.NPCToys = 1;
	Rules.Duration = 60; Rules.RequiredItems = 1;
	Mode->SetRules(Host, Rules);
	Host->GetPlayerState<ATBPlayerState>()->Preference = ETBTeam::Human;
	Second->GetPlayerState<ATBPlayerState>()->Preference = ETBTeam::Toy;
	auto Start = [&]()
	{
		Host->GetPlayerState<ATBPlayerState>()->bReady = true;
		Second->GetPlayerState<ATBPlayerState>()->bReady = true;
		Mode->StartRound(Host);
	};
	Start();
	if (!TestTrue(TEXT("Successful placement starts preparation"), State->IsPreparing() && State->HidePreparation.bValid)) return false;
	const uint32 FirstRound = State->HidePreparation.Round;
	TestTrue(TEXT("Committed interval uses the configured six seconds"), FMath::IsNearlyEqual(double(State->HideRemaining()), 6.0, .05));
	const auto Preparation = State->HidePreparation;
	const double Now = State->GetServerWorldTimeSeconds();
	State->HidePreparation.StartsAt = Now - 3.;
	State->HidePreparation.EndsAt = Now + 3.;
	TestTrue(TEXT("Camera progress follows replicated timing, not a ten-second constant"),
	    FMath::IsNearlyEqual(State->HideProgress(), .5f));
	State->HidePreparation = Preparation;
	TestEqual(TEXT("Ordinary deadline is unset"), State->EndTime, 0.0);
	TestEqual(TEXT("Full duration retained while hiding"), State->Remaining(), 60.f);
	auto* Human = Cast<ATBCharacter>(Host->GetPawn());
	auto* Toy = Cast<ATBCharacter>(Second->GetPawn());
	if (!Human || !Toy) return false;
	TestTrue(TEXT("Human is physics locked"), Human->IsPhysicsLocked());
	TestFalse(TEXT("Toy can move while interaction is disabled"), Toy->IsMovementLocked());
	TestFalse(TEXT("Human cannot capture"), Human->CanAct());
	TestFalse(TEXT("Toy cannot interact or collect"), Toy->CanAct());
	TestTrue(TEXT("Human sees held preparation screen"), Host->ShouldHidePreparationView());
	TestFalse(TEXT("Confirmed toy can see the world"), Second->ShouldHidePreparationView());
	Host->ClientPreparationRole(FirstRound + 1, ETBTeam::Toy);
	TestTrue(TEXT("Reordered role/round remains closed"), Host->ShouldHidePreparationView());
	Host->ClientPreparationRole(FirstRound, ETBTeam::Human);
	State->HidePreparation.bValid = false;
	TestTrue(TEXT("Missing replicated preparation remains closed for toys too"), Second->ShouldHidePreparationView());
	State->HidePreparation.bValid = true;
	Toy->AddMovementInput(FVector::ForwardVector, 1.f);
	TestTrue(TEXT("Toy movement request is accepted"), !Toy->ConsumeMovementInputVector().IsNearlyZero());
	Human->RequestInteract();
	TestFalse(TEXT("Capture request did not attach a toy"), Human->HasCarriedToy());
	Mode->EvaluateGaze();
	TestFalse(TEXT("Human gaze does not freeze toys during hiding"), Toy->TBPS()->bFrozen);
	ATBPickup* Pickup = nullptr;
	for (TActorIterator<ATBPickup> It(World); It; ++It) { Pickup = *It; break; }
	if (!TestNotNull(TEXT("Arena pickup"), Pickup)) return false;
	Toy->ContactItem = Pickup; Toy->ItemProgress = 1;
	TestFalse(TEXT("Authority pickup call rejects preparation"), Pickup->Take(Toy));
	Toy->Tick(0);
	TestTrue(TEXT("Contact and progress reset while hiding"), !Toy->ContactItem && Toy->ItemProgress == 0);
	TestEqual(TEXT("No collection during hiding"), State->Collected, 0);
	State->Collected = State->Settings.RequiredItems;
	Mode->CheckWin();
	TestEqual(TEXT("Preparation does not produce a winner"), State->Winner, ETBWinner::None);
	TestEqual(TEXT("Even a fulfilled objective cannot finish during preparation"), State->Phase, ETBPhase::Playing);
	State->Collected = 0;
	Mode->AdvancePreparationClockForTest(.001);
	TestTrue(TEXT("Before the release boundary remains hiding"), State->IsPreparing());
	State->HidePreparation.EndsAt = State->GetServerWorldTimeSeconds();
	TestTrue(TEXT("Client zero countdown is not authority release"), Host->ShouldHidePreparationView());
	Mode->AdvanceHidePreparation();
	TestFalse(TEXT("Server releases at the boundary"), State->IsPreparing());
	TestTrue(TEXT("Ordinary duration starts in full at release"), FMath::IsNearlyEqual(double(State->Remaining()), 60.0, .05));
	TestFalse(TEXT("Human unlocks at server release"), Human->IsPhysicsLocked());
	TestFalse(TEXT("Human view releases only after authority state"), Host->ShouldHidePreparationView());
	const double Deadline = State->EndTime;
	Mode->AdvanceHidePreparation();
	TestEqual(TEXT("Duplicate release cannot extend the round"), State->EndTime, Deadline);
	Host->PlayerTick(0); Second->PlayerTick(0);
	const auto Released = State->HidePreparation;
	const uint32 SimulatedNextRound = FirstRound + 100;
	Host->ClientPreparationBegin(SimulatedNextRound);
	Host->PlayerTick(0);
	TestTrue(TEXT("New Begin survives an old released Playing snapshot"), Host->ShouldHidePreparationView());
	State->Phase = ETBPhase::Results;
	Host->PlayerTick(0);
	TestTrue(TEXT("New Begin survives an old Results snapshot"), Host->ShouldHidePreparationView());
	Host->ClientPreparationCancelled(FirstRound);
	TestTrue(TEXT("Old cancel cannot release the newer Begin"), Host->ShouldHidePreparationView());
	Host->ClientPreparationRole(SimulatedNextRound, ETBTeam::Human);
	State->Phase = ETBPhase::Playing;
	TestTrue(TEXT("New role before new payload stays closed"), Host->ShouldHidePreparationView());
	Second->ClientPreparationBegin(SimulatedNextRound);
	Second->ClientPreparationCancelled(FirstRound);
	TestTrue(TEXT("Toy new Begin cannot be cleared by old cancel"), Second->ShouldHidePreparationView());
	State->HidePreparation.Round = SimulatedNextRound;
	State->HidePreparation.bHiding = true;
	TestTrue(TEXT("New payload before toy role stays closed"), Second->ShouldHidePreparationView());
	Second->ClientPreparationRole(SimulatedNextRound, ETBTeam::Toy);
	TestFalse(TEXT("Matching new role and payload reveal only the toy"), Second->ShouldHidePreparationView());
	TestTrue(TEXT("Matching human still waits for authority release"), Host->ShouldHidePreparationView());
	Host->ClientPreparationCancelled(SimulatedNextRound);
	Second->ClientPreparationCancelled(SimulatedNextRound);
	State->HidePreparation = Released;
	Host->ClientPreparationRole(FirstRound, ETBTeam::Human);
	Second->ClientPreparationRole(FirstRound, ETBTeam::Toy);
	Host->PlayerTick(0); Second->PlayerTick(0);
	Mode->Finish(ETBWinner::Humans, TEXT("Preparation first round"));
	TestFalse(TEXT("Results invalidate preparation"), State->HidePreparation.bValid);
	Mode->ReturnToLobby(Host);
	TestFalse(TEXT("Lobby has no preparation deadline"), State->HidePreparation.bValid);
	Start();
	if (!TestTrue(TEXT("Rematch begins a new preparation"), State->IsPreparing())) return false;
	TestTrue(TEXT("Rematch has a new round generation"), State->HidePreparation.Round != FirstRound);
	// No PlayerTick observes Results/Aborted between these calls: explicit cancel must still end the hold.
	Mode->Finish(ETBWinner::None, TEXT("Preparation cancellation fixture"), true);
	Mode->ReturnToLobby(Host);
	TestFalse(TEXT("Unobserved intermediate phase cannot leave a permanent black lobby"), Host->ShouldHidePreparationView());
	Start();
	if (!TestTrue(TEXT("A later Begin still starts preparation after cancellation"), State->IsPreparing())) return false;
	ATBNPCController* ToyNPC = nullptr;
	ATBNPCController* HumanNPC = nullptr;
	for (TActorIterator<ATBNPCController> It(World); It; ++It)
	{
		if (Cast<ATBCharacter>(It->GetPawn())->IsHuman()) HumanNPC = *It;
		else ToyNPC = *It;
	}
	if (!TestNotNull(TEXT("Toy NPC"), ToyNPC) || !TestNotNull(TEXT("Human NPC"), HumanNPC)) return false;
	HumanNPC->Tick(.5f);
	TestTrue(TEXT("Human NPC stays idle without a focus"), !HumanNPC->GetFocusActor() && HumanNPC->GetPawn()->GetVelocity().IsNearlyZero());
	const FVector NPCOrigin = ToyNPC->GetPawn()->GetActorLocation();
	const double MovementDeadline = FPlatformTime::Seconds() + 8;
	// Real AI/path movement during the committed ten seconds, independent of normal-round fixture shortcuts.
	Deferred = true;
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, WeakWorld, ToyNPC, HumanNPC, NPCOrigin, MovementDeadline, Mode, State, Second, Cleanup]()
	{
		if (!WeakWorld.IsValid() || !IsValid(ToyNPC) || !IsValid(HumanNPC))
		{ AddError(TEXT("Preparation movement fixture lost an actor")); Cleanup(); return true; }
		const bool Moved = FVector::Dist2D(ToyNPC->GetPawn()->GetActorLocation(), NPCOrigin) > 10.f;
		if (!Moved && State->IsPreparing() && FPlatformTime::Seconds() < MovementDeadline) return false;
		TestTrue(TEXT("ThinkHide produces actual toy NPC movement"), Moved && State->IsPreparing());
		TestEqual(TEXT("NPC hiding never collects a battery"), State->Collected, 0);
		TestTrue(TEXT("Human NPC remains stopped"), HumanNPC->GetPawn()->GetVelocity().IsNearlyZero());
		// Losing the entire toy team during preparation must abort without awarding a winner.
		Mode->RemoveNPCs();
		UGameplayStatics::RemovePlayer(Second, true);
		Mode->AdvanceHidePreparation();
		TestEqual(TEXT("Missing team aborts preparation"), State->Phase, ETBPhase::Aborted);
		TestEqual(TEXT("No winner for cancelled preparation"), State->Winner, ETBWinner::None);
		TestFalse(TEXT("Cancellation invalidates the countdown"), State->HidePreparation.bValid);
		Cleanup();
		return true;
	}));
	return true;
}
#endif
