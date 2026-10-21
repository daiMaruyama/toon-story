// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBOptionsMenu.h"
#include "Audio/TBAudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/TBGameUserSettings.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TBOptionRow.h"
#include "TBPaperStyle.h"

namespace
{
	const EWindowMode::Type WindowModes[] = {EWindowMode::Windowed, EWindowMode::WindowedFullscreen, EWindowMode::Fullscreen};
	const TCHAR* WindowModeNames[] = {TEXT("ウィンドウ"), TEXT("ボーダーレス"), TEXT("フルスクリーン")};
	const TCHAR* QualityNames[] = {TEXT("低"), TEXT("中"), TEXT("高"), TEXT("最高")};

	FString OnOff(bool bOn)
	{
		return bOn ? TEXT("オン") : TEXT("オフ");
	}

	// 範囲の中の位置（0〜1）。バーの長さに使う。
	float Fraction(float Value, float Min, float Max)
	{
		return Max > Min ? (Value - Min) / (Max - Min) : 1.f;
	}

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

UTBOptionsMenu* UTBOptionsMenu::Open(APlayerController* Player, TFunction<void()> OnClosed)
{
	auto* Menu = Player ? CreateWidget<UTBOptionsMenu>(Player) : nullptr;
	if (!Menu)
	{
		return nullptr;
	}
	Menu->Closed = MoveTemp(OnClosed);
	Menu->AddToViewport(50);
	Menu->SetKeyboardFocus();
	return Menu;
}

void UTBOptionsMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (auto* Settings = UTBGameUserSettings::Get())
	{
		Build(*Settings);
		ShowPage(0);
	}
}

