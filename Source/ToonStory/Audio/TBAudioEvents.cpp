#include "Audio/TBAudioEvents.h"
#include "Core/TBController.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

bool UTBAudioEvents::ShouldDeliver(ETBSoundEvent Event, const ATBPlayerState* Recipient,
                                  const APlayerState* Subject)
{
	if (!Recipient)
	{
		return false;
	}

	switch (Event)
	{
	case ETBSoundEvent::Ready:
	case ETBSoundEvent::Pickup:
	case ETBSoundEvent::Frozen:
	case ETBSoundEvent::Thawed:
		return Subject && Recipient == Subject;
	case ETBSoundEvent::RescueComplete:
		return Recipient->Team == ETBTeam::Toy;
	case ETBSoundEvent::Capture:
	case ETBSoundEvent::Store:
		return true;
	default:
		// UI, movement, loops and phase sounds have local/state-driven playback.
		return false;
	}
}

void UTBAudioEvents::Emit(UObject* WorldContext, ETBSoundEvent Event, APlayerState* Subject, FVector Location)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_Client || Location.ContainsNaN())
	{
		return;
	}
	const ATBGameState* State = TB::GS(World);
	const ETBPhase RequiredPhase = Event == ETBSoundEvent::Ready ? ETBPhase::Lobby : ETBPhase::Playing;
	if (!State || State->Phase != RequiredPhase)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		auto* Controller = Cast<ATBController>(It->Get());
		if (Controller && Controller->HasAuthority() &&
		    ShouldDeliver(Event, Controller->GetPlayerState<ATBPlayerState>(), Subject))
		{
			// The same Client RPC also executes once for the listen-server player.
			Controller->ClientPlaySoundEvent(Event, Location);
		}
	}
}
