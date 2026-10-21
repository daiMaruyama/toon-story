// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBGameUserSettings.h"
#include "Audio/TBAudioSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "RenderUtils.h"

UTBGameUserSettings* UTBGameUserSettings::Get()
{
	return GEngine ? Cast<UTBGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UTBGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	// 初回起動もここを通る。データはまだ読めないことがあるので、「まだ選んでいない」に戻すだけにする。
	ResetVolumes();
	QualityPreset = MotionBlur = RayTracing = InvertMouseY = -1;
	Brightness = MouseSensitivity = -1.f;
}

void UTBGameUserSettings::ApplyNonResolutionSettings()
{
	EnforceLimits();
	Super::ApplyNonResolutionSettings();
	PreviewBrightness();
	// -1 は各ステージのポストプロセスの設定どおり、0 は切る。
	if (auto* BlurAmount = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlur.Amount")))
	{
		BlurAmount->Set(IsMotionBlurEnabled() ? -1.f : 0.f, ECVF_SetByGameSetting);
	}
	// 切ると Lumen はソフトウェアの計算になる。非対応の GPU では、オンでもエンジンが自動でソフトウェアにする。
	if (auto* LumenHardware = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing")))
	{
		LumenHardware->Set(IsRayTracingOn() ? 1 : 0, ECVF_SetByGameSetting);
	}
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
	SetVolume(Kind, TBSettings::StepFrom(GetVolume(Kind), 0.f, TB::ConfigData().Audio.Step, Steps));
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

void UTBGameUserSettings::PreviewVolumes(const UGameInstance* GameInstance) const
{
	if (auto* Audio = GameInstance ? GameInstance->GetSubsystem<UTBAudioSubsystem>() : nullptr)
	{
		ApplyVolumes(*Audio);
	}
}

void UTBGameUserSettings::CommitVolumes(const UGameInstance* GameInstance)
{
	PreviewVolumes(GameInstance);
	SaveSettings();
}

int32 UTBGameUserSettings::GetQualityPreset() const
{
	return QualityPreset >= 0 ? FMath::Min(QualityPreset, 3) : GetOverallScalabilityLevel();
}

void UTBGameUserSettings::SetQualityPreset(int32 Level)
{
	QualityPreset = FMath::Clamp(Level, 0, 3);
	// エンジンのプリセットは描画解像度も変えるので、プレイヤーが選んだ描画解像度は残す。
	const float Scale = ScalabilityQuality.ResolutionQuality;
	SetOverallScalabilityLevel(QualityPreset);
	ScalabilityQuality.ResolutionQuality = Scale;
	EnforceLimits();
}

float UTBGameUserSettings::GetResolutionScalePercent() const
{
	// 0 はエンジンの既定（100% 以下）。
	return ScalabilityQuality.ResolutionQuality > 0.f ? ScalabilityQuality.ResolutionQuality : 100.f;
}

void UTBGameUserSettings::StepResolutionScale(int32 Steps)
{
	SetResolutionScaleValueEx(TB::ConfigData().Display.ResolutionScale.StepFrom(GetResolutionScalePercent(), Steps));
	EnforceLimits();
}

void UTBGameUserSettings::SetResolutionScalePercent(float Percent)
{
	SetResolutionScaleValueEx(TB::ConfigData().Display.ResolutionScale.Snap(Percent));
	EnforceLimits();
}

float UTBGameUserSettings::GetBrightness() const
{
	const auto& Setting = TB::ConfigData().Display.Brightness;
	return Setting.Clamp(Brightness < 0.f ? Setting.Default : Brightness);
}

void UTBGameUserSettings::StepBrightness(int32 Steps)
{
	Brightness = TB::ConfigData().Display.Brightness.StepFrom(GetBrightness(), Steps);
}

void UTBGameUserSettings::SetBrightness(float Gamma)
{
	Brightness = TB::ConfigData().Display.Brightness.Snap(Gamma);
}

void UTBGameUserSettings::PreviewBrightness() const
{
	if (GEngine)
	{
		GEngine->DisplayGamma = GetBrightness();
	}
}

bool UTBGameUserSettings::IsMotionBlurEnabled() const
{
	return MotionBlur < 0 ? TB::ConfigData().Display.bDefaultMotionBlur : MotionBlur != 0;
}

void UTBGameUserSettings::SetMotionBlurEnabled(bool bEnabled)
{
	MotionBlur = bEnabled ? 1 : 0;
}

bool UTBGameUserSettings::IsRayTracingSupported()
{
	return ::IsRayTracingEnabled();
}

bool UTBGameUserSettings::IsRayTracingOn() const
{
	return RayTracing < 0 ? TB::ConfigData().Display.bDefaultRayTracing : RayTracing != 0;
}

void UTBGameUserSettings::SetRayTracingOn(bool bOn)
{
	RayTracing = bOn ? 1 : 0;
}

int32 UTBGameUserSettings::GetFrameRateLimitIndex() const
{
	return TB::ConfigData().Display.FrameRateLimits.IndexOfByKey(FMath::RoundToInt(GetFrameRateLimit()));
}

void UTBGameUserSettings::StepFrameRateLimit(int32 Steps)
{
	const auto& Limits = TB::ConfigData().Display.FrameRateLimits;
	if (Limits.IsEmpty())
	{
		return;
	}
	const int32 Index = GetFrameRateLimitIndex();
	SetFrameRateLimit(Limits[Index == INDEX_NONE ? 0 : FMath::Clamp(Index + Steps, 0, Limits.Num() - 1)]);
}

void UTBGameUserSettings::ResetDisplay()
{
	const auto& Display = TB::ConfigData().Display;
	SetQualityPreset(Display.DefaultQuality);
	SetResolutionScaleValueEx(Display.ResolutionScale.Clamp(Display.ResolutionScale.Default));
	Brightness = -1.f;
	MotionBlur = RayTracing = -1;
	SetVSyncEnabled(Display.bDefaultVSync);
	SetFrameRateLimit(Display.DefaultFrameRateLimit);
	EnforceLimits();
}

void UTBGameUserSettings::EnforceLimits()
{
	const auto& Display = TB::ConfigData().Display;
	auto& Levels = ScalabilityQuality;
	Levels.GlobalIlluminationQuality = FMath::Max(Levels.GlobalIlluminationQuality, Display.MinGlobalIlluminationQuality);
	Levels.ShadowQuality = FMath::Max(Levels.ShadowQuality, Display.MinShadowQuality);
	Levels.ViewDistanceQuality = FMath::Max(Levels.ViewDistanceQuality, Display.MinViewDistanceQuality);
	// 上げて得をしないよう、データの上限が何であっても 100% より上にはしない。
	Levels.ResolutionQuality = FMath::Min(Levels.ResolutionQuality, FMath::Min(Display.ResolutionScale.Max, 100.f));
}

void UTBGameUserSettings::CommitDisplay()
{
	ApplyNonResolutionSettings();
	SaveSettings();
}

float UTBGameUserSettings::GetMouseSensitivity() const
{
	const auto& Setting = TB::ConfigData().Controls.MouseSensitivity;
	return Setting.Clamp(MouseSensitivity < 0.f ? Setting.Default : MouseSensitivity);
}

void UTBGameUserSettings::StepMouseSensitivity(int32 Steps)
{
	MouseSensitivity = TB::ConfigData().Controls.MouseSensitivity.StepFrom(GetMouseSensitivity(), Steps);
}

void UTBGameUserSettings::SetMouseSensitivity(float Value)
{
	MouseSensitivity = TB::ConfigData().Controls.MouseSensitivity.Snap(Value);
}

bool UTBGameUserSettings::IsMouseYInverted() const
{
	return InvertMouseY < 0 ? TB::ConfigData().Controls.bDefaultInvertY : InvertMouseY != 0;
}

void UTBGameUserSettings::SetMouseYInverted(bool bInverted)
{
	InvertMouseY = bInverted ? 1 : 0;
}

void UTBGameUserSettings::ResetControls()
{
	MouseSensitivity = -1.f;
	InvertMouseY = -1;
}

FVector2D UTBGameUserSettings::GetLookScale() const
{
	const float Scale = GetMouseSensitivity();
	return FVector2D(Scale, IsMouseYInverted() ? -Scale : Scale);
}
