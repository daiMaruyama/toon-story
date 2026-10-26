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
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/TBGameUserSettings.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Input/TBKeyPrompt.h"
#include "Input/TBKeyRemap.h"
#include "InputCoreTypes.h"
#include "TBKeyRow.h"
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
		Pages.Add(Box);
		if (Index == TBOptionsPage::Controls)
		{
			// キーの行が多いので、スクロールで包む。右にスクロールバーの分を空ける。
			ControlsScroll = WidgetTree->ConstructWidget<UScrollBox>();
			ControlsScroll->AddChild(Box);
			Place(Canvas, ControlsScroll, 240, 150, 824, 500);
			PageRoots.Add(ControlsScroll);
		}
		else
		{
			Place(Canvas, Box, 240, 150, 800, 500);
			PageRoots.Add(Box);
		}
	}
	ResetButton = AddButton(Canvas, TEXT("初期設定に戻す"), 240, 672, 360, true);
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

	// キーの付け替えの板。暗い幕を押せる（Visible）にして、後ろの行やボタンを押せなくする。
	auto* KeyLayer = WidgetTree->ConstructWidget<UCanvasPanel>();
	Place(Canvas, KeyLayer, 0, 0, 1280, 800);
	auto* KeyDim = WidgetTree->ConstructWidget<UImage>();
	KeyDim->SetColorAndOpacity(FLinearColor(0.015f, .025f, .02f, .6f));
	KeyDim->SetVisibility(ESlateVisibility::Visible);
	Place(KeyLayer, KeyDim, 0, 0, 1280, 800);
	auto* KeyCard = WidgetTree->ConstructWidget<UBorder>();
	KeyCard->SetBrush(TBPaper::Brush(PaperTexture));
	Place(KeyLayer, KeyCard, 380, 260, 520, 280);
	KeyPromptText = Label(TEXT(""), 22, TBPaper::Ink);
	KeyPromptText->SetAutoWrapText(true);
	Place(KeyLayer, KeyPromptText, 420, 290, 440, 110);
	KeyErrorText = Label(TEXT(""), 18, FLinearColor(.75f, .1f, .08f));
	KeyErrorText->SetAutoWrapText(true);
	Place(KeyLayer, KeyErrorText, 420, 400, 440, 40);
	KeySwapButton = AddButton(KeyLayer, TEXT("入れ替える"), 420, 460, 210);
	KeySwapButton->OnClicked.AddDynamic(this, &ThisClass::SwapKey);
	KeyCancelButton = AddButton(KeyLayer, TEXT("やめる"), 650, 460, 210, true);
	KeyCancelButton->OnClicked.AddDynamic(this, &ThisClass::CancelKey);
	KeyCloseButton = AddButton(KeyLayer, TEXT("とじる"), 535, 460, 210);
	KeyCloseButton->OnClicked.AddDynamic(this, &ThisClass::CancelKey);
	KeyOverlay = KeyLayer;
	KeyOverlay->SetVisibility(ESlateVisibility::Collapsed);
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

void UTBOptionsMenu::AddHeading(int32 PageIndex, const FString& Text)
{
	Pages[PageIndex]->AddChildToVerticalBox(MakeLabel(Text, 22, TBPaper::Accent))->SetPadding(FMargin(0, 14, 0, 4));
}

UTBKeyRow* UTBOptionsMenu::AddKeyRow(int32 PageIndex, const FString& Label, FName MappingName)
{
	auto* Row = CreateWidget<UTBKeyRow>(this);
	Row->Build(Label, MappingName, PaperFont, PaperTexture);
	Row->Pressed = [this, MappingName] { BeginKeyCapture(MappingName); };
	Pages[PageIndex]->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 0, 0, 2));
	KeyRows.Add(Row);
	return Row;
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
	for (int32 Other = 0; Other < PageRoots.Num(); ++Other)
	{
		PageRoots[Other]->SetVisibility(Other == Page ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Tabs[Other]->SetStyle(TBPaper::ButtonStyle(PaperTexture, Other != Page));
	}
	if (Page == TBOptionsPage::Controls && ControlsScroll)
	{
		ControlsScroll->ScrollToStart();
	}
	RefreshRows();
}

void UTBOptionsMenu::ShowAudio()
{
	if (IsCapturing()) return;
	PlayClick();
	ShowPage(0);
}

void UTBOptionsMenu::ShowDisplay()
{
	if (IsCapturing()) return;
	PlayClick();
	ShowPage(1);
}

