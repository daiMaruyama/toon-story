#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBCharacter.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/Items/TBEscapeVehicle.h"
#include "GamePlay/Items/TBEscapeExit.h"
#include "Tests/TBTestMatch.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBEscapeVehicleTest, "ToonStory.Items.EscapeVehicle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBEscapeVehicleTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetGameState<ATBGameState>())
		{ World = Context.World(); break; }
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	if (World->GetMapName().Contains(TEXT("Arena")))
	{
		TestTrue(TEXT("Test arena has a vehicle"), bool(TActorIterator<ATBEscapeVehicle>(World)));
		TestTrue(TEXT("Test arena has an exit"), bool(TActorIterator<ATBEscapeExit>(World)));
	}
	auto* Match = World->GetGameState<ATBGameState>();
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* PC = World->GetFirstPlayerController();
	auto* Toy = PC ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Mode"), Mode) || !TestNotNull(TEXT("Toy"), Toy) || !TestNotNull(TEXT("Player"), Toy->TBPS())) return false;
	auto* Info = Toy->TBPS();
	const auto OldTeam = Info->Team;
	const auto OldToyState = Info->ToyState;
	const bool OldFrozen = Info->bFrozen, OldEscape = Match->bEscapePhase;
	const bool OldResult = Match->bVehicleEscapeResult, OldEscaped = Info->bEscapedInVehicle;
	const ETBWinner OldWinner = Match->Winner;
	const FString OldReason = Match->Reason;
	const FVector OldLocation = Toy->GetActorLocation();
	TBTest::FPlayingScope Playing(*Match, Match->GetServerWorldTimeSeconds() + 5.0);
	Info->Team = ETBTeam::Toy; Info->ToyState = ETBToyState::Free; Info->bFrozen = false;
	Match->bEscapePhase = false;
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Car = World->SpawnActor<ATBEscapeVehicle>(FVector(10000,10000,10000), FRotator::ZeroRotator, Params);
	ATBEscapeExit* Exit = nullptr;
	APlayerController* Guest = nullptr;
	ON_SCOPE_EXIT {
		if (IsValid(Exit)) Exit->Destroy();
		if (IsValid(Car)) Car->Destroy();
		if (IsValid(Guest)) UGameplayStatics::RemovePlayer(Guest, true);
		Info->Team = OldTeam; Info->ToyState = OldToyState; Info->bFrozen = OldFrozen;
		Info->bEscapedInVehicle = OldEscaped;
		Match->bEscapePhase = OldEscape; Match->bVehicleEscapeResult = OldResult;
		Match->Winner = OldWinner; Match->Reason = OldReason;
		Toy->SetActorLocation(OldLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Toy->ApplyState();
	};
	if (!TestNotNull(TEXT("Vehicle"), Car)) return false;
	TestFalse(TEXT("Cannot board before escape phase"), Car->Board(Toy));
	TestTrue(TEXT("Escape phase starts"), Mode->StartEscapePhase());
	TestTrue(TEXT("At least 30 seconds remain"), Match->Remaining() >= 29.9f);
	TestFalse(TEXT("Phase cannot start twice"), Mode->StartEscapePhase());
	TestTrue(TEXT("Toy boards"), Car->Board(Toy));
	TestTrue(TEXT("First passenger drives"), Car->IsDriver(Toy));
	TestFalse(TEXT("Passenger is protected from gaze"), Toy->CanBeFrozenBy(Toy));
	Guest = UGameplayStatics::CreatePlayer(World, -1, true);
	ATBCharacter* Passenger = Guest ? Cast<ATBCharacter>(Guest->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Second passenger"), Passenger) || !TestNotNull(TEXT("Second player state"), Passenger->TBPS())) return false;
	Passenger->TBPS()->Team = ETBTeam::Toy;
	Passenger->TBPS()->ToyState = ETBToyState::Free;
	Passenger->TBPS()->bFrozen = false;
	TestTrue(TEXT("Second toy boards"), Car->Board(Passenger));
	TestFalse(TEXT("Second passenger is not the driver"), Car->IsDriver(Passenger));
	const FVector Before = Car->GetActorLocation();
	Car->SetDrivingInput(Toy, FVector2D(0, 1));
	Car->Tick(.1f);
	TestTrue(TEXT("Driver advances the vehicle"), Car->GetActorLocation().X > Before.X);
	TestTrue(TEXT("Passenger leaves"), Car->Leave(Toy));
	TestNull(TEXT("Vehicle reference cleared"), Toy->EscapeVehicle.Get());
	TestTrue(TEXT("Driving passes to the next passenger"), Car->IsDriver(Passenger));
	Exit = World->SpawnActor<ATBEscapeExit>(Car->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("Exit"), Exit)) return false;
	Car->Collision->UpdateOverlaps();
	Exit->ExitArea->UpdateOverlaps();
	TestTrue(TEXT("Vehicle overlaps exit"), Exit->ExitArea->IsOverlappingActor(Car));
	Exit->Tick(.01f);
	TestTrue(TEXT("Passenger is recorded as escaped"), Passenger->TBPS()->bEscapedInVehicle);
	TestFalse(TEXT("Toy left behind is not recorded as escaped"), Info->bEscapedInVehicle);
	TestTrue(TEXT("Vehicle escape result is recorded"), Match->bVehicleEscapeResult);
	TestEqual(TEXT("Reaching exit ends the match for toys"), Match->Winner, ETBWinner::Toys);
	return true;
}
#endif
