#include "TBItemSpawnDirector.h"
#include "TBItemSpawnPoint.h"
#include "TBItemBase.h"
#include "Data/TBRuleData.h"
#include "Core/TBGameState.h"
#include "GamePlay/TBLightSwitch.h"
#include "GamePlay/TBRoomLighting.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"

int32 UTBItemSpawnDirector::GetSpawnedCount() const
{
	int32 Count=0;
	for(const auto& Item:SpawnedItems) if(Item.IsValid() && !Item->IsActorBeingDestroyed()) ++Count;
	return Count;
}

void UTBItemSpawnDirector::ResetRound()
{
	if(!GetOwner() || !GetOwner()->HasAuthority()) return;
	if(bSpawning) { bResetRequested=true; return; }
	for(const auto& Item:SpawnedItems) if(Item.IsValid()) Item->Destroy();
	SpawnedItems.Reset();
	bStarted=false;
	bResetRequested=false;
}

void UTBItemSpawnDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	ResetRound();
	Super::EndPlay(Reason);
}

bool UTBItemSpawnDirector::HasFloorAt(const FVector& Location, const FTBItemSpawnRules& Rules, const AActor* Ignore) const
{
	FHitResult Floor;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RandomItemFloor),false,Ignore);
	return GetWorld()->LineTraceSingleByChannel(Floor,Location+FVector(0,0,2),
		Location-FVector(0,0,FMath::Max(1.f,Rules.FloorProbeDistance)),ECC_WorldStatic,Query) && Floor.ImpactNormal.Z>=.7f;
}

bool UTBItemSpawnDirector::CanSpawnAt(const ATBItemSpawnPoint& Point, const FTBItemSpawnRules& Rules) const
{
	if(!Point.bEnabled || Point.IsActorBeingDestroyed() || Point.GetActorTransform().ContainsNaN() ||
	   Point.GetActorScale3D().GetMin()<=0) return false;
	return HasFloorAt(Point.GetActorLocation(),Rules,&Point);
}

ATBItemBase* UTBItemSpawnDirector::TrySpawn(const ATBItemSpawnPoint& Point, const FTBItemSpawnEntry& Entry, const FTBItemSpawnRules& Rules)
{
	const FTransform* Placement=Entry.FindPlacement(Point.PlacementType);
	if(!Placement) return nullptr;
	const FTransform Transform=*Placement*Point.GetActorTransform();
	if(Transform.ContainsNaN() || Transform.GetScale3D().GetMin()<=0) return nullptr;
	// 地点に床があっても、オフセット先が床の外なら置かない。
	// 壁スイッチの高さは許容するが、床より下への移動では移動後の高さから確認する。
	FVector FloorLocation=Transform.GetLocation();
	FloorLocation.Z=FMath::Min(FloorLocation.Z,Point.GetActorLocation().Z);
	if(!HasFloorAt(FloorLocation,Rules,&Point)) return nullptr;
	for(const auto& Existing:SpawnedItems)
		if(Existing.IsValid() && FVector::DistSquared(Existing->GetActorLocation(),Transform.GetLocation())<FMath::Square(FMath::Max(0.f,Rules.MinimumSeparation))) return nullptr;
	// 配置確定前は描画用のOwnerを付けない。全クライアントへ同じActorを複製する。
	auto* Item=GetWorld()->SpawnActorDeferred<ATBItemBase>(Entry.ItemClass,Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if(!Item) return nullptr;
	if(auto* Switch=Cast<ATBLightSwitch>(Item)) Switch->Room=Point.Room;
	Item->FinishSpawning(Transform);
	if(!IsValid(Item) || Item->IsActorBeingDestroyed()) return nullptr;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RandomItemSpace),false,Item);
	const FBox Bounds=Item->GetComponentsBoundingBox(true);
	const FVector Extent=Bounds.GetExtent();
	const bool Blocked=!Item->GetIsReplicated() || !Bounds.IsValid || Extent.IsNearlyZero() ||
		GetWorld()->OverlapBlockingTestByChannel(Bounds.GetCenter(),FQuat::Identity,ECC_WorldDynamic,
			FCollisionShape::MakeBox(Extent.ComponentMax(FVector(1))*.98f),Query);
	if(Blocked) { Item->Destroy(); return nullptr; }
	Item->ForceNetUpdate();
	return Item;
}

