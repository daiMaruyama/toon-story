// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBGameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TBOptionRow.h"
#include "TBOptionsMenu.h"
#include "TBOptionsPages.h"

namespace
{
	const EWindowMode::Type WindowModes[] = {EWindowMode::Windowed, EWindowMode::WindowedFullscreen, EWindowMode::Fullscreen};
	const TCHAR* WindowModeNames[] = {TEXT("ウィンドウ"), TEXT("ボーダーレス"), TEXT("フルスクリーン")};
	const TCHAR* QualityNames[] = {TEXT("低"), TEXT("中"), TEXT("高"), TEXT("最高")};

	int32 WindowModeIndex(const UTBGameUserSettings& Settings)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(WindowModes); ++Index)
		{
			if (WindowModes[Index] == Settings.GetFullscreenMode())
			{
				return Index;
			}
		}
		return 1;
	}

	// 今の画面モードで選べる解像度（小さい順）。ボーダーレスは画面全体なので選ばせない。
	TArray<FIntPoint> Resolutions(const UTBGameUserSettings& Settings)
	{
		TArray<FIntPoint> Result;
		if (Settings.GetFullscreenMode() == EWindowMode::Fullscreen)
		{
			UKismetSystemLibrary::GetSupportedFullscreenResolutions(Result);
		}
		else if (Settings.GetFullscreenMode() == EWindowMode::Windowed)
		{
			UKismetSystemLibrary::GetConvenientWindowedResolutions(Result);
		}
		Result.AddUnique(Settings.GetScreenResolution());
		Result.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
		return Result;
	}
}

