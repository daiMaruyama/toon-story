// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBOptionRow.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
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
		// つまんで動かすほか、バーのどこかを押すとそこへ飛ぶ。細かく合わせるときは「<」「>」を使う。
		Slider = WidgetTree->ConstructWidget<USlider>();
		// マウスでも目盛り（StepSize）にそろえる。表示より前に決める値なので、ここで入れる。
		Slider->MouseUsesStep = true;
		Slider->SetSliderBarColor(FLinearColor(.78f, .78f, .75f));
		Slider->SetSliderHandleColor(TBPaper::Accent);
		Slider->OnValueChanged.AddDynamic(this, &ThisClass::SliderMoved);
		Slider->OnMouseCaptureBegin.AddDynamic(this, &ThisClass::SliderGrabbed);
		Slider->OnMouseCaptureEnd.AddDynamic(this, &ThisClass::SliderReleased);
		Place(Slider, 340, 13, 250, 24);
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
	RefreshText();
	// 動かしている途中は、指の位置を優先する（目盛りへのそろえで手元が跳ねないように）。
	if (Slider && Fill && !bDragging)
	{
		TGuardValue<bool> Guard(bRefreshing, true);
		Slider->SetStepSize(SliderStep > 0.f ? SliderStep : .01f);
		Slider->SetValue(FMath::Clamp(Fill(), 0.f, 1.f));
	}
}

void UTBOptionRow::RefreshText()
{
	if (Value)
	{
		ValueText->SetText(FText::FromString(Value()));
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

void UTBOptionRow::SliderMoved(float Fraction)
{
	if (bRefreshing)
	{
		return;
	}
	if (Slide)
	{
		Slide(Fraction);
	}
	RefreshText();
	// キー操作など、つまんでいないときの変化は、その場で確定する。
	if (!bDragging)
	{
		SliderReleased();
	}
}

void UTBOptionRow::SliderGrabbed()
{
	bDragging = true;
}

void UTBOptionRow::SliderReleased()
{
	bDragging = false;
	if (Released)
	{
		Released();
	}
	if (Changed)
	{
		Changed();
	}
	Refresh();
}