void UTBOptionsMenu::Build(UTBGameUserSettings& Settings)
{
	PaperFont = TBPaper::Font(this);
	PaperTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/PaperClub/Panel.Panel"));
	auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	auto Fill = [&](UWidget* Widget)
	{
		auto* Slot = Root->AddChildToCanvas(Widget);
		Slot->SetAnchors(FAnchors(0, 0, 1, 1));
		Slot->SetOffsets(FMargin(0));
	};
	auto* Dim = WidgetTree->ConstructWidget<UImage>();
	Dim->SetColorAndOpacity(FLinearColor(0.015f, .025f, .02f, .78f));
	Fill(Dim);
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit);
	Fill(Scale);
	auto* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(1280);
	Size->SetHeightOverride(800);
	Scale->SetContent(Size);
	auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Size->SetContent(Canvas);
	auto Place = [](UCanvasPanel* Panel, UWidget* Widget, float X, float Y, float W, float H)
	{
		auto* Slot = Panel->AddChildToCanvas(Widget);
		Slot->SetPosition(FVector2D(X, Y));
		Slot->SetSize(FVector2D(W, H));
	};
	auto Label = [&](const FString& Text, int32 FontSize, FLinearColor Color)
	{
		auto* Block = WidgetTree->ConstructWidget<UTextBlock>();
		Block->SetText(FText::FromString(Text));
		Block->SetFont(FSlateFontInfo(PaperFont, FontSize));
		Block->SetColorAndOpacity(Color);
		return Block;
	};
	auto AddButton = [&](UCanvasPanel* Panel, const FString& Text, float X, float Y, float W, bool Secondary = false)
	{
		auto* Button = WidgetTree->ConstructWidget<UButton>();
		Button->SetStyle(TBPaper::ButtonStyle(PaperTexture, Secondary));
		Button->SetContent(Label(Text, 20, TBPaper::Ink));
		Place(Panel, Button, X, Y, W, 56);
		return Button;
	};
	auto* Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrush(TBPaper::Brush(PaperTexture, TBPaper::Cream));
	Place(Canvas, Card, 200, 40, 880, 720);
	Place(Canvas, Label(TEXT("オプション"), 30, TBPaper::Accent), 240, 74, 260, 50);
	Tabs.Add(AddButton(Canvas, TEXT("音"), 520, 68, 160));
	Tabs.Add(AddButton(Canvas, TEXT("画面"), 690, 68, 160));
	Tabs.Add(AddButton(Canvas, TEXT("操作"), 860, 68, 160));
	Tabs[0]->OnClicked.AddDynamic(this, &ThisClass::ShowAudio);
	Tabs[1]->OnClicked.AddDynamic(this, &ThisClass::ShowDisplay);
	Tabs[2]->OnClicked.AddDynamic(this, &ThisClass::ShowControls);
	for (int32 Index = 0; Index < Tabs.Num(); ++Index)
	{
		auto* Box = WidgetTree->ConstructWidget<UVerticalBox>();
		Place(Canvas, Box, 240, 150, 800, 500);
		Pages.Add(Box);
	}
	auto* ResetButton = AddButton(Canvas, TEXT("初期設定に戻す"), 240, 672, 360, true);
	ResetButton->OnClicked.AddDynamic(this, &ThisClass::ResetPage);
	CloseButton = AddButton(Canvas, TEXT("閉じる [Esc]"), 680, 672, 360);
	CloseButton->OnClicked.AddDynamic(this, &ThisClass::Close);

	// 音。足音は効果音に含める（足音だけを上げると索敵が有利になるため）。
	auto* S = &Settings;
	const TCHAR* VolumeNames[] = {TEXT("全体"), TEXT("BGM"), TEXT("効果音"), TEXT("ボイスチャット")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(VolumeNames); ++Index)
	{
		const auto Kind = ETBVolume(Index);
		auto* Row = AddRow(0, VolumeNames[Index], true);
		Row->Step = [this, S, Kind](int32 Steps)
		{
			S->StepVolume(Kind, Steps);
			S->CommitVolumes(GetOwningPlayer() ? GetOwningPlayer()->GetGameInstance() : nullptr);
		};
		Row->Value = [S, Kind] { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S->GetVolume(Kind) * 100)); };
		Row->Fill = [S, Kind] { return S->GetVolume(Kind); };
		Row->CanStep = [S, Kind](int32 Steps)
		{
			const auto& Range = TB::ConfigData().Audio.Get(Kind);
			const float Volume = S->GetVolume(Kind);
			return Steps < 0 ? Volume > Range.Clamp(0.f) + KINDA_SMALL_NUMBER : Volume < Range.Clamp(1.f) - KINDA_SMALL_NUMBER;
		};
	}

	// 画面。エディタで遊んでいるときは、エディタのウィンドウを変えてしまうので解像度と画面モードは触らない。
	const bool bCanChangeWindow = !GIsEditor;
	auto* Mode = AddRow(1, TEXT("画面モード"), false);
	Mode->Value = [S] { return FString(WindowModeNames[WindowModeIndex(*S)]); };
	Mode->CanStep = [S, bCanChangeWindow](int32 Steps)
	{
		const int32 Next = WindowModeIndex(*S) + Steps;
		return bCanChangeWindow && Next >= 0 && Next < UE_ARRAY_COUNT(WindowModes);
	};
	Mode->Step = [this](int32 Steps)
	{
		ChangeVideoMode([Steps](UTBGameUserSettings& Changing)
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
	auto* Resolution = AddRow(1, TEXT("解像度"), false);
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
	Resolution->Step = [this](int32 Steps)
	{
		ChangeVideoMode([Steps](UTBGameUserSettings& Changing)
		{
			const auto List = Resolutions(Changing);
			Changing.SetScreenResolution(List[FMath::Clamp(List.IndexOfByKey(Changing.GetScreenResolution()) + Steps, 0, List.Num() - 1)]);
		});
	};
	auto* VSync = AddRow(1, TEXT("VSync"), false);
	VSync->Value = [S] { return OnOff(S->IsVSyncEnabled()); };
	VSync->CanStep = [S](int32 Steps) { return (Steps > 0) != S->IsVSyncEnabled(); };
	VSync->Step = [S](int32 Steps)
	{
		S->SetVSyncEnabled(Steps > 0);
		S->CommitDisplay();
	};
	auto* FrameRate = AddRow(1, TEXT("fps の上限"), false);
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
	auto* Quality = AddRow(1, TEXT("画質"), false);
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
	auto* ResolutionScale = AddRow(1, TEXT("描画解像度"), true);
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
	auto* Brightness = AddRow(1, TEXT("明るさ"), true);
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
	auto* MotionBlur = AddRow(1, TEXT("モーションブラー"), false);
	MotionBlur->Value = [S] { return OnOff(S->IsMotionBlurEnabled()); };
	MotionBlur->CanStep = [S](int32 Steps) { return (Steps > 0) != S->IsMotionBlurEnabled(); };
	MotionBlur->Step = [S](int32 Steps)
	{
		S->SetMotionBlurEnabled(Steps > 0);
		S->CommitDisplay();
	};
	auto* RayTracing = AddRow(1, TEXT("レイトレーシング"), false);
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
	Pages[1]->AddChildToVerticalBox(Label(TEXT("解像度と画面モードは、変えたあと「このままにする」を押さないと15秒で元に戻ります。"), 15,
	                                      TBPaper::Muted));

	// 操作
	auto* Sensitivity = AddRow(2, TEXT("マウス感度"), true);
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
	auto* Invert = AddRow(2, TEXT("上下反転"), false);
	Invert->Value = [S] { return OnOff(S->IsMouseYInverted()); };
	Invert->CanStep = [S](int32 Steps) { return (Steps > 0) != S->IsMouseYInverted(); };
	Invert->Step = [S](int32 Steps)
	{
		S->SetMouseYInverted(Steps > 0);
		S->SaveSettings();
	};

	// 解像度と画面モードを変えたあとの確認。
	auto* Confirm = WidgetTree->ConstructWidget<UCanvasPanel>();
	Place(Canvas, Confirm, 0, 0, 1280, 800);
	auto* ConfirmDim = WidgetTree->ConstructWidget<UImage>();
	ConfirmDim->SetColorAndOpacity(FLinearColor(0.015f, .025f, .02f, .6f));
	Place(Confirm, ConfirmDim, 0, 0, 1280, 800);
	auto* ConfirmCard = WidgetTree->ConstructWidget<UBorder>();
	ConfirmCard->SetBrush(TBPaper::Brush(PaperTexture));
	Place(Confirm, ConfirmCard, 380, 280, 520, 240);
	ConfirmText = Label(TEXT(""), 22, TBPaper::Ink);
	Place(Confirm, ConfirmText, 420, 310, 440, 90);
	KeepButton = AddButton(Confirm, TEXT("このままにする"), 420, 430, 210);
	KeepButton->OnClicked.AddDynamic(this, &ThisClass::KeepVideoMode);
	AddButton(Confirm, TEXT("元に戻す"), 650, 430, 210, true)->OnClicked.AddDynamic(this, &ThisClass::RevertVideoMode);
	ConfirmOverlay = Confirm;
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
}

UTBOptionRow* UTBOptionsMenu::AddRow(int32 PageIndex, const FString& Label, bool bBar)
{
	auto* Row = CreateWidget<UTBOptionRow>(this);
	Row->Build(Label, PaperFont, PaperTexture, bBar);
	// 変えたあとの音量・設定で鳴らす。ほかの行の表示も変わることがあるので、全部の行を更新する。
	Row->Changed = [this]
	{
		PlayClick();
		RefreshRows();
	};
	Pages[PageIndex]->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 0, 0, 2));
	Rows.Add(Row);
	return Row;
}

