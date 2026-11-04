#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Events/TBMomArrivalEvent.h"
#include "GamePlay/Events/TBMomArrivalZone.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameHelpers.h"
#include "Character/TBCharacter.h"
#include "Character/TBNPCController.h"
#include "Tests/TBEventTestEvent.h"
#include "Data/TBRuleData.h"
#include "Tests/TBTestMatch.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMomEventTest,"ToonStory.Events.MomArrival",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMomEventTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::Game) World=Context.World();
	if (!World || World->GetNetMode()!=NM_Standalone) return false;
	auto* Match=World->GetGameState<ATBGameState>();
	if (!Match) return false;
	TBTest::FPlayingScope Playing(*Match,World->GetTimeSeconds()+600);
	auto& Rules=const_cast<UTBRuleData&>(TB::Rules()).Events.Mom;
	const auto OldRules=Rules;
	const auto OldWinner=Match->Winner; const FString OldReason=Match->Reason;
	TArray<TPair<ATBPlayerState*,ETBTeam>> Players;
	for (auto It=World->GetPlayerControllerIterator();It;++It)
		if (auto* PS=It->Get()->GetPlayerState<ATBPlayerState>()) { Players.Emplace(PS,PS->Team); PS->Team=ETBTeam::Toy; }
	TArray<TPair<ATBMomArrivalZone*,bool>> Existing;
	for (TActorIterator<ATBMomArrivalZone> It(World);It;++It) { Existing.Emplace(*It,It->bEnabled); It->bEnabled=false; }
	auto* Event=NewObject<UTBMomArrivalEvent>();
	TestFalse(TEXT("No enabled zone means event is ineligible"),Event->CanExecute(World));
	auto* Zone=World->SpawnActor<ATBMomArrivalZone>(FVector(0,0,10000),FRotator::ZeroRotator);
	auto* OtherZone=World->SpawnActor<ATBMomArrivalZone>(FVector(1000,0,10000),FRotator::ZeroRotator);
	TArray<ATBController*> Controllers;
	TArray<ATBCharacter*> Humans;
	for (int32 I=0;I<2;++I)
	{
		auto* PC=World->SpawnActor<ATBController>();
		auto* PS=PC->GetPlayerState<ATBPlayerState>();
		if (!PS) { PS=World->SpawnActor<ATBPlayerState>(); PC->PlayerState=PS; Match->AddPlayerState(PS); }
		PS->Team=ETBTeam::Human;
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Human=World->SpawnActor<ATBCharacter>(FVector(500,0,10000),FRotator::ZeroRotator,Params);
		PC->Possess(Human); Human->ApplyState();
		Controllers.Add(PC); Humans.Add(Human);
	}
	ON_SCOPE_EXIT {
		Zone->Destroy(); OtherZone->Destroy(); Rules=OldRules; Match->Winner=OldWinner; Match->Reason=OldReason;
		for (int32 I=0;I<2;++I) {
			auto* PS=Controllers[I]->GetPlayerState<ATBPlayerState>(); Controllers[I]->UnPossess(); Humans[I]->Destroy();
			Match->RemovePlayerState(PS); PS->Destroy(); Controllers[I]->Destroy();
		}
		for (const auto& Pair:Existing) Pair.Key->bEnabled=Pair.Value;
		for (const auto& Pair:Players) Pair.Key->Team=Pair.Value;
	};
	auto Move=[&](int32 Index,float X,float Height=0) {
		Humans[Index]->SetActorLocation(Zone->GetActorLocation()+FVector(X,0,Humans[Index]->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+Height));
	};
	Rules.ArrivalSeconds=10; Rules.PersuadeSeconds=3; Rules.Radius=90;
	Match->Phase=ETBPhase::Lobby;
	TestFalse(TEXT("Lobby cannot start event"),Zone->StartEvent());
	Playing.Hold(); Match->HidePreparation.bHiding=true;
	TestFalse(TEXT("Preparation cannot start event"),Event->Execute(World));
	Match->HidePreparation.bHiding=false;
	// NPCだけの人間陣営と、途中切断によりNPCだけになった状況を再現する。
	auto* NPC=World->SpawnActor<ATBNPCController>();
	NPC->SetActorTickEnabled(false);
	ON_SCOPE_EXIT { NPC->UnPossess(); NPC->Destroy(); };
	Controllers[0]->UnPossess(); NPC->Possess(Humans[0]);
	Humans[1]->TBPS()->Team=ETBTeam::Toy;
	NPC->GetPlayerState<ATBPlayerState>()->Team=ETBTeam::Human;
	TestFalse(TEXT("NPC-only human team is ineligible"),Event->CanExecute(World));
	TestFalse(TEXT("Direct zone start also rejects NPC-only team"),Zone->StartEvent());
	NPC->UnPossess(); Controllers[0]->Possess(Humans[0]);
	Humans[1]->TBPS()->Team=ETBTeam::Human;
	TestTrue(TEXT("Configured event is eligible"),Event->CanExecute(World));
	TestTrue(TEXT("Server starts event"),Zone->StartEvent());
	TestFalse(TEXT("No second simultaneous mom event"),OtherZone->StartEvent());
	Zone->Advance(2); TestEqual(TEXT("Empty zone counts down"),Zone->State.Remaining,8.f);
	Move(0,0); Zone->Advance(1);
	TestEqual(TEXT("One human pauses arrival"),Zone->State.Remaining,8.f);
	TestEqual(TEXT("One human advances persuasion"),Zone->State.Held,1.f);
	Move(1,50); Zone->Advance(.5f);
	TestEqual(TEXT("Two humans do not double progress"),Zone->State.Held,1.5f);
	Move(0,500); Zone->Advance(.5f);
	TestEqual(TEXT("Another human maintains progress"),Zone->State.Held,2.f);
	Move(1,500); Zone->Advance(1);
	TestEqual(TEXT("Leaving resumes arrival"),Zone->State.Remaining,7.f);
	TestEqual(TEXT("Everyone leaving resets persuasion"),Zone->State.Held,0.f);
	Move(0,0,Rules.Height+10); Zone->Advance(1);
	TestEqual(TEXT("Different floor does not pause arrival"),Zone->State.Remaining,6.f);
	Move(0,0); Humans[0]->TBPS()->Team=ETBTeam::Toy; Zone->Advance(1);
	TestEqual(TEXT("Toys cannot persuade"),Zone->State.Remaining,5.f);
	Humans[0]->TBPS()->Team=ETBTeam::Human; Zone->Advance(3);
	TestFalse(TEXT("Continuous stay resolves event"),Zone->State.bActive);
	TestEqual(TEXT("Success keeps match playing"),Match->Phase,ETBPhase::Playing);
	Zone->StartEvent();
	Controllers[0]->UnPossess(); Controllers[1]->UnPossess(); NPC->Possess(Humans[0]);
	Zone->Advance(100);
	TestFalse(TEXT("Last human player leaving cancels active event"),Zone->State.bActive);
	TestEqual(TEXT("NPC-only cancellation never awards timeout victory"),Match->Phase,ETBPhase::Playing);
	NPC->UnPossess(); Controllers[0]->Possess(Humans[0]); Controllers[1]->Possess(Humans[1]);
	Move(0,500); Zone->StartEvent(); Zone->Advance(10);
	TestEqual(TEXT("Deadline forces human defeat"),Match->Winner,ETBWinner::Toys);
	TestEqual(TEXT("Deadline ends round"),Match->Phase,ETBPhase::Results);
	Playing.Hold(); Zone->StartEvent(); ++Match->HidePreparation.Round; Zone->Advance(1);
	TestFalse(TEXT("Previous round never carries event"),Zone->State.bActive);
	Zone->StartEvent(); Match->Phase=ETBPhase::Lobby; Zone->Advance(1);
	TestFalse(TEXT("Returning to lobby clears event"),Zone->State.bActive);
	Playing.Hold(); Rules.PersuadeSeconds=0;
	TestFalse(TEXT("Invalid DB value rejected"),Zone->StartEvent());
	Rules.PersuadeSeconds=3;
	// 実際のママイベントが共通の継続イベント枠を保持することを検証。
	auto& Events=const_cast<UTBRuleData&>(TB::Rules()).Events;
	const auto SavedEvents=Events;
	auto* Owner=World->SpawnActor<AActor>();
	auto* Director=NewObject<UTBEventDirector>(Owner);
	Owner->AddInstanceComponent(Director); Director->RegisterComponent();
	const int32 Executions=UTBEventTestEvent::Executions;
	ON_SCOPE_EXIT { Events=SavedEvents; UTBEventTestEvent::Executions=Executions; Owner->Destroy(); };
	Events.bEnabled=true; Events.CooldownSeconds=3; Events.ChancePerSecond=1;
	Events.EventClasses={UTBMomArrivalEvent::StaticClass(),UTBEventTestEvent::StaticClass()};
	TestTrue(TEXT("Director starts mom event"),Director->TryTrigger(UTBMomArrivalEvent::StaticClass()));
	Director->Advance(100);
	TestEqual(TEXT("Mom blocks other natural event rolls"),UTBEventTestEvent::Executions,Executions);
	TestFalse(TEXT("Mom blocks explicit other-event trigger"),Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	auto* Active=ATBMomArrivalZone::FindActive(World);
	if (TestNotNull(TEXT("Mom stays active until zone resolves"),Active)) Active->ResetEvent();
	TestFalse(TEXT("Mom completion starts shared cooldown"),Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	Events.ChancePerSecond=0;
	Director->Advance(3);
	TestTrue(TEXT("Other event resumes after mom cooldown"),Director->TryTrigger(UTBEventTestEvent::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMomZoneAssetTest,"ToonStory.Events.MomAsset",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMomZoneAssetTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::Game) World=Context.World();
	if (!World) return false;
	TestTrue(TEXT("Mom event registered in shared event DB"),TB::Rules().Events.EventClasses.Contains(UTBMomArrivalEvent::StaticClass()));
	auto* Class=LoadClass<ATBMomArrivalZone>(nullptr,TEXT("/Game/Events/Mom/BP_MomArrivalZone.BP_MomArrivalZone_C"));
	if (!TestNotNull(TEXT("Drag-and-drop zone Blueprint exists"),Class)) return false;
	auto* Zone=World->SpawnActor<ATBMomArrivalZone>(Class,FVector(0,0,10000),FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Zone can spawn without editing a map"),Zone)) return false;
	ON_SCOPE_EXIT { Zone->Destroy(); };
	if (TestNotNull(TEXT("Blueprint supplies red material"),Zone->BoundaryMaterial.Get()))
		TestTrue(TEXT("Boundary supports instanced rendering"),Zone->BoundaryMaterial->GetMaterial()->bUsedWithInstancedStaticMeshes);
	for (auto* Part : TInlineComponentArray<UStaticMeshComponent*>(Zone))
		if (Part->GetName() == TEXT("PlaceholderCube"))
		{
			TestTrue(TEXT("Editor marker stays hidden in game"), Part->bHiddenInGame);
			TestFalse(TEXT("Hidden marker cannot cast a cube shadow"), Part->CastShadow);
		}
	auto* Boundary=Zone->FindComponentByClass<UInstancedStaticMeshComponent>();
	if (TestNotNull(TEXT("Blueprint retains dashed boundary"),Boundary))
	{
		TestEqual(TEXT("Boundary has all dashes"),Boundary->GetInstanceCount(),48);
		TestFalse(TEXT("Boundary is hidden before event"),Boundary->IsVisible());
		TestEqual(TEXT("Boundary never blocks players"),Boundary->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
	}
	return true;
}
#endif
