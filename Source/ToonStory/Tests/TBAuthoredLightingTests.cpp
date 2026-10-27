#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/TBLightSwitch.h"
#include "GamePlay/TBRoomLighting.h"
#include "GamePlay/Items/TBInteractionTarget.h"
#include "Character/TBCharacter.h"
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBLobbySettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBAuthoredLightingTest, "ToonStory.Lighting.AuthoredStage",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBAuthoredLightingTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Entry : GEngine->GetWorldContexts())
	{
		if (Entry.WorldType == EWorldType::Game && Entry.World() && Entry.World()->GetGameState<ATBGameState>())
		{ World = Entry.World(); break; }
	}
	if (!TestNotNull(TEXT("Standalone game world"), World) || World->GetNetMode() != NM_Standalone) return false;
	// The retained Demo has two authored switchable rooms. Workshop uses constant lights instead.
	if (!TestEqual(TEXT("Run authored switch coverage on retained Demo"), TBLobbySettings::StageIndex(World), 1)) return false;
	auto* Controller = Cast<ATBController>(World->GetFirstPlayerController());
	auto* Character = Controller ? Cast<ATBCharacter>(Controller->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Player state"), Character->TBPS())) return false;
	auto* Player = Character->TBPS();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	if (!TestNotNull(TEXT("Authority game mode"), Mode) ||
	    !TestTrue(TEXT("Isolated lighting fixture starts in Lobby with one local player"),
	        State->Phase == ETBPhase::Lobby && State->PlayerArray.Num() == 1 && Controller->IsLocalController()) ||
	    !TestFalse(TEXT("Lighting fixture does not request stage travel"), TBLobbySettings::ShouldTravelToStage(World)) ||
	    !TestFalse(TEXT("Lighting fixture is not the dedicated lobby"),
	        World->GetGameInstance()->GetSubsystem<UTBLobbySettings>()->bDedicatedLobby)) return false;
	const auto OldSettings = State->Settings;
	const auto OldPreference = Player->Preference;
	const bool bOldReady = Player->bReady;
	const auto OldTeam = Player->Team;
	const auto OldToyState = Player->ToyState;
	const bool bOldFrozen = Player->bFrozen;
	const FTransform OldTransform = Character->GetActorTransform();
	const FRotator OldRotation = Controller->GetControlRotation();
	ATBController* Second = nullptr;
	ON_SCOPE_EXIT
	{
		if (State->Phase == ETBPhase::Playing) Mode->Finish(ETBWinner::None, TEXT("Lighting fixture cleanup"), true);
		if (State->Phase == ETBPhase::Results || State->Phase == ETBPhase::Aborted) Mode->ReturnToLobby(Controller);
		if (IsValid(Second)) UGameplayStatics::RemovePlayer(Second, true);
		State->Settings = OldSettings;
		if (auto* RestoredPlayer = Controller->GetPlayerState<ATBPlayerState>())
		{
			RestoredPlayer->Preference = OldPreference;
			RestoredPlayer->bReady = bOldReady;
			RestoredPlayer->Team = OldTeam;
			RestoredPlayer->ToyState = OldToyState;
			RestoredPlayer->bFrozen = bOldFrozen;
		}
		if (auto* RestoredCharacter = Cast<ATBCharacter>(Controller->GetPawn()))
		{
			RestoredCharacter->ApplyState();
			RestoredCharacter->SetActorTransform(OldTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
		Controller->SetControlRotation(OldRotation);
		Mode->SyncLobbyCounts();
	};
	Second = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!TestNotNull(TEXT("Second local player"), Second)) return false;
	if (!TestTrue(TEXT("Second player is local"), Second->IsLocalController())) return false;
	auto* SecondPlayer = Second->GetPlayerState<ATBPlayerState>();
	if (!TestNotNull(TEXT("Second player state"), SecondPlayer)) return false;
	auto Rules = State->Settings;
	Rules.Humans = Rules.Toys = 1;
	Rules.NPCHumans = Rules.NPCToys = 0;
	Mode->SetRules(Controller, Rules);
	Player->Preference = ETBTeam::Human;
	SecondPlayer->Preference = ETBTeam::Toy;
	Player->bReady = SecondPlayer->bReady = true;
	Mode->StartRound(Controller);
	Character = Cast<ATBCharacter>(Controller->GetPawn());
	Player = Controller->GetPlayerState<ATBPlayerState>();
	SecondPlayer = Second->GetPlayerState<ATBPlayerState>();
	if (!TestNotNull(TEXT("Started host pawn"), Character) || !TestNotNull(TEXT("Started host state"), Player) ||
	    !TestNotNull(TEXT("Started second state"), SecondPlayer) ||
	    !TestTrue(TEXT("Normal StartRound commits valid hiding preparation"),
	        State->Phase == ETBPhase::Playing && State->HidePreparation.bValid && State->HidePreparation.bHiding)) return false;
	Mode->AdvancePreparationClockForTest(0);
	if (!TestTrue(TEXT("Server releases valid preparation into the normal match"),
	        State->Phase == ETBPhase::Playing && State->HidePreparation.bValid && !State->HidePreparation.bHiding && !State->IsPreparing()) ||
	    !TestTrue(TEXT("Released match gets the full duration"), State->EndTime > State->GetServerWorldTimeSeconds() &&
	        FMath::IsNearlyEqual(State->EndTime - State->GetServerWorldTimeSeconds(), double(State->Settings.Duration), .01)) ||
	    !TestTrue(TEXT("Normal lighting interaction is permitted after server release"), Character->CanAct())) return false;
	TSet<ALight*> AssignedLights;
	int32 RoomCount = 0, SwitchCount = 0;
	for (TActorIterator<ATBRoomLighting> It(World); It; ++It)
	{
		++RoomCount;
		TestTrue(TEXT("Room has lights"), !It->Lights.IsEmpty());
		for (ALight* Light : It->Lights)
		{
			if (!TestNotNull(TEXT("Saved light reference resolves"), Light)) continue;
			TestFalse(TEXT("Each light belongs to one room"), AssignedLights.Contains(Light));
			AssignedLights.Add(Light);
			TestEqual(TEXT("Light can change at runtime"), Light->GetLightComponent()->Mobility.GetValue(), EComponentMobility::Movable);
		}
	}
	for (TActorIterator<ATBLightSwitch> It(World); It; ++It)
	{
		++SwitchCount;
		auto* Switch = *It;
		auto* Room = Switch->Room.Get();
		if (!TestNotNull(TEXT("Saved room reference resolves"), Room)) continue;
		const bool bInitial = Room->bLightsOn;
		ON_SCOPE_EXIT { Room->SetLightsOn(bInitial); };
		for (ETBTeam Team : {ETBTeam::Human, ETBTeam::Toy})
		{
			Player->Team = Team;
			Character->ApplyState();
			if (!TestTrue(TEXT("Both teams can act in the released lighting fixture"), Character->CanAct())) return false;
			const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
			FCollisionQueryParams Query(SCENE_QUERY_STAT(TBLightingReach), false, Character);
			Query.AddIgnoredActor(Switch);
			FTBInteractionContext Use;
			Use.InstigatorCharacter = Character;
			Use.ActionId = TEXT("Toggle");
			bool bReachable = false;
			for (float Distance : {65.f, 90.f, 115.f})
			{
				for (float Side : {0.f, -40.f, 40.f})
				{
					FVector Position = Switch->GetActorLocation() + Switch->GetActorForwardVector() * Distance + Switch->GetActorRightVector() * Side;
					Position.Z = Switch->GetActorLocation().Z - 55.f + HalfHeight + 2.f;
					if (!Room->Contains(Position) || World->OverlapBlockingTestByChannel(Position, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Query)) continue;
					FHitResult Floor;
					if (!World->LineTraceSingleByChannel(Floor, Position, Position - FVector(0, 0, HalfHeight + 10), ECC_Pawn, Query)) continue;
					Character->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
					Controller->SetControlRotation((Switch->GetInteractionLocation() - Character->GetPawnViewLocation()).Rotation());
					if (Switch->CanInteract(Use) && TB::FindInteractionTarget(Character).Actor == Switch)
					{ bReachable = true; break; }
				}
				if (bReachable) break;
			}
			if (!TestTrue(*FString::Printf(TEXT("%s reachable by team %d from clear floor"), *Switch->GetName(), static_cast<int32>(Team)), bReachable)) continue;
			Room->SetLightsOn(true);
			TestTrue(TEXT("Interact turns room off"), Switch->TryInteract(Use) && !Room->bLightsOn);
			for (ALight* Light : Room->Lights)
				if (Light) TestFalse(TEXT("Real light visibility turns off"), Light->GetLightComponent()->IsVisible());
			TestTrue(TEXT("Interact restores room light"), Switch->TryInteract(Use) && Room->bLightsOn);
		}
	}
	TestEqual(TEXT("Demo retains two furnished switchable rooms"), RoomCount, 2);
	TestEqual(TEXT("One switch per room"), SwitchCount, RoomCount);
	return true;
}
#endif