// エディタで遊んでいるときは、エディタのウィンドウを変えてしまうので解像度と画面モードは触らない。
void TBOptionsPages::BuildDisplay(UTBOptionsMenu& Menu, UTBGameUserSettings& Settings)
{
	auto* S = &Settings;
	auto* M = &Menu;
	const bool bCanChangeWindow = !GIsEditor;
	auto* Mode = Menu.AddRow(TBOptionsPage::Display, TEXT("画面モード"), false);
	Mode->Value = [S] { return FString(WindowModeNames[WindowModeIndex(*S)]); };
	Mode->CanStep = [S, bCanChangeWindow](int32 Steps)
	{
		const int32 Next = WindowModeIndex(*S) + Steps;
		return bCanChangeWindow && Next >= 0 && Next < UE_ARRAY_COUNT(WindowModes);
	};
	Mode->Step = [M](int32 Steps)
	{
		M->ChangeVideoMode([Steps](UTBGameUserSettings& Changing)
		{
			const auto Next = WindowModes[FMath::Clamp(WindowModeIndex(Changing) + Steps, 0, UE_ARRAY_COUNT(WindowModes) - 1)];
			const FIntPoint Desktop = Changing.GetDesktopResolution();
			Changing.SetFullscreenMode(Next);
			if (Next == EWindowMode::WindowedFullscreen)
			{
				Changing.SetScreenResolution(Desktop);
			}
			else if (Next == EWindowMode::Windowed && Changing.GetScreenResolution().X >= Desktop.X)
			{
				// 画面と同じ大きさのウィンドウは端がはみ出すので、一回り小さくする。
				for (const FIntPoint& Candidate : Resolutions(Changing))
				{
					if (Candidate.X < Desktop.X && Candidate.Y < Desktop.Y)
					{
						Changing.SetScreenResolution(Candidate);
					}
				}
			}
		});
	};
	auto* Resolution = Menu.AddRow(TBOptionsPage::Display, TEXT("解像度"), false);
	Resolution->Value = [S]
	{
		const FIntPoint Size = S->GetFullscreenMode() == EWindowMode::WindowedFullscreen ? S->GetDesktopResolution()
		                                                                                : S->GetScreenResolution();
		return FString::Printf(TEXT("%d × %d"), Size.X, Size.Y);
	};
	Resolution->CanStep = [S, bCanChangeWindow](int32 Steps)
	{
		if (!bCanChangeWindow || S->GetFullscreenMode() == EWindowMode::WindowedFullscreen)
		{
			return false;
		}
		const auto List = Resolutions(*S);
		const int32 Next = List.IndexOfByKey(S->GetScreenResolution()) + Steps;
		return Next >= 0 && Next < List.Num();
	};
	Resolution->Step = [M](int32 Steps)
	{
		M->ChangeVideoMode([Steps](UTBGameUserSettings& Changing)
		{
			const auto List = Resolutions(Changing);
			Changing.SetScreenResolution(List[FMath::Clamp(List.IndexOfByKey(Changing.GetScreenResolution()) + Steps, 0, List.Num() - 1)]);
		});
	};
	auto* VSync = Menu.AddRow(TBOptionsPage::Display, TEXT("VSync"), false);
	VSync->Value = [S] { return OnOff(S->IsVSyncEnabled()); };
	VSync->CanStep = [S](int32 Steps) { return (Steps > 0) != S->IsVSyncEnabled(); };
	VSync->Step = [S](int32 Steps)
	{
		S->SetVSyncEnabled(Steps > 0);
		S->CommitDisplay();
	};
	auto* FrameRate = Menu.AddRow(TBOptionsPage::Display, TEXT("fps の上限"), false);
	FrameRate->Value = [S]
	{
		const int32 Limit = FMath::RoundToInt(S->GetFrameRateLimit());
		return Limit > 0 ? FString::FromInt(Limit) : FString(TEXT("上限なし"));
	};
	FrameRate->CanStep = [S](int32 Steps)
	{
		const int32 Index = S->GetFrameRateLimitIndex();
		return Index == INDEX_NONE || (Index + Steps >= 0 && Index + Steps < TB::ConfigData().Display.FrameRateLimits.Num());
	};
	FrameRate->Step = [S](int32 Steps)
	{
		S->StepFrameRateLimit(Steps);
		S->CommitDisplay();
	};
	auto* Quality = Menu.AddRow(TBOptionsPage::Display, TEXT("画質"), false);
	Quality->Value = [S]
	{
		const int32 Level = S->GetQualityPreset();
		return Level >= 0 ? FString(QualityNames[FMath::Min(Level, 3)]) : FString(TEXT("カスタム"));
	};
	// 段階がばらばら（カスタム）なら、最高から数える（「<」で高、「>」で最高）。
	Quality->CanStep = [S](int32 Steps)
	{
		const int32 Level = S->GetQualityPreset();
		return Level < 0 || (Level + Steps >= 0 && Level + Steps <= 3);
	};
	Quality->Step = [S](int32 Steps)
	{
		const int32 Level = S->GetQualityPreset();
		S->SetQualityPreset((Level < 0 ? 3 : Level) + Steps);
		S->CommitDisplay();
	};
	auto* ResolutionScale = Menu.AddRow(TBOptionsPage::Display, TEXT("描画解像度"), true);
	ResolutionScale->Value = [S] { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S->GetResolutionScalePercent())); };
	ResolutionScale->Fill = [S]
	{
		const auto& Range = TB::ConfigData().Display.ResolutionScale;
		return Fraction(S->GetResolutionScalePercent(), Range.Min, FMath::Min(Range.Max, 100.f));
	};
	ResolutionScale->CanStep = [S](int32 Steps)
	{
		const auto& Range = TB::ConfigData().Display.ResolutionScale;
		const float Value = S->GetResolutionScalePercent();
		return Steps < 0 ? Value > Range.Min + .01f : Value < FMath::Min(Range.Max, 100.f) - .01f;
	};
	ResolutionScale->Step = [S](int32 Steps)
	{
		S->StepResolutionScale(Steps);
		S->CommitDisplay();
	};
	auto* Brightness = Menu.AddRow(TBOptionsPage::Display, TEXT("明るさ"), true);
	Brightness->Value = [S]
	{
		const auto& Range = TB::ConfigData().Display.Brightness;
		const int32 Offset = FMath::RoundToInt((S->GetBrightness() - Range.Default) / FMath::Max(Range.Step, KINDA_SMALL_NUMBER));
		return Offset == 0 ? FString(TEXT("ふつう")) : FString::Printf(TEXT("%+d"), Offset);
	};
	Brightness->Fill = [S]
	{
		const auto& Range = TB::ConfigData().Display.Brightness;
		return Fraction(S->GetBrightness(), Range.Min, Range.Max);
	};
	Brightness->CanStep = [S](int32 Steps)
	{
		const auto& Range = TB::ConfigData().Display.Brightness;
		const float Value = S->GetBrightness();
		return Steps < 0 ? Value > Range.Min + .001f : Value < Range.Max - .001f;
	};
	Brightness->Step = [S](int32 Steps)
	{
		S->StepBrightness(Steps);
		S->CommitDisplay();
	};
	auto* MotionBlur = Menu.AddRow(TBOptionsPage::Display, TEXT("モーションブラー"), false);
	MotionBlur->Value = [S] { return OnOff(S->IsMotionBlurEnabled()); };
	MotionBlur->CanStep = [S](int32 Steps) { return (Steps > 0) != S->IsMotionBlurEnabled(); };
	MotionBlur->Step = [S](int32 Steps)
	{
		S->SetMotionBlurEnabled(Steps > 0);
		S->CommitDisplay();
	};
	auto* RayTracing = Menu.AddRow(TBOptionsPage::Display, TEXT("レイトレーシング"), false);
	RayTracing->Value = [S]
	{
		return UTBGameUserSettings::IsRayTracingSupported() ? OnOff(S->IsRayTracingOn()) : FString(TEXT("この PC では使えません"));
	};
	RayTracing->CanStep = [S](int32 Steps)
	{
		return UTBGameUserSettings::IsRayTracingSupported() && (Steps > 0) != S->IsRayTracingOn();
	};
	RayTracing->Step = [S](int32 Steps)
	{
		S->SetRayTracingOn(Steps > 0);
		S->CommitDisplay();
	};
	Menu.AddNote(TBOptionsPage::Display,
	             FString::Printf(TEXT("解像度と画面モードは、変えたあと「このままにする」を押さないと%d秒で元に戻ります。"),
	                             FMath::RoundToInt(TB::ConfigData().Display.VideoModeConfirmSeconds)));
}
