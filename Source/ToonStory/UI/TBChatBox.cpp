// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBChatBox.h"
#include "TBPaperStyle.h"
#include "Audio/TBAudioSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/TBChat.h"
#include "Core/TBController.h"
#include "Data/TBRuleData.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"

namespace
{
	// 仮の配置（本番のUIが決まるまで）。1280x800の板の上で左下。ロビーは下の板のすぐ上、結果画面は角。
	constexpr float PanelX = 68, PanelW = 520, PanelH = 220;
	constexpr float LobbyBottom = 636, ResultsBottom = 776;
	const FLinearColor SelfName(1.f, .74f, .42f), OtherName(.58f, .86f, 1.f), Body(.97f, .95f, .9f);
}

void UTBChatBox::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	PaperFont = TBPaper::Font(this);
	// 板の外のクリックはHUDのボタンへ通す。
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit);
	auto* ScaleSlot = Root->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ScaleSlot->SetOffsets(FMargin(0));
	auto* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(1280);
	Size->SetHeightOverride(800);
	Scale->SetContent(Size);
	auto* Board = WidgetTree->ConstructWidget<UCanvasPanel>();
	Size->SetContent(Board);
	for (UWidget* Layer : TArray<UWidget*>{Root, Scale, Size, Board})
	{
		Layer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	Panel = WidgetTree->ConstructWidget<UVerticalBox>();
	PanelSlot = Board->AddChildToCanvas(Panel);
	PanelSlot->SetSize(FVector2D(PanelW, PanelH));
	LogBack = WidgetTree->ConstructWidget<UBorder>();
	LogBack->SetPadding(FMargin(10, 6));
	LogBack->SetVerticalAlignment(VAlign_Bottom);
	Panel->AddChildToVerticalBox(LogBack)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	LogBack->SetContent(Scroll);
	LineBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(LineBox);

	InputBack = WidgetTree->ConstructWidget<UBorder>();
	InputBack->SetBrushColor(TBPaper::Cream);
	InputBack->SetPadding(FMargin(10, 6));
	Panel->AddChildToVerticalBox(InputBack)->SetPadding(FMargin(0, 4, 0, 0));
	Input = WidgetTree->ConstructWidget<UEditableText>();
	Input->SetFont(FSlateFontInfo(PaperFont, 15));
	FEditableTextStyle Style = Input->WidgetStyle;
	Style.SetColorAndOpacity(TBPaper::Ink);
	Input->SetWidgetStyle(Style);
	Input->SetHintText(FText::FromString(FString::Printf(TEXT("Enterで送る ／ Escでやめる（%d文字まで）"), TB::Rules().Chat.MaxLength)));
	// 早すぎて送れなかったときも、打ち直さずに続けられるようにフォーカスを残す。
	Input->SetClearKeyboardFocusOnCommit(false);
	Input->OnTextChanged.AddDynamic(this, &UTBChatBox::TextChanged);
	Input->OnTextCommitted.AddDynamic(this, &UTBChatBox::TextCommitted);
	InputBack->SetContent(Input);
	SetResultsLayout(false);
	SetOpen(false);
}

void UTBChatBox::SetResultsLayout(bool bInResults)
{
	bResults = bInResults;
	PanelSlot->SetPosition(FVector2D(PanelX, (bResults ? ResultsBottom : LobbyBottom) - PanelH));
}

void UTBChatBox::SetOpen(bool bInOpen)
{
	bOpen = bInOpen;
	Panel->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	LogBack->SetBrushColor(FLinearColor(0, 0, 0, bOpen ? .5f : 0.f));
	InputBack->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bOpen)
	{
		Input->SetKeyboardFocus();
	}
	Scroll->ScrollToEnd();
}

void UTBChatBox::Rebuild()
{
	LineBox->ClearChildren();
	LineWidgets.Reset();
	const auto* Log = GetGameInstance()->GetSubsystem<UTBChatLog>();
	BuiltRevision = Log->GetRevision();
	for (const FTBChatLine& Line : Log->GetLines())
	{
		auto MakeText = [this](const FString& Value, FLinearColor Color)
		{
			auto* Block = WidgetTree->ConstructWidget<UTextBlock>();
			Block->SetText(FText::FromString(Value));
			Block->SetFont(FSlateFontInfo(PaperFont, 15));
			Block->SetColorAndOpacity(Color);
			Block->SetShadowOffset(FVector2D(1, 1));
			Block->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, .8f));
			return Block;
		};
		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Row->AddChildToHorizontalBox(MakeText(Line.Name + TEXT(": "), Line.bSelf ? SelfName : OtherName));
		auto* Text = MakeText(Line.Text, Body);
		Text->SetAutoWrapText(true);
		Row->AddChildToHorizontalBox(Text)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LineBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 1));
		LineWidgets.Add(Row);
	}
	Scroll->ScrollToEnd();
}

void UTBChatBox::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	const auto* Log = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTBChatLog>() : nullptr;
	if (!Log)
	{
		return;
	}
	if (Log->GetRevision() != BuiltRevision)
	{
		Rebuild();
	}
	// 開いている間は全部。閉じている間は新しい発言だけを出し、最後の1秒で薄れさせる。
	const double Now = FPlatformTime::Seconds();
	const double ShowSeconds = TB::Rules().Chat.ShowSeconds;
	const auto& Lines = Log->GetLines();
	for (int32 Index = 0; Index < LineWidgets.Num() && Index < Lines.Num(); ++Index)
	{
		const double Left = ShowSeconds - (Now - Lines[Index].Time);
		LineWidgets[Index]->SetVisibility(bOpen || Left > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		LineWidgets[Index]->SetRenderOpacity(bOpen ? 1.f : FMath::Clamp(float(Left), 0.f, 1.f));
	}
	if (!bOpen)
	{
		Scroll->ScrollToEnd();
	}
}

void UTBChatBox::TextChanged(const FText& Text)
{
	const FString Value = Text.ToString();
	if (Value.Len() > TB::Rules().Chat.MaxLength)
	{
		Input->SetText(FText::FromString(TB::Chat::Limit(Value)));
	}
}

void UTBChatBox::TextCommitted(const FText& Text, ETextCommit::Type Method)
{
	auto* PC = Cast<ATBController>(GetOwningPlayer());
	// 閉じたあとのフォーカス移動でも呼ばれる。
	if (!bOpen || !PC)
	{
		return;
	}
	if (Method != ETextCommit::OnEnter)
	{
		PC->CloseChat(); // ほかをクリックした。書きかけは残す
		return;
	}
	const FString Message = TB::Chat::Clean(Text.ToString());
	if (!Message.IsEmpty() && !PC->SendChat(Message))
	{
		if (auto* Audio = PC->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>())
		{
			Audio->PlaySoundEvent(ETBSoundEvent::UIError);
		}
		return;
	}
	Input->SetText(FText::GetEmpty());
	PC->CloseChat();
}

FReply UTBChatBox::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (bOpen && KeyEvent.GetKey() == EKeys::Escape)
	{
		Input->SetText(FText::GetEmpty());
		if (auto* PC = Cast<ATBController>(GetOwningPlayer()))
		{
			PC->CloseChat();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

// 打っている間のキーは、入力欄が使わなかったものもゲーム（準備・開始など）へ渡さない。
FReply UTBChatBox::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	return bOpen ? FReply::Handled() : Super::NativeOnKeyDown(Geometry, KeyEvent);
}

FReply UTBChatBox::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	return bOpen ? FReply::Handled() : Super::NativeOnKeyUp(Geometry, KeyEvent);
}