void UTBOptionsMenu::ShowControls()
{
	if (IsCapturing()) return;
	PlayClick();
	ShowPage(2);
}

void UTBOptionsMenu::RefreshRows()
{
	for (UTBOptionRow* Row : Rows)
	{
		Row->Refresh();
	}
	for (UTBKeyRow* Row : KeyRows)
	{
		Row->Refresh();
	}
}

void UTBOptionsMenu::ResetPage()
{
	auto* Settings = UTBGameUserSettings::Get();
	if (!Settings || IsConfirming() || IsCapturing())
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
	if (!Settings || IsCapturing())
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

void UTBOptionsMenu::BeginKeyCapture(FName MappingName)
{
	if (IsConfirming() || IsCapturing())
	{
		return;
	}
	CaptureRow = MappingName;
	PendingKey = EKeys::Invalid;
	PlayClick();
	SetKeyOverlay(EKeyCapture::Waiting, TB::WaitingText(MappingName));
	// 後ろを押せなくしてから、待っている間のキーを受けるメニューにフォーカスを移す。
	SetBackgroundEnabled(false);
	SetKeyboardFocus();
}

void UTBOptionsMenu::ReceiveCapturedKey(const FKey& Key)
{
	if (KeyCapture == EKeyCapture::None)
	{
		return;
	}
	// このキーを押したままのリピートとKeyUpは、メニューの先読みで止める（マウスのボタンにKeyUpは来ない）。
	if (!Key.IsMouseButton())
	{
		HeldTriggerKey = Key;
	}
	// Escとパッドの下のボタン（B）。お知らせは閉じてキー待ちに戻り、それ以外は取り消す。
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		if (KeyCapture == EKeyCapture::Notice)
		{
			ReturnToWaiting();
		}
		else
		{
			CancelKeyCapture();
		}
		return;
	}
	if (KeyCapture != EKeyCapture::Waiting)
	{
		return;
	}
	ApplyKeyPrompt(TB::PromptForRemap(TB::Remap(GetOwningPlayer(), CaptureRow, Key), Key), Key);
}

void UTBOptionsMenu::ConfirmKeySwap()
{
	if (KeyCapture != EKeyCapture::AskSwap)
	{
		return;
	}
	const TB::FKeyPrompt Prompt = TB::PromptForSwap(TB::SwapRemap(GetOwningPlayer(), CaptureRow, PendingKey));
	if (Prompt.Kind == TB::EKeyPromptKind::Redo)
	{
		// 相手がいなくなっていた。普通の付け替えをやり直す。
		ApplyKeyPrompt(TB::PromptForRemap(TB::Remap(GetOwningPlayer(), CaptureRow, PendingKey), PendingKey), PendingKey);
		return;
	}
	ApplyKeyPrompt(Prompt, PendingKey);
}

void UTBOptionsMenu::ApplyKeyPrompt(const TB::FKeyPrompt& Prompt, const FKey& Key)
{
	switch (Prompt.Kind)
	{
	case TB::EKeyPromptKind::Done:
		CancelKeyCapture(false);
		RefreshRows();
		break;
	case TB::EKeyPromptKind::Retry:
		SetKeyOverlay(EKeyCapture::Waiting, TB::WaitingText(CaptureRow), Prompt.Text);
		break;
	case TB::EKeyPromptKind::AskSwap:
		PendingKey = Key;
		SetKeyOverlay(EKeyCapture::AskSwap, Prompt.Text);
		// 押し間違いで入れ替わらないよう、初めは「やめる」にフォーカスを置く。
		KeyCancelButton->SetKeyboardFocus();
		break;
	default:
		SetKeyOverlay(EKeyCapture::Notice, Prompt.Text);
		KeyCloseButton->SetKeyboardFocus();
		break;
	}
}

void UTBOptionsMenu::CancelKeyCapture(bool bBack)
{
	if (KeyCapture == EKeyCapture::None)
	{
		return;
	}
	KeyCapture = EKeyCapture::None;
	KeyOverlay->SetVisibility(ESlateVisibility::Collapsed);
	SetBackgroundEnabled(true);
	PlayClick(bBack);
	// 押した行の「変える」にフォーカスを戻す。
	if (auto* Row = KeyRows.FindByPredicate([this](const UTBKeyRow* Other) { return Other->GetMappingName() == CaptureRow; }))
	{
		(*Row)->FocusButton();
	}
	else
	{
		SetKeyboardFocus();
	}
}

