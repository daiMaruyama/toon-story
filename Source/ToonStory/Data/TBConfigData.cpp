// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Data/TBConfigData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "TBConfigData"

EDataValidationResult UTBConfigData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	auto Check = [&](const FTBVolumeSetting& Setting, const FText& Name)
	{
		if (Setting.Min > Setting.Max)
		{
			Context.AddError(FText::Format(LOCTEXT("VolumeRangeReversed", "音量（{0}）の下限が上限より大きくなっています。"), Name));
			Result = EDataValidationResult::Invalid;
		}
		else if (Setting.Default < Setting.Min || Setting.Default > Setting.Max)
		{
			Context.AddError(FText::Format(
			    LOCTEXT("VolumeDefaultOutOfRange", "音量（{0}）の初期値が下限〜上限の範囲の外にあります。"), Name));
			Result = EDataValidationResult::Invalid;
		}
	};
	Check(Audio.Master, LOCTEXT("Master", "全体"));
	Check(Audio.Music, LOCTEXT("Music", "BGM"));
	Check(Audio.Effects, LOCTEXT("Effects", "効果音"));
	Check(Audio.Voice, LOCTEXT("Voice", "ボイスチャット"));
	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
