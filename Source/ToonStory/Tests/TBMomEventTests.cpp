#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Events/TBMomArrivalEvent.h"
#include "GamePlay/Events/TBMomArrivalZone.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameHelpers.h"
#include "Character/TBCharacter.h"
#include "Character/TBCharacterGeometry.h"
#include "Data/TBRuleData.h"
#include "Tests/TBTestMatch.h"
#include "Components/CapsuleComponent.h"
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
	Move(0,500); Zone->StartEvent(); Zone->Advance(10);
	TestEqual(TEXT("Deadline forces human defeat"),Match->Winner,ETBWinner::Toys);
	TestEqual(TEXT("Deadline ends round"),Match->Phase,ETBPhase::Results);
	Playing.Hold(); Zone->StartEvent(); ++Match->HidePreparation.Round; Zone->Advance(1);
	TestFalse(TEXT("Previous round never carries event"),Zone->State.bActive);
	Zone->StartEvent(); Match->Phase=ETBPhase::Lobby; Zone->Advance(1);
	TestFalse(TEXT("Returning to lobby clears event"),Zone->State.bActive);
	Playing.Hold(); Rules.PersuadeSeconds=0;
	TestFalse(TEXT("Invalid DB value rejected"),Zone->StartEvent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMomZonePlacementTest,"ToonStory.Events.MomPlacement",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMomZonePlacementTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for (const auto& Context:GEngine->GetWorldContexts()) if (Context.WorldType==EWorldType::Game) World=Context.World();
	if (!World) return false;
	TestTrue(TEXT("Mom event registered in shared event DB"),TB::Rules().Events.EventClasses.Contains(UTBMomArrivalEvent::StaticClass()));
	int32 Count=0;
	for (TActorIterator<ATBMomArrivalZone> It(World);It;++It)
	{
		++Count;
		if (TestNotNull(TEXT("Red boundary material saved"),It->BoundaryMaterial.Get()))
			TestTrue(TEXT("Boundary supports instanced rendering in game"),It->BoundaryMaterial->GetMaterial()->bUsedWithInstancedStaticMeshes);
		const FVector Center=It->GetActorLocation();
		FCollisionQueryParams Query(NAME_None,false,*It);
		TestTrue(TEXT("Human can stand at zone center"),TB::SupportsCapsule(World,Center+FVector(0,0,TBCharacterGeometry::HumanHalfHeight+2),
		    TBCharacterGeometry::HumanRadius,TBCharacterGeometry::HumanHalfHeight,ECC_Pawn,Query));
		for (int32 Sample=0;Sample<8;++Sample)
		{
			const float Angle=2*PI*Sample/8;
			const FVector P=Center+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*TB::Rules().Events.Mom.Radius;
			FHitResult Floor;
			TestTrue(TEXT("Boundary stays over stage floor"),World->LineTraceSingleByChannel(Floor,P+FVector(0,0,10),P-FVector(0,0,20),ECC_WorldStatic,Query) && Floor.ImpactNormal.Z>.7f);
		}
	}
	TestEqual(TEXT("One authored persuasion zone"),Count,1);
	return true;
}
#endif