void UTBItemSpawnDirector::StartRound()
{
	const auto* State=GetWorld() ? GetWorld()->GetGameState<ATBGameState>() : nullptr;
	if(!GetOwner() || !GetOwner()->HasAuthority() || !State || State->Phase!=ETBPhase::Playing || bStarted || bSpawning) return;
	bStarted=true;
	const uint32 Round=State->HidePreparation.Round;
	auto IsSameRound=[&]() { return !bResetRequested && IsValid(State) && State->Phase==ETBPhase::Playing && State->HidePreparation.Round==Round; };
	ON_SCOPE_EXIT { if(!IsSameRound()) ResetRound(); };
	TGuardValue<bool> Guard(bSpawning,true);
	const FTBItemSpawnRules Rules=TB::Rules().RandomItems;
	if(!Rules.bEnabled || Rules.SpawnCount<=0 || !FMath::IsFinite(Rules.MinimumSeparation) || !FMath::IsFinite(Rules.FloorProbeDistance)) return;
	// 同じクラスのON/OFFが競合しないよう、重複設定されたクラスは全件除外する。
	TMap<UClass*,int32> ClassCounts;
	for(const auto& Entry:Rules.Entries) if(Entry.ItemClass) ++ClassCounts.FindOrAdd(Entry.ItemClass.Get());
	TArray<ATBItemSpawnPoint*> Points;
	for(TActorIterator<ATBItemSpawnPoint> It(GetWorld());It;++It) if(CanSpawnAt(**It,Rules)) Points.Add(*It);
	FRandomStream Random(FMath::Rand());
	for(int32 Index=Points.Num()-1;Index>0;--Index) Points.Swap(Index,Random.RandRange(0,Index));
	for(const auto* Point:Points)
	{
		if(!IsSameRound()) break;
		if(GetSpawnedCount()>=FMath::Clamp(Rules.SpawnCount,0,100)) break;
		if(!IsValid(Point) || !CanSpawnAt(*Point,Rules)) continue;
		TArray<const FTBItemSpawnEntry*> Candidates;
		for(const auto& Entry:Rules.Entries)
		{
			UClass* Class=Entry.ItemClass.Get();
			if(!Entry.bEnabled || !Class || ClassCounts.FindRef(Class)!=1 || Class==ATBItemBase::StaticClass() || Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists) ||
			   !FMath::IsFinite(Entry.Weight) || Entry.Weight<=0 || !Class->GetDefaultObject<ATBItemBase>()->GetIsReplicated() ||
			   !Entry.FindPlacement(Point->PlacementType)) continue;
			if(Class->IsChildOf(ATBLightSwitch::StaticClass()) && (!IsValid(Point->Room) || Point->Room->IsActorBeingDestroyed() || Point->Room->GetWorld()!=GetWorld())) continue;
			Candidates.Add(&Entry);
		}
		while(!Candidates.IsEmpty())
		{
			if(!IsSameRound() || !IsValid(Point) || Point->IsActorBeingDestroyed()) break;
			double Total=0;
			for(const auto* Entry:Candidates) Total+=FMath::Min(1000.f,Entry->Weight);
			double Roll=Random.GetFraction()*Total;
			int32 Selected=Candidates.Num()-1;
			for(int32 Index=0;Index<Candidates.Num();++Index)
			{
				Roll-=FMath::Min(1000.f,Candidates[Index]->Weight);
				if(Roll<0) { Selected=Index; break; }
			}
			if(auto* Item=TrySpawn(*Point,*Candidates[Selected],Rules)) { SpawnedItems.Add(Item); break; }
			Candidates.RemoveAt(Selected);
		}
	}
}
