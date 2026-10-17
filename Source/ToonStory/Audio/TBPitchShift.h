// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "DSP/Delay.h"
#include "DSP/TapDelayPitchShifter.h"
#include "Sound/SoundEffectSource.h"
#include "TBPitchShift.generated.h"

USTRUCT(BlueprintType)
struct FTBPitchShiftSettings
{
	GENERATED_BODY()
	// 半音単位。正で高く、負で低くなる。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "-12.0", ClampMax = "12.0"))
	float Semitones = 0.f;
	// 内部の遅延の長さ(ms)。声の揺れが気になるときに調整する。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "10.0", ClampMax = "100.0"))
	float DelayLength = 30.f;
};

/** 再生速度を変えずに音程だけをずらす。リアルタイムに届くVCの声にかける。 */
class FTBPitchShift : public FSoundEffectSource
{
public:
	virtual void Init(const FSoundEffectSourceInitData& InitData) override;
	virtual void OnPresetChanged() override;
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData) override;

private:
	int32 NumChannels = 0;
	bool bPassThrough = true;
	TArray<Audio::FDelay> Delays;
	TArray<Audio::FTapDelayPitchShifter> Shifters;
};

UCLASS()
class TOONSTORY_API UTBPitchShiftPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()
public:
	EFFECT_PRESET_METHODS(TBPitchShift)
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FTBPitchShiftSettings& InSettings);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SourceEffectPreset, meta = (ShowOnlyInnerProperties))
	FTBPitchShiftSettings Settings;
};
