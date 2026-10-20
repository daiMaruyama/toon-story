// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Data/TBRuleData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "TBRuleData"

EDataValidationResult UTBRuleData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	auto Fail = [&](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	// 狙う範囲は視線を中心にした円すいで、画面の形とは違う。ここで防げるのは、基準の画面比の画面で左右の端より外を
	// 狙えることだけ。上下の端の外や、基準より狭い画面の左右の端の外は、この値でも狙える。
	if (Aim.AimAngle > Vision.FieldOfView * .5f)
	{
		Fail(LOCTEXT("AimWiderThanScreen",
		             "狙える角度（片側）が視野角の半分より広いため、基準の画面比の画面で、左右の端より外の人形もつかめてしまいます。"));
	}
	if (Match.MinDuration > Match.MaxDuration)
	{
		Fail(LOCTEXT("DurationRangeReversed", "試合時間の最短が最長より長くなっています。"));
	}
	else if (Match.DefaultDuration < Match.MinDuration || Match.DefaultDuration > Match.MaxDuration)
	{
		Fail(LOCTEXT("DurationDefaultOutOfRange",
		             "試合時間の初期値が最短〜最長の範囲の外にあるため、ロビーで試合時間を変えられなくなります。"));
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
