// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Data/TBConfigData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "TBConfigData"

EDataValidationResult UTBConfigData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	auto Fail = [&](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	auto Check = [&](float Default, float Min, float Max, const FText& Name)
	{
		if (Min > Max)
		{
			Fail(FText::Format(LOCTEXT("RangeReversed", "{0}の下限が上限より大きくなっています。"), Name));
		}
		else if (Default < Min || Default > Max)
		{
			Fail(FText::Format(LOCTEXT("DefaultOutOfRange", "{0}の初期値が下限〜上限の範囲の外にあります。"), Name));
		}
	};
	auto CheckVolume = [&](const FTBVolumeSetting& Setting, const FText& Name)
	{
		Check(Setting.Default, Setting.Min, Setting.Max, Name);
	};
	auto CheckNumber = [&](const FTBNumberSetting& Setting, const FText& Name)
	{
		Check(Setting.Default, Setting.Min, Setting.Max, Name);
	};
	CheckVolume(Audio.Master, LOCTEXT("Master", "音量（全体）"));
	CheckVolume(Audio.Music, LOCTEXT("Music", "音量（BGM）"));
	CheckVolume(Audio.Effects, LOCTEXT("Effects", "音量（効果音）"));
	CheckVolume(Audio.Voice, LOCTEXT("Voice", "音量（ボイスチャット）"));
	CheckNumber(Display.ResolutionScale, LOCTEXT("ResolutionScale", "描画解像度"));
	CheckNumber(Display.Brightness, LOCTEXT("Brightness", "明るさ"));
	CheckNumber(Controls.MouseSensitivity, LOCTEXT("MouseSensitivity", "マウス感度"));
	// 設定で得をさせない。上限はここで止め、ゲーム側でも 100% より上にはしない。
	if (Display.ResolutionScale.Max > 100.f)
	{
		Fail(LOCTEXT("ResolutionScaleAbove100", "描画解像度の上限が 100% を超えています。上げると遠くの小さいおもちゃがくっきり見えて有利になります。"));
	}
	if (!Display.FrameRateLimits.Contains(Display.DefaultFrameRateLimit))
	{
		Fail(LOCTEXT("FrameRateDefaultMissing", "fps の上限の初期値が、選択肢の中にありません。"));
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
