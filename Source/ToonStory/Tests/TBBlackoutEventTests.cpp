#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/TBTestMatch.h"
#include "GamePlay/Events/TBBlackoutEvent.h"
#include "GamePlay/Events/TBEventDirector.h"
#include "GamePlay/TBRoomLighting.h"
#include "Core/TBGameState.h"
#include "Data/TBRuleData.h"
#include "Components/LightComponent.h"
#include "Engine/Engine.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBBlackoutEventTest, "ToonStory.Events.Blackout",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBBlackoutEventTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Entry : GEngine->GetWorldContexts())
		if (Entry.WorldType == EWorldType::Game && Entry.World() && Entry.World()->GetGameState<ATBGameState>())
		{ World = Entry.World(); break; }
	if (!TestNotNull(TEXT("Standalone world"), World) || World->GetNetMode() != NM_Standalone) return false;
	auto* State = World->GetGameState<ATBGameState>();
	auto& Rules = const_cast<UTBRuleData&>(TB::Rules()).Events;
	TestTrue(TEXT("Shared DA registers blackout"), Rules.EventClasses.Contains(UTBBlackoutEvent::StaticClass()));
	const auto OldRules = Rules;
	const auto OldPhase = State->Phase;
	TMap<ATBRoomLighting*, bool> OldLights;
	TArray<ATBRoomLighting*> Rooms;
	for (TActorIterator<ATBRoomLighting> It(World); It; ++It)
	{
		Rooms.Add(*It);
		OldLights.Add(*It, It->bLightsOn);
	}
	auto* Owner = World->SpawnActor<AActor>();
	auto* Director = NewObject<UTBEventDirector>(Owner);
	Owner->AddInstanceComponent(Director);
	Director->RegisterComponent();
	auto* Event = NewObject<UTBBlackoutEvent>(Owner);
	auto* EmptyRoom = World->SpawnActor<ATBRoomLighting>();
	ON_SCOPE_EXIT
	{
		for (const auto& Pair : OldLights) Pair.Key->SetLightsOn(Pair.Value);
		Rules = OldRules;
		State->Phase = OldPhase;
		EmptyRoom->Destroy();
		Owner->Destroy();
	};
	if (!TestTrue(TEXT("Authored map has at least two rooms"), Rooms.Num() >= 2)) return false;
	TBTest::FPlayingScope Playing(*State);
	for (auto* Room : Rooms) Room->SetLightsOn(false);
	TestFalse(TEXT("Empty room and dark rooms are ineligible"), Event->CanExecute(World));
	TestFalse(TEXT("No target reports failure"), Event->Execute(World));
	Rooms[0]->SetLightsOn(true);
	Rooms[1]->SetLightsOn(true);
	Rules.bEnabled = true;
	Rules.ChancePerSecond = 1;
	Rules.CooldownSeconds = 3;
	Rules.EventClasses = {UTBBlackoutEvent::StaticClass()};
	Director->Advance(1);
	int32 LitCount = 0;
	for (auto* Room : Rooms)
	{
		LitCount += Room->bLightsOn ? 1 : 0;
		for (ALight* Light : Room->Lights)
			if (Light) TestEqual(TEXT("Actual lights match room state"), Light->GetLightComponent()->IsVisible(), Room->bLightsOn);
	}
	TestEqual(TEXT("Random event turns off exactly one lit room"), LitCount, 1);
	TestTrue(TEXT("Unconfigured room remains untouched"), EmptyRoom->bLightsOn);
	TestFalse(TEXT("Blackout shares global cooldown"), Director->TryTrigger(UTBBlackoutEvent::StaticClass()));
	Director->Advance(3);
	TestFalse(TEXT("Remaining room goes dark"), Event->CanExecute(World));
	Director->Advance(3);
	TestFalse(TEXT("All-dark event fails"), Director->TryTrigger(UTBBlackoutEvent::StaticClass()));
	Rooms[0]->SetLightsOn(true);
	TestTrue(TEXT("Relit room becomes eligible without failed-attempt cooldown"), Director->TryTrigger(UTBBlackoutEvent::StaticClass()));
	State->Phase = ETBPhase::Lobby;
	Rooms[0]->SetLightsOn(true);
	TestFalse(TEXT("Direct execution outside match rejected"), Event->Execute(World));
	TestTrue(TEXT("Rejected execution preserves light"), Rooms[0]->bLightsOn);
	return true;
}
#endif
