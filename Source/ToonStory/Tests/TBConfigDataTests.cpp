#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBGameUserSettings.h"
#include "Data/TBConfigData.h"
#include "Data/TBDataSettings.h"
#include "Misc/ScopeExit.h"
#include <limits>
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBConfigDataTest, "ToonStory.Data.Config",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBConfigDataTest::RunTest(const FString& Parameters)
{
	// 音量の初期値は、今のゲームの音量（どれも 1）と同じ。上限は 1 より上にできない。
	const auto& Defaults = GetDefault<UTBConfigData>()->Audio;
	for (const FTBVolumeSetting* Volume : {&Defaults.Master, &Defaults.Music, &Defaults.Effects, &Defaults.Voice})
	{
		TestEqual(TEXT("Volume default and range"), FVector(Volume->Default, Volume->Min, Volume->Max), FVector(1, 0, 1));
	}

	// プロジェクト設定でアセットを選んでいれば、初期値ではなくそのアセットを使う。
	const auto& Selected = GetDefault<UTBDataSettings>()->ConfigData;
	if (!Selected.IsNull())
	{
		TestTrue(TEXT("Selected config asset is used"), &TB::ConfigData() == Selected.LoadSynchronous());
	}

	// 範囲の外の値や数でない値は、範囲に収める。データが壊れていても 0〜1 からは出さない。
	FTBVolumeSetting Volume;
	Volume.Default = .8f;
	Volume.Min = .1f;
	Volume.Max = .9f;
	TestEqual(TEXT("Inside the range stays"), Volume.Clamp(.5f), .5f);
	TestEqual(TEXT("Above the range stops at the max"), Volume.Clamp(2.f), .9f);
	TestEqual(TEXT("Below the range stops at the min"), Volume.Clamp(-1.f), .1f);
	TestEqual(TEXT("Not a number uses the default"), Volume.Clamp(std::numeric_limits<float>::quiet_NaN()), .8f);
	Volume.Max = 3.f;
	TestEqual(TEXT("Max above one is treated as one"), Volume.Clamp(2.f), 1.f);

	// プレイヤーの値は、選ぶまではデータの初期値。選んだ値も、今のデータの範囲で返す。終わったらデータを戻す。
	auto& Config = const_cast<UTBConfigData&>(TB::ConfigData());
	const FTBAudioConfig Saved = Config.Audio;
	ON_SCOPE_EXIT
	{
		Config.Audio = Saved;
	};
	Config.Audio.Music.Default = .6f;
	auto* Settings = NewObject<UTBGameUserSettings>();
	TestEqual(TEXT("Unchosen volume follows the data default"), Settings->GetMusicVolume(), .6f);
	Settings->SetMusicVolume(.3f);
	TestEqual(TEXT("Chosen volume is kept"), Settings->GetMusicVolume(), .3f);
	Settings->SetMasterVolume(5.f);
	TestEqual(TEXT("Volume cannot go above the max"), Settings->GetMasterVolume(), 1.f);
	Config.Audio.Music.Max = .2f;
	TestEqual(TEXT("Saved volume is clamped by the current data"), Settings->GetMusicVolume(), .2f);
	Config.Audio.Music.Max = 1.f;
	Settings->SetToDefaults();
	TestEqual(TEXT("Reset goes back to the data default"), Settings->GetMusicVolume(), .6f);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBConfigDataValidationTest, "ToonStory.Data.ConfigValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBConfigDataValidationTest::RunTest(const FString& Parameters)
{
	auto Passes = [](const UTBConfigData& Config)
	{
		FDataValidationContext Context;
		return Config.IsDataValid(Context) == EDataValidationResult::Valid;
	};
	TestTrue(TEXT("Defaults are valid"), Passes(*GetDefault<UTBConfigData>()));
	TestTrue(TEXT("Selected config asset is valid"), Passes(TB::ConfigData()));

	auto* Config = NewObject<UTBConfigData>();
	Config->Audio.Voice.Default = .5f;
	Config->Audio.Voice.Max = .4f;
	TestFalse(TEXT("Default above the max is rejected"), Passes(*Config));
	Config->Audio.Voice.Max = .5f;
	TestTrue(TEXT("Default at the max is accepted"), Passes(*Config));
	Config->Audio.Voice.Min = .6f;
	TestFalse(TEXT("Min above the max is rejected"), Passes(*Config));
	return true;
}
#endif
#endif
