#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Core/BatteryTagGameMode.h"
#include "Core/BatteryTagGameState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

namespace ToonMatchTests
{
	struct FMatchWorld
	{
		UWorld* World;
		ABatteryTagGameMode* Mode;
		ABatteryTagGameState* State;
		APlayerState* ToyA;
		APlayerState* ToyB;

		FMatchWorld()
		{
			UWorld::InitializationValues Values;
			Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Mode = World->SpawnActor<ABatteryTagGameMode>();
			Mode->PreInitializeComponents();
			State = Mode->GetGameState<ABatteryTagGameState>();
			ToyA = World->SpawnActor<APlayerState>();
			ToyB = World->SpawnActor<APlayerState>();
			World->GetTimerManager().Tick(0.0f);
		}

		void AdvanceFrame(float Seconds)
		{
			// TimerManager ticks at most once per engine frame. Synchronous tests advance it explicitly.
			++GFrameCounter;
			World->Tick(LEVELTICK_All, Seconds);
		}

		void RegisterPlayers()
		{
			Mode->RegisterToy(ToyA);
			Mode->RegisterToy(ToyB);
		}

		~FMatchWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToonMatchLifecycle, "ToonStory.Match.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FToonMatchLifecycle::RunTest(const FString& Parameters)
{
	ToonMatchTests::FMatchWorld Test;
	TestFalse(TEXT("No zero-toy start"), Test.Mode->TryStartRound());
	TestFalse(TEXT("No deposit before start"), Test.Mode->TryRecordBatteryDeposit(Test.ToyA, FGuid::NewGuid()));
	Test.RegisterPlayers();
	TestFalse(TEXT("Duplicate registration"), Test.Mode->RegisterToy(Test.ToyA));
	Test.Mode->RoundDurationSeconds = 0;
	TestFalse(TEXT("Zero duration rejected"), Test.Mode->TryStartRound());
	Test.Mode->RoundDurationSeconds = 60;
	Test.Mode->RequiredBatteries = 0;
	TestFalse(TEXT("Zero battery goal rejected"), Test.Mode->TryStartRound());
	Test.Mode->RequiredBatteries = 7;
	TestTrue(TEXT("Start"), Test.Mode->TryStartRound());
	TestFalse(TEXT("Start is single-use"), Test.Mode->TryStartRound());
	TestFalse(TEXT("Roster locked while playing"), Test.Mode->UnregisterToy(Test.ToyA));
	TestTrue(TEXT("Remaining time"), Test.State->GetRemainingSeconds() > 0);
	TestTrue(TEXT("Abort"), Test.Mode->AbortRound());
	TestFalse(TEXT("No repeated end"), Test.Mode->AbortRound());
	TestEqual(TEXT("Abort has no winner"), Test.State->GetMatchStatus().WinningTeam, NAME_None);
	TestEqual(TEXT("Abort reason"), Test.State->GetMatchStatus().EndReason, FName(TEXT("Aborted")));
	TestEqual(TEXT("Finished timer"), Test.State->GetRemainingSeconds(), 0.0);
	TestFalse(TEXT("No restart without new world"), Test.Mode->TryStartRound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToonBatteryDeposits, "ToonStory.Match.DepositValidationAndWin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FToonBatteryDeposits::RunTest(const FString& Parameters)
{
	ToonMatchTests::FMatchWorld Test;
	Test.RegisterPlayers();
	Test.Mode->RequiredBatteries = 2;
	Test.Mode->TryStartRound();
	Test.Mode->RequiredBatteries = 99; // Active rules are frozen at start.
	const FGuid Battery = FGuid::NewGuid();
	APlayerState* Outsider = Test.World->SpawnActor<APlayerState>();
	TestFalse(TEXT("Unknown player"), Test.Mode->TryRecordBatteryDeposit(Outsider, Battery));
	TestFalse(TEXT("Invalid ID"), Test.Mode->TryRecordBatteryDeposit(Test.ToyA, FGuid()));
	TestTrue(TEXT("First deposit"), Test.Mode->TryRecordBatteryDeposit(Test.ToyA, Battery));
	TestFalse(TEXT("Same battery from another toy"), Test.Mode->TryRecordBatteryDeposit(Test.ToyB, Battery));
	TestEqual(TEXT("Count remains one"), Test.State->GetProgress().DepositedBatteries, 1);
	TestTrue(TEXT("Goal deposit"), Test.Mode->TryRecordBatteryDeposit(Test.ToyB, FGuid::NewGuid()));
	TestEqual(TEXT("Goal fixed at start"), Test.State->GetProgress().RequiredBatteries, 2);
	TestEqual(TEXT("Toy wins"), Test.State->GetMatchStatus().WinningTeam, FName(TEXT("Toy")));
	TestEqual(TEXT("Battery reason"), Test.State->GetMatchStatus().EndReason, FName(TEXT("BatteryGoal")));
	TestFalse(TEXT("No late deposits"), Test.Mode->TryRecordBatteryDeposit(Test.ToyA, FGuid::NewGuid()));
	TestFalse(TEXT("Capture cannot overwrite result"), Test.Mode->SetToyCaptured(Test.ToyA, true));
	TestEqual(TEXT("Final count"), Test.State->GetProgress().DepositedBatteries, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToonCaptureRescue, "ToonStory.Match.CaptureRescue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FToonCaptureRescue::RunTest(const FString& Parameters)
{
	ToonMatchTests::FMatchWorld Test;
	Test.RegisterPlayers();
	Test.Mode->TryStartRound();
	for (int32 Cycle = 0; Cycle < 4; ++Cycle)
	{
		TestTrue(TEXT("Capture"), Test.Mode->SetToyCaptured(Test.ToyA, true));
		TestFalse(TEXT("Duplicate capture"), Test.Mode->SetToyCaptured(Test.ToyA, true));
		TestEqual(TEXT("One captured"), Test.State->GetProgress().CapturedToys, 1);
		TestFalse(TEXT("Captured toy cannot deposit"), Test.Mode->TryRecordBatteryDeposit(Test.ToyA, FGuid::NewGuid()));
		TestTrue(TEXT("Rescue without elimination"), Test.Mode->SetToyCaptured(Test.ToyA, false));
		TestEqual(TEXT("Count restored"), Test.State->GetProgress().CapturedToys, 0);
	}
	Test.Mode->SetToyCaptured(Test.ToyA, true);
	Test.Mode->SetToyCaptured(Test.ToyB, true);
	TestEqual(TEXT("All captured"), Test.State->GetProgress().CapturedToys, 2);
	TestEqual(TEXT("Human wins"), Test.State->GetMatchStatus().WinningTeam, FName(TEXT("Human")));
	TestEqual(TEXT("Capture reason"), Test.State->GetMatchStatus().EndReason, FName(TEXT("AllCaptured")));
	TestFalse(TEXT("No rescue after round"), Test.Mode->SetToyCaptured(Test.ToyB, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToonDeadline, "ToonStory.Match.Deadline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FToonDeadline::RunTest(const FString& Parameters)
{
	ToonMatchTests::FMatchWorld Test;
	Test.RegisterPlayers();
	Test.Mode->RoundDurationSeconds = 0.25f;
	Test.Mode->RequiredBatteries = 1;
	Test.Mode->TryStartRound();
	Test.AdvanceFrame(0.25f);
	TestFalse(TEXT("Deadline deposit rejected"), Test.Mode->TryRecordBatteryDeposit(Test.ToyA, FGuid::NewGuid()));
	TestEqual(TEXT("Timeout winner"), Test.State->GetMatchStatus().WinningTeam, FName(TEXT("Human")));
	TestEqual(TEXT("Timeout reason"), Test.State->GetMatchStatus().EndReason, FName(TEXT("TimeExpired")));
	TestEqual(TEXT("No late count"), Test.State->GetProgress().DepositedBatteries, 0);
	{
		ToonMatchTests::FMatchWorld BeforeDeadline;
		BeforeDeadline.RegisterPlayers();
		BeforeDeadline.Mode->RequiredBatteries = 1;
		BeforeDeadline.Mode->RoundDurationSeconds = 1.0f;
		BeforeDeadline.Mode->TryStartRound();
		BeforeDeadline.AdvanceFrame(0.25f);
		TestTrue(TEXT("Pre-deadline goal accepted"), BeforeDeadline.Mode->TryRecordBatteryDeposit(BeforeDeadline.ToyA, FGuid::NewGuid()));
		BeforeDeadline.AdvanceFrame(1.0f);
		TestEqual(TEXT("Old timeout cannot overwrite a win"), BeforeDeadline.State->GetMatchStatus().WinningTeam, FName(TEXT("Toy")));
	}
	{
		ToonMatchTests::FMatchWorld TimerOnly;
		TimerOnly.RegisterPlayers();
		TimerOnly.Mode->RoundDurationSeconds = 0.25f;
		TimerOnly.Mode->TryStartRound();
		TimerOnly.AdvanceFrame(0.26f);
		TestEqual(TEXT("Timer finishes without a gameplay event"), TimerOnly.State->GetMatchStatus().Phase, EToonMatchPhase::Finished);
		TestEqual(TEXT("Timer winner"), TimerOnly.State->GetMatchStatus().WinningTeam, FName(TEXT("Human")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FToonInvalidParticipant, "ToonStory.Match.MissingParticipant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FToonInvalidParticipant::RunTest(const FString& Parameters)
{
	ToonMatchTests::FMatchWorld Test;
	Test.RegisterPlayers();
	Test.Mode->TryStartRound();
	Test.ToyB->Destroy();
	TestFalse(TEXT("Destroyed participant aborts processing"), Test.Mode->TryRecordBatteryDeposit(Test.ToyA, FGuid::NewGuid()));
	TestEqual(TEXT("No invented winner"), Test.State->GetMatchStatus().WinningTeam, NAME_None);
	TestEqual(TEXT("Abort reason"), Test.State->GetMatchStatus().EndReason, FName(TEXT("Aborted")));
	return true;
}

#endif
