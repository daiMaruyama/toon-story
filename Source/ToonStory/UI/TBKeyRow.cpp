// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBKeyRow.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Input/TBKeyLabels.h"
#include "TBPaperStyle.h"

void UTBKeyRow::Build(const FString& Label, FName InMappingName, UFont* Font, UTexture2D* Paper)
{
	MappingName = InMappingName;
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
	Place(Text(Label, 20), 0, 10, 230, 36);
	KeyText = Text(TEXT(""), 19);
	KeyText->SetJustification(ETextJustify::Center);
	Place(KeyText, 240, 10, 456, 36);
	ChangeButton = WidgetTree->ConstructWidget<UButton>();
	ChangeButton->SetStyle(TBPaper::ButtonStyle(Paper));
	ChangeButton->SetContent(Text(TEXT("変える"), 19));
	Place(ChangeButton, 716, 3, 84, 44);
	ChangeButton->OnClicked.AddDynamic(this, &ThisClass::Clicked);
}

void UTBKeyRow::Refresh()
{
	KeyText->SetText(FText::FromString(TB::KeyLabel(GetOwningPlayer(), MappingName)));
}

FString UTBKeyRow::GetKeyText() const
{
	return KeyText ? KeyText->GetText().ToString() : FString();
}

void UTBKeyRow::Clicked()
{
	if (Pressed)
	{
		Pressed();
	}
}
