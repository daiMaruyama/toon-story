#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Events/TBMomArrivalZone.h"
#include "GamePlay/Events/TBMomArrivalEvent.h"
#include "Character/TBCharacter.h"
#include "Character/TBCharacterGeometry.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBController.h"
#include "Data/TBRuleData.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tests/TBTestMatch.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBAuthoredMomTest, "ToonStory.Events.AuthoredMom",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBAuthoredMomTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World || (!World->GetMapName().Contains(TEXT("TB_StylizedHouse")) &&
	    !World->GetMapName().Contains(TEXT("TB_ToyWorldDemoExpanded"))))
	{
		AddError(TEXT("Run on an authored House or Demo stage")); return false;
	}
	auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
	auto* Human = PC ? PC->GetPawn<ATBCharacter>() : nullptr;
	auto* Info = PC ? PC->GetPlayerState<ATBPlayerState>() : nullptr;
	auto* Match = World->GetGameState<ATBGameState>();
	if (!Human || !Info || !Match || World->GetNetMode() != NM_Standalone) return false;
	const auto OldTeam = Info->Team; const auto OldTransform = Human->GetActorTransform();
	TBTest::FPlayingScope Playing(*Match, World->GetTimeSeconds() + 600);
	ON_SCOPE_EXIT { Info->Team = OldTeam; Human->SetActorTransform(OldTransform); Human->ApplyState(); };
	Info->Team = ETBTeam::Human; Human->ApplyState();
	const auto& Rules = TB::Rules().Events;
	TestTrue(TEXT("Random events enabled and mom registered"), Rules.bEnabled && Rules.ChancePerSecond > 0 &&
	    Rules.EventClasses.Contains(UTBMomArrivalEvent::StaticClass()));
	int32 Count = 0;
	for (TActorIterator<ATBMomArrivalZone> It(World); It; ++It)
	{
		auto* Zone = *It; ++Count;
		TestTrue(TEXT("Authored mom candidate is enabled"), Zone->bEnabled);
		TestNotNull(TEXT("Authored zone has boundary material"), Zone->BoundaryMaterial.Get());
		FCollisionQueryParams Query(SCENE_QUERY_STAT(AuthoredMom), false, Human); Query.AddIgnoredActor(Zone);
		const FVector Origin = Zone->GetActorLocation();
		for (FVector Offset : {FVector::ZeroVector, FVector(45,0,0), FVector(-45,0,0), FVector(0,45,0), FVector(0,-45,0)})
		{
			FHitResult Floor;
			if (TestTrue(TEXT("Persuasion area has floor"), World->LineTraceSingleByChannel(Floor,
			    Origin+Offset+FVector(0,0,80), Origin+Offset-FVector(0,0,100), ECC_Visibility, Query)))
			{
				TestTrue(TEXT("Human can stand in persuasion area"), TB::SupportsCapsule(World,
				    Floor.ImpactPoint+FVector(0,0,TBCharacterGeometry::HumanHalfHeight+2.15),
				    TBCharacterGeometry::HumanRadius, TBCharacterGeometry::HumanHalfHeight, ECC_Pawn, Query));
			}
		}
		if (!TestTrue(TEXT("Authored event starts with a human player"), Zone->StartEvent())) continue;
		Human->SetActorLocation(Origin+FVector(0,0,Human->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.15));
		TestTrue(TEXT("Standing in authored area counts as persuasion"), Zone->ContainsHuman(*Human));
		Zone->Advance(.5f);
		TestTrue(TEXT("Authored area advances persuasion"), Zone->State.bOccupied && Zone->State.Held > 0);
		Zone->ResetEvent();
	}
	TestEqual(TEXT("One authored mom zone per stage"), Count, 1);
	return true;
}
#endif
