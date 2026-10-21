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
#include "TBOptionRow.h"
#include "TBOptionsPages.h"
#include "TBPaperStyle.h"

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
	auto Label = [this](const FString& Text, int32 FontSize, FLinearColor Color) { return MakeLabel(Text, FontSize, Color); };
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

	TBOptionsPages::BuildAudio(*this, Settings);
	TBOptionsPages::BuildDisplay(*this, Settings);
	TBOptionsPages::BuildControls(*this, Settings);

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

void UTBOptionsMenu::AddNote(int32 PageIndex, const FString& Text)
{
	Pages[PageIndex]->AddChildToVerticalBox(MakeLabel(Text, 15, TBPaper::Muted));
}

UTextBlock* UTBOptionsMenu::MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color)
{
	auto* Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(FText::FromString(Text));
	Block->SetFont(FSlateFontInfo(PaperFont, FontSize));
	Block->SetColorAndOpacity(Color);
	return Block;
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
	if (Page == TBOptionsPage::Audio)
	{
		Settings->ResetVolumes();
		Settings->CommitVolumes(GetOwningPlayer() ? GetOwningPlayer()->GetGameInstance() : nullptr);
	}
	else if (Page == TBOptionsPage::Display)
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
	PreviousResolution = Settings->GetScreenResolution();
	PreviousWindowMode = Settings->GetFullscreenMode();
	Change(*Settings);
	Settings->ApplyResolutionSettings(false);
	ConfirmUntil = FPlatformTime::Seconds() + TB::ConfigData().Display.VideoModeConfirmSeconds;
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

void UTBOptionsMenu::RestoreVideoMode(UTBGameUserSettings& Settings) const
{
	Settings.SetScreenResolution(PreviousResolution);
	Settings.SetFullscreenMode(PreviousWindowMode);
	Settings.ApplyResolutionSettings(false);
	Settings.ConfirmVideoMode();
}

void UTBOptionsMenu::RevertVideoMode()
{
	if (auto* Settings = UTBGameUserSettings::Get())
	{
		RestoreVideoMode(*Settings);
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

void UTBOptionsMenu::NativeDestruct()
{
	// 確認していない解像度・画面モードは、閉じるときに元に戻す。ステージの移動で画面ごと消えたときも、ここを通る。
	if (IsConfirming())
	{
		if (auto* Settings = UTBGameUserSettings::Get())
		{
			RestoreVideoMode(*Settings);
		}
		ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	Super::NativeDestruct();
}

void UTBOptionsMenu::Dismiss()
{
	RemoveFromParent();
	if (Closed)
	{
		const auto Callback = MoveTemp(Closed);
		Closed = nullptr;
		Callback();
	}
}
