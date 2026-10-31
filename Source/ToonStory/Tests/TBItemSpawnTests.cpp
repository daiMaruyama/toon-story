#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Items/TBItemSpawnDirector.h"
#include "GamePlay/Items/TBItemSpawnPoint.h"
#include "GamePlay/Items/TBNoiseRadio.h"
#include "GamePlay/TBLightSwitch.h"
#include "GamePlay/TBRoomLighting.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Data/TBRuleData.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"
#include "Tests/TBTestMatch.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBItemSpawnTest,"ToonStory.Items.RandomSpawns",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBItemSpawnTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game) World=Context.World();
	if(!World || World->GetNetMode()!=NM_Standalone) return false;
	auto* Mode=World->GetAuthGameMode<ATBGameMode>(); auto* State=World->GetGameState<ATBGameState>();
	auto* Director=Mode ? Mode->FindComponentByClass<UTBItemSpawnDirector>() : nullptr;
	if(!TestNotNull(TEXT("GameMode owns random item director"),Director) || !State) return false;
	auto& Rules=const_cast<UTBRuleData&>(TB::Rules()).RandomItems;
	const auto OldRules=Rules;
	TBTest::FPlayingScope Playing(*State,World->GetTimeSeconds()+600);
	TArray<AActor*> Fixtures;
	TArray<ATBItemSpawnPoint*> Points;
	TArray<TPair<ATBItemSpawnPoint*,bool>> ExistingPoints;
	for(TActorIterator<ATBItemSpawnPoint> It(World);It;++It) { ExistingPoints.Emplace(*It,It->bEnabled); It->bEnabled=false; }
	Director->ResetRound();
	auto SpawnBox=[&](FVector Position,FVector Extent) {
		auto* Actor=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box); Box->SetBoxExtent(Extent); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
		Actor->SetActorLocation(Position); Fixtures.Add(Actor); return Actor;
	};
	SpawnBox(FVector(0,0,9990),FVector(1000,500,10));
	auto* Room=World->SpawnActor<ATBRoomLighting>(FVector(0,0,10070),FRotator::ZeroRotator); Fixtures.Add(Room);
	Room->Bounds->SetBoxExtent(FVector(1000,500,100));
	for(int32 Index=0;Index<5;++Index)
	{
		auto* Point=World->SpawnActor<ATBItemSpawnPoint>(FVector(-600+300*Index,0,10003),FRotator::ZeroRotator);
		Point->PlacementType=TEXT("Wall"); Point->Room=Room; Points.Add(Point); Fixtures.Add(Point);
	}
	auto* HandPlaced=World->SpawnActor<ATBNoiseRadio>(FVector(0,700,10003),FRotator::ZeroRotator); Fixtures.Add(HandPlaced);
	ON_SCOPE_EXIT {
		Director->ResetRound(); Rules=OldRules;
		for(auto* Actor:Fixtures) if(IsValid(Actor)) Actor->Destroy();
		for(const auto& Point:ExistingPoints) if(IsValid(Point.Key)) Point.Key->bEnabled=Point.Value;
	};
	Rules=FTBItemSpawnRules(); Rules.SpawnCount=3;
	FTBItemSpawnEntry Radio; Radio.ItemClass=ATBNoiseRadio::StaticClass(); Radio.Placements.Add(TEXT("Wall"),FTransform(FVector(40,0,0)));
	FTBItemSpawnEntry Switch; Switch.ItemClass=ATBLightSwitch::StaticClass(); Switch.Placements.Add(TEXT("Wall"),FTransform(FVector(0,0,70)));
	Rules.Entries={Radio,Switch};
	State->Phase=ETBPhase::Lobby; Director->StartRound(); TestEqual(TEXT("Lobby never spawns"),Director->GetSpawnedCount(),0);
	Playing.Hold();
	bool SawRadio=false,SawSwitch=false;
	TSet<int32> UsedPoints;
	for(int32 Round=0;Round<20;++Round)
	{
		Director->StartRound();
		TestEqual(TEXT("Configured item count per round"),Director->GetSpawnedCount(),3);
		Director->StartRound(); TestEqual(TEXT("Repeated start does not duplicate"),Director->GetSpawnedCount(),3);
		TArray<FVector> Locations;
		for(TActorIterator<ATBItemBase> It(World);It;++It)
		{
			if(*It==HandPlaced || It->IsActorBeingDestroyed() || It->GetActorLocation().Z<9000) continue;
			Locations.Add(It->GetActorLocation());
			TestTrue(TEXT("Spawned items are replicated"),It->GetIsReplicated());
			if(auto* LightSwitch=Cast<ATBLightSwitch>(*It)) { SawSwitch=true; TestEqual(TEXT("Dynamic switch has the point's room"),LightSwitch->Room.Get(),Room); }
			if(It->IsA<ATBNoiseRadio>()) SawRadio=true;
			for(int32 Index=0;Index<Points.Num();++Index)
				if(FMath::Abs(It->GetActorLocation().X-Points[Index]->GetActorLocation().X)<60) UsedPoints.Add(Index);
		}
		for(int32 A=0;A<Locations.Num();++A) for(int32 B=A+1;B<Locations.Num();++B)
			TestTrue(TEXT("No overlapping or repeated point"),FVector::Dist(Locations[A],Locations[B])>=Rules.MinimumSeparation);
		Director->ResetRound(); TestEqual(TEXT("Round reset removes spawned items"),Director->GetSpawnedCount(),0);
		TestFalse(TEXT("Round reset preserves hand-placed items"),HandPlaced->IsActorBeingDestroyed());
	}
	TestTrue(TEXT("Both valid item types appear across rounds"),SawRadio && SawSwitch);
	TestTrue(TEXT("Selected locations vary across rounds"),UsedPoints.Num()>Rules.SpawnCount);
	const FProperty* RoomProperty=FindFProperty<FProperty>(ATBLightSwitch::StaticClass(),TEXT("Room"));
	TestTrue(TEXT("Dynamic switch room reference is replicated"),RoomProperty && RoomProperty->HasAnyPropertyFlags(CPF_Net));
	// 1クラスのチェックで床・壁をまとめて切り替え、別クラスと手置きには影響させない。
	Radio.Placements.Add(TEXT("Floor"),FTransform::Identity);
	for(int32 Index=0;Index<Points.Num();++Index) Points[Index]->PlacementType=Index%2 ? TEXT("Floor") : TEXT("Wall");
	Radio.bEnabled=false; Rules.Entries={Radio,Switch}; Director->StartRound();
	TestEqual(TEXT("Disabling radio still allows switches"),Director->GetSpawnedCount(),3);
	for(TActorIterator<ATBNoiseRadio> It(World);It;++It)
		if(It->GetActorLocation().Z>9000 && !It->IsActorBeingDestroyed()) TestTrue(TEXT("Radio disabled across floor and wall"),*It==HandPlaced);
	Director->ResetRound();
	Radio.bEnabled=true; Switch.bEnabled=false; Rules.Entries={Radio,Switch}; Director->StartRound();
	TestEqual(TEXT("Re-enabling radio restores spawning"),Director->GetSpawnedCount(),3);
	for(TActorIterator<ATBLightSwitch> It(World);It;++It)
		if(It->GetActorLocation().Z>9000) TestTrue(TEXT("Only disabled switch class is excluded"),It->IsActorBeingDestroyed());
	Director->ResetRound();
	Radio.bEnabled=false; Rules.Entries={Radio,Switch}; Director->StartRound();
	TestEqual(TEXT("All item classes disabled"),Director->GetSpawnedCount(),0);
	TestFalse(TEXT("Disabled class leaves hand-placed actor usable"),HandPlaced->IsActorBeingDestroyed()); Director->ResetRound();
	Radio.bEnabled=true; Switch.bEnabled=true;
	Rules.Entries={Radio,Radio}; Rules.Entries[1].bEnabled=false; Director->StartRound();
	TestEqual(TEXT("Duplicate class cannot override its disabled setting"),Director->GetSpawnedCount(),0); Director->ResetRound();
	for(auto* Point:Points) Point->PlacementType=TEXT("Wall");
	Radio.Placements.Add(NAME_None,FTransform(FVector(4000,0,0)));
	Rules.Entries={Radio}; Director->StartRound();
	TestEqual(TEXT("Exact placement takes precedence over fallback"),Director->GetSpawnedCount(),3); Director->ResetRound();
	Radio.Placements.Remove(TEXT("Wall")); Radio.Placements[NAME_None]=FTransform(FVector(40,0,0));
	Rules.Entries={Radio}; Director->StartRound();
	TestEqual(TEXT("None placement is used when exact type is absent"),Director->GetSpawnedCount(),3); Director->ResetRound();
	Radio.Placements.Remove(NAME_None); Radio.Placements.Add(TEXT("Wall"),FTransform(FVector(40,0,0)));
	Rules.Entries={Switch};
	for(auto* Point:Points) Point->Room=nullptr;
	Director->StartRound(); TestEqual(TEXT("Unbound switches never spawn"),Director->GetSpawnedCount(),0); Director->ResetRound();
	Rules.Entries={Radio}; Rules.Entries[0].Weight=0;
	Director->StartRound(); TestEqual(TEXT("Zero-weight entry disabled"),Director->GetSpawnedCount(),0); Director->ResetRound();
	Rules.Entries[0].Weight=1;
	Rules.MinimumSeparation=2000;
	Director->StartRound(); TestEqual(TEXT("Nearby alternatives respect minimum separation"),Director->GetSpawnedCount(),1); Director->ResetRound();
	Rules.MinimumSeparation=120; Rules.bEnabled=false;
	Director->StartRound(); TestEqual(TEXT("Disabled rule never spawns"),Director->GetSpawnedCount(),0); Director->ResetRound(); Rules.bEnabled=true;
	for(auto* Point:Points) Point->bEnabled=false;
	Points[0]->bEnabled=true;
	auto* Blocker=SpawnBox(Points[0]->GetActorLocation()+FVector(40,0,50),FVector(80,80,100));
	Director->StartRound(); TestEqual(TEXT("Blocked location skipped"),Director->GetSpawnedCount(),0); Director->ResetRound(); Blocker->Destroy();
	Points[0]->SetActorLocation(FVector(3000,0,10003));
	Director->StartRound(); TestEqual(TEXT("Unsupported location skipped"),Director->GetSpawnedCount(),0); Director->ResetRound();
	Points[0]->SetActorLocation(FVector(-600,0,10003)); Points[0]->PlacementType=TEXT("Ceiling");
	Director->StartRound(); TestEqual(TEXT("Incompatible placement skipped"),Director->GetSpawnedCount(),0); Director->ResetRound();
	Points[0]->PlacementType=TEXT("Wall");
	Rules.SpawnCount=100; Director->StartRound(); TestEqual(TEXT("Insufficient points produce only safe count"),Director->GetSpawnedCount(),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBAuthoredItemSpawnTest,"ToonStory.Items.AuthoredRandomSpawns",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBAuthoredItemSpawnTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game) World=Context.World();
	if(!World || World->GetNetMode()!=NM_Standalone) return false;
	auto* Mode=World->GetAuthGameMode<ATBGameMode>(); auto* State=World->GetGameState<ATBGameState>();
	auto* Director=Mode ? Mode->FindComponentByClass<UTBItemSpawnDirector>() : nullptr;
	if(!Director || !State) return false;
	auto& Rules=const_cast<UTBRuleData&>(TB::Rules()).RandomItems;
	const auto OldRules=Rules;
	TArray<ATBItemSpawnPoint*> Points;
	for(TActorIterator<ATBItemSpawnPoint> It(World);It;++It) if(It->bEnabled) Points.Add(*It);
	if(!TestTrue(TEXT("Authored stage has more candidate points than items"),Points.Num()>Rules.SpawnCount)) return false;
	TBTest::FPlayingScope Playing(*State,World->GetTimeSeconds()+600);
	ON_SCOPE_EXIT { Director->ResetRound(); Rules=OldRules; for(auto* Point:Points) Point->bEnabled=true; };
	Director->ResetRound();
	for(auto* Point:Points) Point->bEnabled=false;
	Rules.SpawnCount=1;
	for(auto* Point:Points)
	{
		Point->bEnabled=true;
		int32 Eligible=0;
		for(const auto& Entry:OldRules.Entries)
		{
			const FTransform* Placement=Entry.FindPlacement(Point->PlacementType);
			if(!Entry.bEnabled || !Entry.ItemClass || Entry.Weight<=0 || !Placement) continue;
			if(Entry.ItemClass->IsChildOf(ATBLightSwitch::StaticClass()) && !Point->Room) continue;
			++Eligible;
			Rules.Entries={Entry}; Director->StartRound();
			if(Director->GetSpawnedCount()==0)
			{
				FHitResult Floor;
				World->LineTraceSingleByChannel(Floor,Point->GetActorLocation()+FVector(0,0,2),Point->GetActorLocation()-FVector(0,0,Rules.FloorProbeDistance),ECC_WorldStatic);
				AddInfo(FString::Printf(TEXT("Floor: %s at %s"),*GetNameSafe(Floor.GetActor()),*Floor.ImpactPoint.ToString()));
				const FTransform Transform=*Placement*Point->GetActorTransform();
				auto* Probe=World->SpawnActorDeferred<ATBItemBase>(Entry.ItemClass,Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				Probe->FinishSpawning(Transform);
				const FBox Bounds=Probe->GetComponentsBoundingBox(true);
				TArray<FOverlapResult> Overlaps;
				World->OverlapMultiByChannel(Overlaps,Bounds.GetCenter(),FQuat::Identity,ECC_WorldDynamic,FCollisionShape::MakeBox(Bounds.GetExtent()*.98f),FCollisionQueryParams(NAME_None,false,Probe));
				for(const auto& Overlap:Overlaps) if(Overlap.bBlockingHit) AddInfo(FString::Printf(TEXT("Blocked by %s (%s)"),*GetNameSafe(Overlap.GetActor()),*GetNameSafe(Overlap.GetComponent())));
				Probe->Destroy();
			}
			TestEqual(FString::Printf(TEXT("Authored point %s at %s supports %s"),*Point->GetName(),*Point->GetActorLocation().ToString(),*Entry.ItemClass->GetName()),Director->GetSpawnedCount(),1);
			Director->ResetRound();
		}
		TestTrue(TEXT("Every authored point has an eligible item"),Eligible>0);
		Point->bEnabled=false;
	}
	Rules=OldRules;
	for(auto* Point:Points) Point->bEnabled=true;
	for(int32 Round=0;Round<10;++Round)
	{
		Director->StartRound();
		TestEqual(TEXT("Authored stage fills the configured count"),Director->GetSpawnedCount(),Rules.SpawnCount);
		Director->ResetRound();
	}
	return true;
}
#endif
