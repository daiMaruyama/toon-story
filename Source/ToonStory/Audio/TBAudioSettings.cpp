#include "Audio/TBAudioSettings.h"

UTBAudioSettings::UTBAudioSettings()
{
	for (auto State : {ETBMusicState::Title, ETBMusicState::Lobby, ETBMusicState::Match, ETBMusicState::Results})
	{
		auto& Clip = Music.Add(State);
		Clip.bSpatial = false;
	}
	for (uint8 Index = 0; Index <= uint8(ETBSoundEvent::TVStatic); ++Index)
	{
		const auto Event = ETBSoundEvent(Index);
		auto& Clip = Sounds.Add(Event);
		Clip.bSpatial = Event == ETBSoundEvent::Capture || Event == ETBSoundEvent::Store ||
		                Event == ETBSoundEvent::Footstep || Event == ETBSoundEvent::Jump ||
		                Event == ETBSoundEvent::Land || Event == ETBSoundEvent::RescueLoop ||
		                Event == ETBSoundEvent::HumanFootstep || Event == ETBSoundEvent::HumanLand ||
		                Event == ETBSoundEvent::CaptureHard;
		if (Event == ETBSoundEvent::Footstep || Event == ETBSoundEvent::HumanFootstep) Clip.MaxInstances = 16;
		if (Event == ETBSoundEvent::HumanFootstep) Clip.MaxDistance = 2400.f;
	}
}
