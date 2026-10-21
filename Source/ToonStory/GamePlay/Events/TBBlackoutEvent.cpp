#include "TBBlackoutEvent.h"
#include "GamePlay/TBRoomLighting.h"
#include "Core/TBGameHelpers.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	bool IsTarget(const ATBRoomLighting* Room)
	{
		return IsValid(Room) && !Room->IsActorBeingDestroyed() && Room->HasAuthority() &&
		       Room->bLightsOn && Room->Lights.ContainsByPredicate([](const ALight* Light)
		       {
			       return IsValid(Light) && Light->GetLightComponent() &&
			              Light->GetLightComponent()->Mobility == EComponentMobility::Movable;
		       });
	}

	bool CanRun(const UWorld* World)
	{
		return World && World->GetNetMode() != NM_Client && TB::Playing(World);
	}
}

bool UTBBlackoutEvent::CanExecute_Implementation(UWorld* World) const
{
	if (!CanRun(World)) return false;
	for (TActorIterator<ATBRoomLighting> It(World); It; ++It)
		if (IsTarget(*It)) return true;
	return false;
}

bool UTBBlackoutEvent::Execute_Implementation(UWorld* World)
{
	if (!CanRun(World)) return false;
	TArray<ATBRoomLighting*> Candidates;
	for (TActorIterator<ATBRoomLighting> It(World); It; ++It)
		if (IsTarget(*It)) Candidates.Add(*It);
	if (Candidates.IsEmpty()) return false;
	auto* Room = Candidates[FMath::RandHelper(Candidates.Num())];
	Room->SetLightsOn(false);
	UE_LOG(LogTemp, Log, TEXT("[ToonStoryEvents] Blackout room: %s"), *Room->GetName());
	return !Room->bLightsOn;
}
