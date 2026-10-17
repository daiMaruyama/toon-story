// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBTitleMenu.h"
#include "Core/TBSession.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "TBPaperStyle.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/Image.h"
#include "Kismet/KismetSystemLibrary.h"

void UTBTitleMenu::BuildPaperLayout()
{
	PaperFont = TBPaper::Font(this);
	LogoFont = TBPaper::Font(this, true);
	PaperTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/PaperClub/Panel.Panel"));
	BackgroundTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/PaperClub/Room.Room"));
	auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	auto Fill = [&](UWidget* Widget)
	{
		auto* Slot = Root->AddChildToCanvas(Widget);
		Slot->SetAnchors(FAnchors(0, 0, 1, 1));
		Slot->SetOffsets(FMargin(0));
	};
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit);
	auto* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(1280);
	Size->SetHeightOverride(800);
	Scale->SetContent(Size);
	auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Size->SetContent(Canvas);
	auto Place = [&](UWidget* Widget, float X, float Y, float W, float H)
	{
		auto* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetPosition(FVector2D(X, Y));
		Slot->SetSize(FVector2D(W, H));
	};
	auto* Background = WidgetTree->ConstructWidget<UImage>();
	if (BackgroundTexture)
	{
		Background->SetBrushFromTexture(BackgroundTexture);
	}
	else
	{
		Background->SetColorAndOpacity(FLinearColor(.08f, .11f, .095f));
	}
	Fill(Background);
	auto* Shade = WidgetTree->ConstructWidget<UImage>();
	Shade->SetColorAndOpacity(FLinearColor(0.015f, .025f, .02f, .38f));
	Fill(Shade);
	Fill(Scale);
	auto Label = [&](const TCHAR* Text, int32 FontSize, FLinearColor Color, bool Logo = false)
	{
		auto* L = WidgetTree->ConstructWidget<UTextBlock>();
		L->SetText(FText::FromString(Text));
		L->SetFont(FSlateFontInfo(Logo ? LogoFont : PaperFont, FontSize));
		L->SetColorAndOpacity(Color);
		L->SetAutoWrapText(true);
		return L;
	};
	auto* Logo = Label(TEXT("TOON"), 84, TBPaper::Cream, true);
	Logo->SetShadowColorAndOpacity(FLinearColor(.16f, .05f, .025f, 1));
	Logo->SetShadowOffset(FVector2D(4, 5));
	Place(Logo, 80, 62, 580, 120);
	Logo->SetRenderTransformAngle(-5);
	auto* Story = Label(TEXT("STORY"), 84, TBPaper::Accent, true);
	Story->SetShadowColorAndOpacity(FLinearColor(.16f, .05f, .025f, 1));
	Story->SetShadowOffset(FVector2D(4, 5));
	Place(Story, 91, 157, 590, 130);
	Story->SetRenderTransformAngle(-5);
	Place(Label(TEXT("トゥーン ストーリー"), 17, TBPaper::Cream), 101, 325, 510, 32);
	Place(Label(TEXT("おもちゃたちの、ひみつの時間。"), 22, TBPaper::Cream), 88, 373, 550, 42);
	auto AddButton = [&](const TCHAR* Name, const TCHAR* Text, float X, float Y, float W, bool Secondary = false)
	{
		auto* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(Name));
		FButtonStyle Style;
		Style.SetNormal(TBPaper::Brush(PaperTexture, Secondary ? FLinearColor(.78f, .78f, .75f) : FLinearColor::White));
		Style.SetHovered(TBPaper::Brush(PaperTexture, TBPaper::Cream));
		Style.SetPressed(TBPaper::Brush(PaperTexture, FLinearColor(.9f, .66f, .47f)));
		Style.SetDisabled(TBPaper::Brush(PaperTexture, FLinearColor(.46f, .46f, .43f)));
		B->SetStyle(Style);
		B->OnHovered.AddDynamic(this, &ThisClass::Hover);
		auto* ButtonLabel = Label(Text, W < 250 ? 15 : 21, TBPaper::Ink);
		ButtonLabel->SetAutoWrapText(false);
		B->SetContent(ButtonLabel);
		Place(B, X, Y, W, 64);
		return B;
	};
	HostButton = AddButton(TEXT("Host"), TEXT("部屋をつくる"), 90, 445, 370);
	FindButton = AddButton(TEXT("Find"), TEXT("部屋をさがす"), 90, 523, 370, true);
	auto* HelpButton = AddButton(TEXT("Help"), TEXT("あそびかた"), 90, 601, 175, true);
	auto* QuitButton = AddButton(TEXT("Quit"), TEXT("ゲームを閉じる"), 280, 601, 180, true);
	auto* Card = WidgetTree->ConstructWidget<UBorder>();
	Card->SetBrush(TBPaper::Brush(PaperTexture));
	Place(Card, 700, 85, 490, 205);
	Card->SetPadding(FMargin(32, 20, 32, 24));
	Card->SetContent(Label(TEXT("しーっ。\nにんげんが来るよ！"), 26, TBPaper::Ink, true));
	Card->SetRenderTransformAngle(3);
	auto* RoomCard = WidgetTree->ConstructWidget<UBorder>();
	RoomCard->SetBrush(TBPaper::Brush(PaperTexture));
	Place(RoomCard, 715, 325, 485, 328);
	RoomCard->SetPadding(FMargin(24));
	auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	RoomCard->SetContent(Column);
	Column->AddChildToVerticalBox(Label(TEXT("みんなの部屋"), 24, TBPaper::Accent, true));
	RoomList = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("Rooms"));
	RoomList->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::RoomLabel);
	Column->AddChildToVerticalBox(RoomList)->SetPadding(FMargin(0, 22, 0, 12));
	JoinButton = AddButton(TEXT("Join"), TEXT("選んだ部屋に入る"), 745, 490, 415);
	CleanupButton = AddButton(TEXT("Cleanup"), TEXT("部屋から退出する"), 745, 567, 415, true);
	StatusText = Label(TEXT(""), 17, TBPaper::Cream);
	Place(StatusText, 90, 685, 1100, 90);
	HostButton->OnClicked.AddDynamic(this, &ThisClass::Host);
	FindButton->OnClicked.AddDynamic(this, &ThisClass::Find);
	JoinButton->OnClicked.AddDynamic(this, &ThisClass::Join);
	CleanupButton->OnClicked.AddDynamic(this, &ThisClass::Cleanup);
	QuitButton->OnClicked.AddDynamic(this, &ThisClass::Quit);
	HelpButton->OnClicked.AddDynamic(this, &ThisClass::Help);
	RoomList->OnSelectionChanged.AddDynamic(this, &ThisClass::SelectionChanged);
}

void UTBTitleMenu::Quit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

UWidget* UTBTitleMenu::RoomLabel(FString Item)
{
	auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(FText::FromString(Item));
	Label->SetFont(FSlateFontInfo(PaperFont, 17));
	Label->SetColorAndOpacity(TBPaper::Ink);
	return Label;
}

void UTBTitleMenu::Help()
{
	StatusText->SetText(FText::FromString(
	    TEXT("おもちゃ：電池を集めて、E長押しで仲間を救助。にんげん：Eでつかみ、箱の前でE長押し。\nWASD 移動 ／ マウス "
	         "見回す ／ Space ジャンプ ／ Shift ダッシュ（おもちゃ） ／ Ctrl しゃがむ（にんげん） ／ Q おろす ／ Esc "
	         "メニュー")));
}
