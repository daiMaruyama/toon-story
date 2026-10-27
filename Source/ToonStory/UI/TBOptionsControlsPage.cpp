// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBGameUserSettings.h"
#include "Input/TBKeyLabels.h"
#include "TBOptionRow.h"
#include "TBOptionsMenu.h"
#include "TBOptionsPages.h"

void TBOptionsPages::BuildControls(UTBOptionsMenu& Menu, UTBGameUserSettings& Settings)
{
	auto* S = &Settings;
	auto* Sensitivity = Menu.AddRow(TBOptionsPage::Controls, TEXT("マウス感度"), true);
	Sensitivity->Value = [S] { return FString::Printf(TEXT("%.1f"), S->GetMouseSensitivity()); };
	Sensitivity->Fill = [S]
	{
		const auto& Range = TB::ConfigData().Controls.MouseSensitivity;
		return Fraction(S->GetMouseSensitivity(), Range.Min, Range.Max);
	};
	Sensitivity->CanStep = [S](int32 Steps)
	{
		const auto& Range = TB::ConfigData().Controls.MouseSensitivity;
		const float Value = S->GetMouseSensitivity();
		return Steps < 0 ? Value > Range.Min + .001f : Value < Range.Max - .001f;
	};
	Sensitivity->Step = [S](int32 Steps)
	{
		S->StepMouseSensitivity(Steps);
		S->SaveSettings();
	};
	const auto& SensitivityRange = TB::ConfigData().Controls.MouseSensitivity;
	BindSlider(*Sensitivity, SensitivityRange.Min, SensitivityRange.Max, SensitivityRange.Step,
	           [S](float Value) { S->SetMouseSensitivity(Value); }, nullptr, [S] { S->SaveSettings(); });
	auto* Invert = Menu.AddRow(TBOptionsPage::Controls, TEXT("上下反転"), false);
	Invert->Value = [S] { return OnOff(S->IsMouseYInverted()); };
	Invert->CanStep = [S](int32 Steps) { return (Steps > 0) != S->IsMouseYInverted(); };
	Invert->Step = [S](int32 Steps)
	{
		S->SetMouseYInverted(Steps > 0);
		S->SaveSettings();
	};

	// キー設定。行ごとに今のキー名を出し、「変える」で付け替える。
	Menu.AddNote(TBOptionsPage::Controls, TEXT("キーボードとマウスの割り当てです"));
	for (const TB::FKeySection& Section : TB::KeySections())
	{
		Menu.AddHeading(TBOptionsPage::Controls, Section.Heading);
		for (const TB::FKeyRowInfo& Row : Section.Rows)
		{
			Menu.AddKeyRow(TBOptionsPage::Controls, Row.Label, Row.Name);
		}
	}
}
