// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Audio/TBPitchShift.h"

// 声が鳴り始めたときに1回。チャンネルごとにテープと再生ヘッドを用意
void FTBPitchShift::Init(const FSoundEffectSourceInitData& InitData)
{
	NumChannels = FMath::Max(1, InitData.NumSourceChannels);
	Delays.SetNum(NumChannels);
	Shifters.SetNum(NumChannels);
	GET_EFFECT_SETTINGS(TBPitchShift);
	bPassThrough = FMath::IsNearlyZero(Settings.Semitones);
	for (int32 Channel = 0; Channel < NumChannels; ++Channel)
	{
		Delays[Channel].Init(InitData.SampleRate, .001f * Audio::FTapDelayPitchShifter::MaxDelayLength); // テープは最大長で確保
		Shifters[Channel].Init(InitData.SampleRate, Settings.Semitones, Settings.DelayLength);
	}
}

// 陣営が変わって設定が変わったら伝え直す
void FTBPitchShift::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(TBPitchShift);
	bPassThrough = FMath::IsNearlyZero(Settings.Semitones);
	for (Audio::FTapDelayPitchShifter& Shifter : Shifters)
	{
		Shifter.SetPitchShift(Settings.Semitones);
		Shifter.SetDelayLength(Settings.DelayLength);
	}
}

// 10msくらいごとに呼ばれる本体。録音しつつ少し速く(遅く)読むと音程だけ変わる
void FTBPitchShift::ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData)
{
	// 左右左右…と交互に並んでるのでチャンネルごとに分ける
	const int32 NumFrames = InData.NumSamples / NumChannels;
	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			const int32 Sample = Frame * NumChannels + Channel;
			const float Shifted = Shifters[Channel].ReadDopplerShiftedTapFromDelay(Delays[Channel]); // 読む
			Delays[Channel].WriteDelayAndInc(InData.InputSourceEffectBufferPtr[Sample]);            // 録音
			// 0半音でも2つの読み位置が混ざって音がにじむので、そのまま通す
			OutAudioBufferData[Sample] = bPassThrough ? InData.InputSourceEffectBufferPtr[Sample] : Shifted;
		}
	}
}

// ゲーム側からの設定変更の入口
void UTBPitchShiftPreset::SetSettings(const FTBPitchShiftSettings& InSettings)
{
	{
		// 声の再生開始時にオーディオ側で遅れて走るInit()が、Settingsで上書きしてくるので揃えておく
		FScopeLock Lock(&SettingsCritSect);
		Settings = InSettings;
	}
	UpdateSettings(InSettings);
}