void UTBOptionsMenu::ShowPage(int32 Index)
{
	Page = FMath::Clamp(Index, 0, Pages.Num() - 1);
	for (int32 Other = 0; Other < Pages.Num(); ++Other)
	{
		Pages[Other]->SetVisibility(Other == Page ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Tabs[Other]->SetStyle(TBPaper::ButtonStyle(PaperTexture, Other != Page));
	}
	RefreshRows();
}

void UTBOptionsMenu::ShowAudio()
{
	PlayClick();
	ShowPage(0);
}

void UTBOptionsMenu::ShowDisplay()
{
	PlayClick();
	ShowPage(1);
}

void UTBOptionsMenu::ShowControls()
{
	PlayClick();
	ShowPage(2);
}

void UTBOptionsMenu::RefreshRows()
{
	for (UTBOptionRow* Row : Rows)
	{
		Row->Refresh();
	}
}

void UTBOptionsMenu::ResetPage()
{
	auto* Settings = UTBGameUserSettings::Get();
	if (!Settings || IsConfirming())
	{
		return;
	}
	if (Page == 0)
	{
		Settings->ResetVolumes();
		Settings->CommitVolumes(GetOwningPlayer() ? GetOwningPlayer()->GetGameInstance() : nullptr);
	}
	else if (Page == 1)
	{
		Settings->ResetDisplay();
		Settings->CommitDisplay();
	}
	else
	{
		Settings->ResetControls();
		Settings->SaveSettings();
	}
	PlayClick();
	RefreshRows();
}

void UTBOptionsMenu::ChangeVideoMode(TFunctionRef<void(UTBGameUserSettings&)> Change)
{
	auto* Settings = UTBGameUserSettings::Get();
	if (!Settings)
	{
		return;
	}
	Change(*Settings);
	Settings->ApplyResolutionSettings(false);
	ConfirmUntil = FPlatformTime::Seconds() + 15;
	ConfirmOverlay->SetVisibility(ESlateVisibility::Visible);
	KeepButton->SetKeyboardFocus();
}

bool UTBOptionsMenu::IsConfirming() const
{
	return ConfirmOverlay && ConfirmOverlay->GetVisibility() == ESlateVisibility::Visible;
}

void UTBOptionsMenu::KeepVideoMode()
{
	if (auto* Settings = UTBGameUserSettings::Get())
	{
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
	}
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	PlayClick();
	RefreshRows();
	SetKeyboardFocus();
}

void UTBOptionsMenu::RevertVideoMode()
{
	if (auto* Settings = UTBGameUserSettings::Get())
	{
		Settings->RevertVideoMode();
		Settings->ApplyResolutionSettings(false);
	}
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	PlayClick(true);
	RefreshRows();
	SetKeyboardFocus();
}

void UTBOptionsMenu::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!IsConfirming())
	{
		return;
	}
	const double Left = ConfirmUntil - FPlatformTime::Seconds();
	if (Left <= 0)
	{
		RevertVideoMode();
		return;
	}
	ConfirmText->SetText(FText::FromString(
	    FString::Printf(TEXT("この表示でよろしいですか？\nあと %d 秒で元に戻ります。"), FMath::CeilToInt(Left))));
}

FReply UTBOptionsMenu::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		if (IsConfirming())
		{
			RevertVideoMode();
		}
		else
		{
			Close();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

void UTBOptionsMenu::PlayClick(bool bBack) const
{
	const auto* Player = GetOwningPlayer();
	if (auto* Audio = Player && Player->GetGameInstance() ? Player->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>() : nullptr)
	{
		Audio->PlaySoundEvent(bBack ? ETBSoundEvent::UIBack : ETBSoundEvent::UIClick);
	}
}

void UTBOptionsMenu::Close()
{
	PlayClick(true);
	Dismiss();
}

void UTBOptionsMenu::Dismiss()
{
	// 確認していない解像度・画面モードは、閉じるときに元に戻す。
	if (IsConfirming())
	{
		if (auto* Settings = UTBGameUserSettings::Get())
		{
			Settings->RevertVideoMode();
			Settings->ApplyResolutionSettings(false);
		}
		ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	RemoveFromParent();
	if (Closed)
	{
		const auto Callback = MoveTemp(Closed);
		Closed = nullptr;
		Callback();
	}
}
