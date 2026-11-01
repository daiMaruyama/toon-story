#include "TBMomArrivalEvent.h"
#include "TBMomArrivalZone.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Data/TBRuleData.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool UTBMomArrivalEvent::CanExecute_Implementation(UWorld* World) const
{
	if (!World || World->GetNetMode()==NM_Client || !TB::Playing(World) ||
	    TB::GS(World)->IsPreparing() || !TB::Rules().Events.Mom.IsValid() || ATBMomArrivalZone::FindActive(World)) return false;
	for (TActorIterator<ATBMomArrivalZone> It(World);It;++It)
		if (It->bEnabled && !It->IsActorBeingDestroyed()) return true;
	return false;
}

bool UTBMomArrivalEvent::Execute_Implementation(UWorld* World)
{
	if (!CanExecute(World)) return false;
	TArray<ATBMomArrivalZone*> Zones;
	for (TActorIterator<ATBMomArrivalZone> It(World);It;++It)
		if (It->bEnabled && !It->IsActorBeingDestroyed()) Zones.Add(*It);
	return !Zones.IsEmpty() && Zones[FMath::RandHelper(Zones.Num())]->StartEvent();
}
