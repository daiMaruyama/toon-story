#include "Audio/TBAudioSettings.h"

UTBAudioSettings::UTBAudioSettings()
{
	for (auto State : {ETBMusicState::Title, ETBMusicState::Lobby, ETBMusicState::Match, ETBMusicState::Results, ETBMusicState::Story})
	{
		auto& Clip = Music.Add(State);
		Clip.bSpatial = false;
	}
	for (uint8 Index = 0; Index <= uint8(ETBSoundEvent::PropDropHeavy); ++Index)
	{
		const auto Event = ETBSoundEvent(Index);
		auto& Clip = Sounds.Add(Event);
		Clip.bToyActionSound = Event == ETBSoundEvent::Footstep || Event == ETBSoundEvent::Jump ||
		    Event == ETBSoundEvent::Land || Event == ETBSoundEvent::ToyTaunt || Event == ETBSoundEvent::ToyPush;
		Clip.bSpatial = Event == ETBSoundEvent::Capture || Event == ETBSoundEvent::Store ||
		                Event == ETBSoundEvent::Footstep || Event == ETBSoundEvent::Jump ||
		                Event == ETBSoundEvent::Land || Event == ETBSoundEvent::RescueLoop ||
		                Event == ETBSoundEvent::HumanFootstep || Event == ETBSoundEvent::HumanLand ||
		                Event == ETBSoundEvent::CaptureHard || Event == ETBSoundEvent::ToyTaunt ||
		                Event == ETBSoundEvent::ToyPush || Event == ETBSoundEvent::PropDropLight ||
		                Event == ETBSoundEvent::PropDropMedium || Event == ETBSoundEvent::PropDropHeavy;
		if (Event == ETBSoundEvent::Footstep || Event == ETBSoundEvent::HumanFootstep) Clip.MaxInstances = 16;
		if (Event == ETBSoundEvent::HumanFootstep) Clip.MaxDistance = 2400.f;
	}
}
