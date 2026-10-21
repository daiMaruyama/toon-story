// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class UTBGameUserSettings;
class UTBOptionsMenu;

// オプション画面のページ。タブの並びと同じ順。
namespace TBOptionsPage
{
	enum Type : int32
	{
		Audio,
		Display,
		Controls,
		Count
	};
}

// ページごとの行を作る。行の中身（値の読み書き）はページのファイルに置き、画面の枠は UTBOptionsMenu が持つ。
namespace TBOptionsPages
{
	void BuildAudio(UTBOptionsMenu& Menu, UTBGameUserSettings& Settings);
	void BuildDisplay(UTBOptionsMenu& Menu, UTBGameUserSettings& Settings);
	void BuildControls(UTBOptionsMenu& Menu, UTBGameUserSettings& Settings);

	inline FString OnOff(bool bOn)
	{
		return bOn ? TEXT("オン") : TEXT("オフ");
	}

	// 範囲の中の位置（0〜1）。スライダーの位置に使う。
	inline float Fraction(float Value, float Min, float Max)
	{
		return Max > Min ? (Value - Min) / (Max - Min) : 1.f;
	}
}
