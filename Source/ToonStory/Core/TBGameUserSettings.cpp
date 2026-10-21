// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBGameUserSettings.h"
#include "Audio/TBAudioSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

UTBGameUserSettings* UTBGameUserSettings::Get()
{
	return GEngine ? Cast<UTBGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UTBGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	// 初回起動もここを通る。
	ResetVolumes();
}

float& UTBGameUserSettings::Saved(ETBVolume Kind)
{
	switch (Kind)
	{
		case ETBVolume::Music:
			return MusicVolume;
		case ETBVolume::Effects:
			return EffectsVolume;
		case ETBVolume::Voice:
			return VoiceVolume;
		default:
			return MasterVolume;
	}
}

float UTBGameUserSettings::Saved(ETBVolume Kind) const
{
	return const_cast<UTBGameUserSettings*>(this)->Saved(Kind);
}

float UTBGameUserSettings::GetVolume(ETBVolume Kind) const
{
	const auto& Setting = TB::ConfigData().Audio.Get(Kind);
	const float Value = Saved(Kind);
	return Setting.Clamp(Value < 0.f ? Setting.Default : Value);
}

void UTBGameUserSettings::SetVolume(ETBVolume Kind, float Value)
{
	Saved(Kind) = TB::ConfigData().Audio.Get(Kind).Clamp(Value);
}

void UTBGameUserSettings::StepVolume(ETBVolume Kind, int32 Steps)
{
	const float Step = FMath::Max(TB::ConfigData().Audio.Step, .01f);
	// 刻みの目盛りにそろえてから動かす。初期値が目盛りの間にあっても、1回で次の目盛りに止まる。
	const float Position = GetVolume(Kind) / Step;
	const float Next = Steps > 0 ? FMath::FloorToFloat(Position + .001f) + Steps : FMath::CeilToFloat(Position - .001f) + Steps;
	SetVolume(Kind, Next * Step);
}

void UTBGameUserSettings::ResetVolumes()
{
	MasterVolume = MusicVolume = EffectsVolume = VoiceVolume = -1.f;
}

void UTBGameUserSettings::ApplyVolumes(UTBAudioSubsystem& Audio) const
{
	Audio.SetMasterVolume(GetVolume(ETBVolume::Master));
	Audio.SetMusicVolume(GetVolume(ETBVolume::Music));
	Audio.SetEffectsVolume(GetVolume(ETBVolume::Effects));
	Audio.SetVoiceVolume(GetVolume(ETBVolume::Voice));
}

void UTBGameUserSettings::CommitVolumes(const UGameInstance* GameInstance)
{
	if (auto* Audio = GameInstance ? GameInstance->GetSubsystem<UTBAudioSubsystem>() : nullptr)
	{
		ApplyVolumes(*Audio);
	}
	SaveSettings();
}
