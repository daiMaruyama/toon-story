#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBGameUserSettings.h"
#include "Data/TBConfigData.h"
#include "Data/TBDataSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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
	TestEqual(TEXT("Volume step"), Defaults.Step, .1f);
	// 画質を下げても、GI（Lumen）・影・描画距離は下限で残す。描画解像度は 100% より上げられない。
	const auto& Display = GetDefault<UTBConfigData>()->Display;
	TestEqual(TEXT("Quality floor"), FVector(Display.MinGlobalIlluminationQuality, Display.MinShadowQuality,
	                                         Display.MinViewDistanceQuality), FVector(1, 1, 2));
	TestTrue(TEXT("Resolution scale is never above 100%"), Display.ResolutionScale.Max <= 100.f);
	TestEqual(TEXT("Brightness default is the standard gamma"), Display.Brightness.Default, 2.2f);
	TestEqual(TEXT("Mouse sensitivity default"), GetDefault<UTBConfigData>()->Controls.MouseSensitivity.Default, 1.f);

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
	Config.Audio = FTBAudioConfig();
	Config.Audio.Music.Default = .65f;
	auto* Settings = NewObject<UTBGameUserSettings>();
	// 新しく作っても、この PC に保存した値を引き継ぐ。どの PC でも「まだ選んでいない」から始める。
	Settings->SetToDefaults();
	TestEqual(TEXT("Unchosen volume follows the data default"), Settings->GetVolume(ETBVolume::Music), .65f);
	Settings->StepVolume(ETBVolume::Music, 1);
	TestEqual(TEXT("A step from between marks stops at the next mark"), Settings->GetVolume(ETBVolume::Music), .7f);
	Settings->StepVolume(ETBVolume::Music, -1);
	Settings->StepVolume(ETBVolume::Music, -1);
	TestEqual(TEXT("Steps move by the data step"), Settings->GetVolume(ETBVolume::Music), .5f);
	Settings->SetVolume(ETBVolume::Master, 5.f);
	TestEqual(TEXT("Volume cannot go above the max"), Settings->GetVolume(ETBVolume::Master), 1.f);
	Settings->StepVolume(ETBVolume::Master, 1);
	TestEqual(TEXT("A step at the max stays at the max"), Settings->GetVolume(ETBVolume::Master), 1.f);
	Config.Audio.Music.Max = .2f;
	TestEqual(TEXT("Saved volume is clamped by the current data"), Settings->GetVolume(ETBVolume::Music), .2f);
	Config.Audio.Music.Max = 1.f;
	Settings->ResetVolumes();
	TestEqual(TEXT("Reset goes back to the data default"), Settings->GetVolume(ETBVolume::Music), .65f);

	// 画面。低にしても下限は残り、表示は「低」のまま。
	Settings->SetQualityPreset(0);
	TestEqual(TEXT("Low preset is shown as low"), Settings->GetQualityPreset(), 0);
	TestTrue(TEXT("Low preset keeps the quality floor"),
	         Settings->GetGlobalIlluminationQuality() >= Display.MinGlobalIlluminationQuality &&
	             Settings->GetShadowQuality() >= Display.MinShadowQuality &&
	             Settings->GetViewDistanceQuality() >= Display.MinViewDistanceQuality);
	Settings->SetResolutionScaleValueEx(150.f);
	Settings->EnforceLimits();
	TestTrue(TEXT("Resolution scale is capped at 100%"), Settings->GetResolutionScalePercent() <= 100.f);
	Settings->SetResolutionScaleValueEx(100.f);
	Settings->StepResolutionScale(-1);
	TestEqual(TEXT("Resolution scale steps by the data step"), Settings->GetResolutionScalePercent(), 95.f);
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Settings->StepBrightness(1);
	}
	TestEqual(TEXT("Brightness stops at the max"), Settings->GetBrightness(), Display.Brightness.Max);
	Settings->SetFrameRateLimit(0.f);
	Settings->StepFrameRateLimit(-1);
	TestEqual(TEXT("Frame rate limit moves through the data choices"), Settings->GetFrameRateLimit(), 144.f);
	TestTrue(TEXT("Ray tracing follows the data default"), Settings->IsRayTracingOn() == Display.bDefaultRayTracing);
	Settings->SetRayTracingOn(false);
	TestFalse(TEXT("Ray tracing can be turned off"), Settings->IsRayTracingOn());
	Settings->ResetDisplay();
	TestTrue(TEXT("Reset display restores ray tracing"), Settings->IsRayTracingOn() == Display.bDefaultRayTracing);

	// 操作。上下反転は縦だけ逆にする。
	Settings->StepMouseSensitivity(1);
	TestEqual(TEXT("Sensitivity steps by the data step"), Settings->GetMouseSensitivity(), 1.1f);
	Settings->SetMouseYInverted(true);
	TestTrue(TEXT("Invert flips only the vertical look"), Settings->GetLookScale().X > 0 && Settings->GetLookScale().Y < 0);
	Settings->ResetControls();
	TestEqual(TEXT("Reset controls"), Settings->GetLookScale(), FVector2D(1, 1));

	// エンジンはこのクラスを使う。選んだ音量は実際の音に届く。終わったら音量を戻す。
	TestNotNull(TEXT("Engine uses the game's user settings"), UTBGameUserSettings::Get());
	UGameInstance* GameInstance = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.World() && Context.World()->GetGameInstance())
		{
			GameInstance = Context.World()->GetGameInstance();
			break;
		}
	}
	auto* Audio = GameInstance ? GameInstance->GetSubsystem<UTBAudioSubsystem>() : nullptr;
	if (TestNotNull(TEXT("Audio subsystem"), Audio))
	{
		const FVector4 Before(Audio->GetMasterVolume(), Audio->GetMusicVolume(), Audio->GetEffectsVolume(),
		                      Audio->GetVoiceVolume());
		ON_SCOPE_EXIT
		{
			Audio->SetMasterVolume(Before.X);
			Audio->SetMusicVolume(Before.Y);
			Audio->SetEffectsVolume(Before.Z);
			Audio->SetVoiceVolume(Before.W);
		};
		Settings->SetVolume(ETBVolume::Music, .3f);
		Settings->ApplyVolumes(*Audio);
		TestEqual(TEXT("Chosen volume reaches the sound"), Audio->GetMusicVolume(), .3f);
	}
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

	auto* Display = NewObject<UTBConfigData>();
	Display->Display.ResolutionScale.Max = 120.f;
	TestFalse(TEXT("Resolution scale above 100% is rejected"), Passes(*Display));
	Display->Display.ResolutionScale.Max = 100.f;
	Display->Display.DefaultFrameRateLimit = 75;
	TestFalse(TEXT("Frame rate default outside the choices is rejected"), Passes(*Display));
	return true;
}
#endif
#endif
