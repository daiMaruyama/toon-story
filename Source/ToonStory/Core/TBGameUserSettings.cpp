// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBGameUserSettings.h"
#include "Data/TBConfigData.h"

namespace
{
	float Resolve(float Saved, const FTBVolumeSetting& Setting)
	{
		return Setting.Clamp(Saved < 0.f ? Setting.Default : Saved);
	}
}

void UTBGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	// 初期値は持たず「まだ選んでいない」に戻す。初回起動もここを通る。
	MasterVolume = MusicVolume = EffectsVolume = VoiceVolume = -1.f;
}

float UTBGameUserSettings::GetMasterVolume() const { return Resolve(MasterVolume, TB::ConfigData().Audio.Master); }
float UTBGameUserSettings::GetMusicVolume() const { return Resolve(MusicVolume, TB::ConfigData().Audio.Music); }
float UTBGameUserSettings::GetEffectsVolume() const { return Resolve(EffectsVolume, TB::ConfigData().Audio.Effects); }
float UTBGameUserSettings::GetVoiceVolume() const { return Resolve(VoiceVolume, TB::ConfigData().Audio.Voice); }

void UTBGameUserSettings::SetMasterVolume(float Value) { MasterVolume = TB::ConfigData().Audio.Master.Clamp(Value); }
void UTBGameUserSettings::SetMusicVolume(float Value) { MusicVolume = TB::ConfigData().Audio.Music.Clamp(Value); }
void UTBGameUserSettings::SetEffectsVolume(float Value) { EffectsVolume = TB::ConfigData().Audio.Effects.Clamp(Value); }
void UTBGameUserSettings::SetVoiceVolume(float Value) { VoiceVolume = TB::ConfigData().Audio.Voice.Clamp(Value); }