void UTBOptionsMenu::ReturnToWaiting()
{
	if (KeyCapture == EKeyCapture::None)
	{
		return;
	}
	PlayClick(true);
	SetKeyOverlay(EKeyCapture::Waiting, TB::WaitingText(CaptureRow));
	SetKeyboardFocus();
}

void UTBOptionsMenu::SetBackgroundEnabled(bool bEnabled)
{
	for (UWidget* Root : PageRoots)
	{
		Root->SetIsEnabled(bEnabled);
	}
	for (UButton* Tab : Tabs)
	{
		Tab->SetIsEnabled(bEnabled);
	}
	ResetButton->SetIsEnabled(bEnabled);
	CloseButton->SetIsEnabled(bEnabled);
}

bool UTBOptionsMenu::IsBackgroundEnabled() const
{
	return CloseButton && CloseButton->GetIsEnabled();
}

void UTBOptionsMenu::SetKeyOverlay(EKeyCapture State, const FString& Text, const FString& Error)
{
	KeyCapture = State;
	KeyPromptText->SetText(FText::FromString(Text));
	KeyErrorText->SetText(FText::FromString(Error));
	const bool bCancelable = State == EKeyCapture::Waiting || State == EKeyCapture::AskSwap;
	KeySwapButton->SetVisibility(State == EKeyCapture::AskSwap ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	KeyCancelButton->SetVisibility(bCancelable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	KeyCloseButton->SetVisibility(State == EKeyCapture::Notice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	KeyOverlay->SetVisibility(ESlateVisibility::Visible);
}

FString UTBOptionsMenu::GetKeyPromptText() const
{
	return KeyPromptText ? KeyPromptText->GetText().ToString() : FString();
}

FString UTBOptionsMenu::GetKeyErrorText() const
{
	return KeyErrorText ? KeyErrorText->GetText().ToString() : FString();
}

void UTBOptionsMenu::SwapKey()
{
	ConfirmKeySwap();
}

void UTBOptionsMenu::CancelKey()
{
	// お知らせは閉じてキー待ちに戻る。待ちと入れ替えの確認では、取り消す。
	if (KeyCapture == EKeyCapture::Notice)
	{
		ReturnToWaiting();
		return;
	}
	CancelKeyCapture();
}

FReply UTBOptionsMenu::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	// 待っている間は、マウスのボタンも次のキーとして受け、ゲームや後ろの画面へ渡さない。
	// 「やめる」ボタンの上のクリックだけは割り当てに使わず、ボタンに渡す。
	if (KeyCapture == EKeyCapture::Waiting &&
	    !KeyCancelButton->GetCachedGeometry().IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		ReceiveCapturedKey(MouseEvent.GetEffectingButton());
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, MouseEvent);
}

FReply UTBOptionsMenu::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (HeldTriggerKey.IsValid() && KeyEvent.GetKey() == HeldTriggerKey)
	{
		HeldTriggerKey = EKeys::Invalid;
		return FReply::Handled();
	}
	// 待ちと確認の間に離したキーは、ゲーム側へ渡さない。
	return IsCapturing() ? FReply::Handled() : Super::NativeOnKeyUp(Geometry, KeyEvent);
}

FReply UTBOptionsMenu::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (HeldTriggerKey.IsValid() && Key == HeldTriggerKey)
	{
		// 状態を変えたキーを押したままのリピートは止める。押し直したら（リピートでない）新しい押下として扱う。
		if (KeyEvent.IsRepeat())
		{
			return FReply::Handled();
		}
		HeldTriggerKey = EKeys::Invalid;
	}
	if (IsCapturing())
	{
		// 確認とお知らせでは、ボタンを選ぶ・押すキー（左右、Enter、Space、パッドのAと十字）だけ通す。Tabは通さない。
		// 押したままのリピートは通さない。
		const bool bPassToButtons = KeyCapture != EKeyCapture::Waiting && !KeyEvent.IsRepeat() &&
		                            (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Left || Key == EKeys::Right ||
		                             Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Gamepad_DPad_Left ||
		                             Key == EKeys::Gamepad_DPad_Right);
		if (bPassToButtons)
		{
			return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
		}
		if (!KeyEvent.IsRepeat())
		{
			ReceiveCapturedKey(Key);
		}
		return FReply::Handled();
	}
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
	if (IsCapturing())
	{
		return;
	}
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
