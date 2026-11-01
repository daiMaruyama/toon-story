// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Data/TBRuleData.h"
#include "GamePlay/Items/TBItemBase.h"

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
	if (!FMath::IsFinite(Toys.TauntCooldownSeconds) || Toys.TauntCooldownSeconds < .1f)
	{
		Fail(LOCTEXT("TauntCooldownTooShort", "音を鳴らすクールダウンは0.1秒以上の有限の値にしてください。"));
	}
	if (!FMath::IsFinite(Match.HidePreparationSeconds) || Match.HidePreparationSeconds < .1f)
	{
		Fail(LOCTEXT("InvalidPreparationTiming", "隠れる時間は0.1秒以上の有限の値にしてください。"));
	}
	if(RandomItems.SpawnCount<0 || RandomItems.SpawnCount>100 || !FMath::IsFinite(RandomItems.MinimumSeparation) || RandomItems.MinimumSeparation<0 ||
	   !FMath::IsFinite(RandomItems.FloorProbeDistance) || RandomItems.FloorProbeDistance<1)
		Fail(LOCTEXT("InvalidRandomItemLimits", "ランダム配置の出現数は0〜100、最短距離は0cm以上、床の確認距離は1cm以上の有限の値にしてください。"));
	if(!FMath::IsFinite(RandomItems.ObstacleClearance) || RandomItems.ObstacleClearance<0 ||
	   !FMath::IsFinite(RandomItems.FloorHeightTolerance) || RandomItems.FloorHeightTolerance<0 ||
	   !FMath::IsFinite(RandomItems.FloorGap) || RandomItems.FloorGap<.1f || RandomItems.FloorGap>2)
		Fail(LOCTEXT("InvalidRandomItemSpace", "障害物との余白と設置面の許容高低差は0cm以上、底面と床の隙間は0.1〜2cmの有限の値にしてください。"));
	TSet<UClass*> ItemClasses;
	for(const auto& Entry:RandomItems.Entries)
	{
		if(!Entry.ItemClass || ItemClasses.Contains(Entry.ItemClass.Get()))
			Fail(LOCTEXT("DuplicateRandomItemClass", "アイテム別設定にはクラスを指定し、同じクラスは1件だけ登録してください。床・壁の違いは配置種別ごとの変換で設定します。"));
		ItemClasses.Add(Entry.ItemClass.Get());
		if(!FMath::IsFinite(Entry.Weight) || Entry.Weight<0 || Entry.Weight>1000)
			Fail(LOCTEXT("InvalidRandomItemWeight", "アイテムの重みは0〜1000の有限の値にしてください。"));
		for(const auto& Placement:Entry.Placements)
			if(Placement.Value.ContainsNaN() || Placement.Value.GetScale3D().GetMin()<=0)
				Fail(LOCTEXT("InvalidRandomItemPlacement", "配置種別ごとの変換は有限の値、拡大率は正の値にしてください。"));
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif
