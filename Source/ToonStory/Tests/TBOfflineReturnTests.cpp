// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBSession.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBOfflineReturnTest, "ToonStory.Session.OfflineReturnRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBOfflineReturnTest::RunTest(const FString&)
{
	// Isolate timers/delegates from the running game's session. Never tick the OpenLevel timer.
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	TStrongObjectPtr<UTBSession> Session(NewObject<UTBSession>(Instance.Get()));
	TStrongObjectPtr<UWorld> Title(NewObject<UWorld>(CreatePackage(TEXT("/Temp/TBOfflineReturnTestTitle"))));
	Title->SetGameInstance(Instance.Get());
	if (!TestNotNull(TEXT("Engine context available"), GEngine)) return false;
	auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(Title.Get());
	bool Deinitialized = false;
	ON_SCOPE_EXIT
	{
		if (!Deinitialized) Session->Deinitialize();
		GEngine->DestroyWorldContext(Title.Get());
	};
	auto& Timers = Instance->GetTimerManager();
	auto CheckStopped = [&]()
	{
		TestFalse(TEXT("Busy released"), Session->IsBusy());
		TestFalse(TEXT("Offline return guard released"), Session->bReturningOffline);
		TestFalse(TEXT("Watch removed"), Session->OfflineReturnWatch.IsValid());
		TestEqual(TEXT("Deadline removed"), Session->OfflineReturnDeadline, 0.0);
		TestFalse(TEXT("No delayed OpenLevel remains"), Timers.TimerExists(Session->CloseTimer));
	};

	const double BeforeReturn = FPlatformTime::Seconds();
	Session->ReturnOffline();
	TestTrue(TEXT("Deadline is bounded to thirty seconds from request"),
	    Session->OfflineReturnDeadline >= BeforeReturn + 30.0 &&
	    Session->OfflineReturnDeadline <= FPlatformTime::Seconds() + 30.0);
	TestTrue(TEXT("Return remains asynchronous"), Session->IsBusy() && Session->bReturningOffline);
	TestTrue(TEXT("Deferred OpenLevel scheduled"), Timers.TimerExists(Session->CloseTimer));
	TestTrue(TEXT("Callback-loss watch active"), Session->OfflineReturnWatch.IsValid());
	const double FirstDeadline = Session->OfflineReturnDeadline;
	Session->ReturnOffline();
	TestEqual(TEXT("Duplicate return does not extend timeout"), Session->OfflineReturnDeadline, FirstDeadline);
	Session->TravelFailed(Title.Get(), ETravelFailure::LoadMapFailure, TEXT("Injected missing Title package"));
	CheckStopped();
	TestTrue(TEXT("Failure preserves diagnostic and retry guidance"),
	    Session->Status.Contains(TEXT("Injected missing Title package")) && Session->Status.Contains(TEXT("再試行")));
	const FString FailureStatus = Session->Status;
	Session->TravelFailed(Title.Get(), ETravelFailure::ClientTravelFailure, TEXT("Second failure in the same Browse"));
	Session->Destroyed(NAME_GameSession, true);
	const bool WasRoomClosed = Session->bRoomClosed;
	const bool WasKicked = Session->bKicked;
	Session->RoomClosed();
	Session->Kicked();
	CheckStopped();
	TestEqual(TEXT("Delayed close does not change disconnect reason"), Session->bRoomClosed, WasRoomClosed);
	TestEqual(TEXT("Delayed kick does not change disconnect reason"), Session->bKicked, WasKicked);
	TestEqual(TEXT("Duplicate/late callback cannot restart travel or replace reason"), Session->Status, FailureStatus);
	// Model the engine listener running after the session's failure listener.
	Context.TravelURL = TEXT("?closed");
	Title->NextURL = TEXT("?closed");
	Session->FinishTravelRecovery();
	TestTrue(TEXT("Engine fallback disconnect removed before next frame"), Context.TravelURL.IsEmpty() && Title->NextURL.IsEmpty());
	TestFalse(TEXT("Same-frame suppression ends with recovery"), Session->bOfflineReturnFailurePending);

	Session->ReturnOffline();
	TestTrue(TEXT("Retry accepted after failure"), Session->IsBusy() && Timers.TimerExists(Session->CloseTimer));
	TestTrue(TEXT("Retry gets a fresh watch"), Session->OfflineReturnWatch.IsValid());
	Session->OfflineMapLoaded(nullptr);
	TestTrue(TEXT("Unrelated/null load does not complete the retry"), Session->IsBusy());
	TStrongObjectPtr<UWorld> OtherMap(NewObject<UWorld>(CreatePackage(TEXT("/Temp/TBOfflineReturnOtherMap"))));
	OtherMap->SetGameInstance(Instance.Get());
	Session->OfflineMapLoaded(OtherMap.Get());
	TestTrue(TEXT("Non-Title map does not complete return"), Session->IsBusy());
	TStrongObjectPtr<UGameInstance> OtherInstance(NewObject<UGameInstance>());
	Title->SetGameInstance(OtherInstance.Get());
	Session->OfflineMapLoaded(Title.Get());
	TestTrue(TEXT("Another game instance's Title does not complete return"), Session->IsBusy());
	Title->SetGameInstance(Instance.Get());
	// Exercise the exact handler registered with PostLoadMapWithWorld without broadcasting globally.
	Session->OfflineMapLoaded(Title.Get());
	CheckStopped();
	const FString SuccessStatus = Session->Status;
	TestFalse(TEXT("Late watch cannot change success"), Session->TickOfflineReturn(0));
	TestEqual(TEXT("Success reason retained"), Session->Status, SuccessStatus);

	Session->ReturnOffline();
	TestTrue(TEXT("Before deadline stays busy"), Session->TickOfflineReturn(0) && Session->IsBusy());
	// Inject elapsed wall time: no sleeps and no world/timer ticks or real map changes.
	Session->OfflineReturnDeadline = FPlatformTime::Seconds() - 1.0;
	TestFalse(TEXT("Missing callback has finite timeout"), Session->TickOfflineReturn(0));
	CheckStopped();
	TestTrue(TEXT("Timeout has a clear reason"), Session->Status.Contains(TEXT("30秒")));
	Session->FinishTravelRecovery();
	Session->ReturnOffline();
	TestTrue(TEXT("Retry accepted after timeout"), Session->IsBusy() && Session->OfflineReturnWatch.IsValid());
	Session->Deinitialize();
	Deinitialized = true;
	TestFalse(TEXT("Deinitialize removes watch"), Session->OfflineReturnWatch.IsValid());
	TestEqual(TEXT("Deinitialize clears deadline"), Session->OfflineReturnDeadline, 0.0);
	TestFalse(TEXT("Deinitialize cancels delayed travel"), Timers.TimerExists(Session->CloseTimer));
	return true;
}
#endif
