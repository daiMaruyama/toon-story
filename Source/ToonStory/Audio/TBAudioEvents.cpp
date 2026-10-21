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
	case ETBSoundEvent::ToyTaunt:
		return Recipient->Team == ETBTeam::Human || Recipient->Team == ETBTeam::Toy;
	case ETBSoundEvent::Unready:
	case ETBSoundEvent::CarryLift:
	case ETBSoundEvent::CarryDrop:
	case ETBSoundEvent::StoreCancel:
	case ETBSoundEvent::BoxedTransition:
	case ETBSoundEvent::Frozen:
	case ETBSoundEvent::Thawed:
		return Subject && Recipient == Subject;
	case ETBSoundEvent::TVButton:
	case ETBSoundEvent::TVChannel:
	case ETBSoundEvent::TVPowerOn:
	case ETBSoundEvent::TVPowerOff:
	case ETBSoundEvent::TVGlitch:
	case ETBSoundEvent::TVStatic:
		return Recipient->Team == ETBTeam::Toy && Recipient->ToyState == ETBToyState::Boxed;
	case ETBSoundEvent::TeamNotice:
		return Recipient->Team == ETBTeam::Toy && Recipient != Subject;
	case ETBSoundEvent::Pickup:
	case ETBSoundEvent::BatteryLast:
	case ETBSoundEvent::RescueCancel:
	case ETBSoundEvent::RescueComplete:
		return Recipient->Team == ETBTeam::Toy;
	case ETBSoundEvent::Ready:
	case ETBSoundEvent::CaptureHard:
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
	const ETBPhase RequiredPhase = (Event == ETBSoundEvent::Ready || Event == ETBSoundEvent::Unready) ? ETBPhase::Lobby : ETBPhase::Playing;
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
