#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "TBEventTestEvent.h"
#include "Core/TBGameState.h"
#include "Data/TBRuleData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBEventTest, "ToonStory.Events.Scheduling",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBEventTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Entry : GEngine->GetWorldContexts())
		if (Entry.WorldType == EWorldType::Game && Entry.World() && Entry.World()->GetGameState<ATBGameState>())
		{ World = Entry.World(); break; }
	if (!TestNotNull(TEXT("Standalone game world"), World) || World->GetNetMode() != NM_Standalone) return false;
	auto* State = World->GetGameState<ATBGameState>();
	auto& Rules = const_cast<UTBRuleData&>(TB::Rules()).Events;
	const auto SavedRules = Rules;
	const auto SavedPhase = State->Phase;
	auto* Owner = World->SpawnActor<AActor>();
	auto* Director = NewObject<UTBEventDirector>(Owner);
	Owner->AddInstanceComponent(Director);
	Director->RegisterComponent();
	ON_SCOPE_EXIT
	{
		Rules = SavedRules;
		State->Phase = SavedPhase;
		UTBEventTestEvent::ReentrantDirector = nullptr;
		UTBEventTestEvent::bEligible = UTBEventTestEvent::bSucceed = true;
		Owner->Destroy();
	};
	Rules = FTBEventRules();
	Rules.ChancePerSecond = 1;
	Rules.CooldownSeconds = 3;
	Rules.EventClasses = {UTBEventTestEvent::StaticClass(), UTBEventTestEvent::StaticClass()};
	UTBEventTestEvent::Executions = 0;
	State->Phase = ETBPhase::Lobby;
	Director->Advance(10);
	TestFalse(TEXT("Lobby rejects explicit trigger"), Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	TestEqual(TEXT("Lobby does not roll"), UTBEventTestEvent::Executions, 0);
	State->Phase = ETBPhase::Playing;
	Director->Advance(.5f);
	TestEqual(TEXT("No roll before one second"), UTBEventTestEvent::Executions, 0);
	Director->Advance(.5f);
	TestEqual(TEXT("One event despite duplicate registration"), UTBEventTestEvent::Executions, 1);
	TestFalse(TEXT("Explicit triggers share cooldown"), Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	Director->Advance(2);
	TestEqual(TEXT("Cooldown blocks rolls"), UTBEventTestEvent::Executions, 1);
	Director->Advance(1);
	TestEqual(TEXT("Cooldown boundary allows roll"), UTBEventTestEvent::Executions, 2);
	Director->Advance(100);
	TestEqual(TEXT("Hitch never bursts"), UTBEventTestEvent::Executions, 3);
	State->Phase = ETBPhase::Results;
	Director->Advance(1);
	TestFalse(TEXT("Results reject events"), Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	State->Phase = ETBPhase::Playing;
	UTBEventTestEvent::bEligible = false;
	Director->Advance(1);
	TestEqual(TEXT("Ineligible candidates do not execute"), UTBEventTestEvent::Executions, 3);
	UTBEventTestEvent::bEligible = true;
	UTBEventTestEvent::bSucceed = false;
	TestFalse(TEXT("Failed effect reports failure"), Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	UTBEventTestEvent::bSucceed = true;
	UTBEventTestEvent::ReentrantDirector = Director;
	TestTrue(TEXT("Failure consumes no cooldown"), Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	TestFalse(TEXT("Recursive trigger rejected"), UTBEventTestEvent::bNestedSucceeded);
	UTBEventTestEvent::ReentrantDirector = nullptr;
	Rules.ChancePerSecond = 0;
	Director->Advance(100);
	TestEqual(TEXT("Zero chance never fires"), UTBEventTestEvent::Executions, 5);
	Rules.bEnabled = false;
	TestFalse(TEXT("Disabled system rejects explicit events"), Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	Rules.bEnabled = true;
	Director->ResetRound();
	Rules.EventClasses.Reset();
	TestFalse(TEXT("Unregistered class rejected"), Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	Director->Advance(100);
	TestEqual(TEXT("Empty catalog is inert"), UTBEventTestEvent::Executions, 5);
	return true;
}
#endif
