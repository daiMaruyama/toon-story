// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBOptionRow.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "TBPaperStyle.h"

void UTBOptionRow::Build(const FString& Label, UFont* Font, UTexture2D* Paper, bool bBar)
{
	auto* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(800);
	Size->SetHeightOverride(50);
	WidgetTree->RootWidget = Size;
	auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Size->SetContent(Canvas);
	auto Place = [&](UWidget* Widget, float X, float Y, float W, float H)
	{
		auto* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetPosition(FVector2D(X, Y));
		Slot->SetSize(FVector2D(W, H));
	};
	auto Text = [&](const FString& String, int32 FontSize)
	{
		auto* Block = WidgetTree->ConstructWidget<UTextBlock>();
		Block->SetText(FText::FromString(String));
		Block->SetFont(FSlateFontInfo(Font, FontSize));
		Block->SetColorAndOpacity(TBPaper::Ink);
		return Block;
	};
	auto Arrow = [&](const TCHAR* Glyph, float X)
	{
		auto* Button = WidgetTree->ConstructWidget<UButton>();
		Button->SetStyle(TBPaper::ButtonStyle(Paper));
		Button->SetContent(Text(Glyph, 22));
		Place(Button, X, 3, 84, 44);
		return Button;
	};
	Place(Text(Label, 20), 0, 10, 230, 36);
	DownButton = Arrow(TEXT("<"), 240);
	UpButton = Arrow(TEXT(">"), 716);
	ValueText = Text(TEXT(""), 19);
	if (bBar)
	{
		Bar = WidgetTree->ConstructWidget<UProgressBar>();
		FProgressBarStyle Style;
		Style.BackgroundImage.TintColor = FLinearColor(.78f, .78f, .75f);
		Style.FillImage.TintColor = TBPaper::Accent;
		Bar->SetWidgetStyle(Style);
		Place(Bar, 340, 16, 250, 18);
		Place(ValueText, 606, 10, 102, 36);
	}
	else
	{
		ValueText->SetJustification(ETextJustify::Center);
		Place(ValueText, 330, 10, 380, 36);
	}
	DownButton->OnClicked.AddDynamic(this, &ThisClass::Down);
	UpButton->OnClicked.AddDynamic(this, &ThisClass::Up);
}

void UTBOptionRow::Refresh()
{
	if (Value)
	{
		ValueText->SetText(FText::FromString(Value()));
	}
	if (Bar && Fill)
	{
		Bar->SetPercent(FMath::Clamp(Fill(), 0.f, 1.f));
	}
	DownButton->SetIsEnabled(!CanStep || CanStep(-1));
	UpButton->SetIsEnabled(!CanStep || CanStep(1));
}

void UTBOptionRow::Down() { Move(-1); }
void UTBOptionRow::Up() { Move(1); }

void UTBOptionRow::Move(int32 Steps)
{
	if (Step)
	{
		Step(Steps);
	}
	if (Changed)
	{
		Changed();
	}
	Refresh();
}
