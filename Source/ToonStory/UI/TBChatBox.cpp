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
	// 仮の色。名前は自分とほかの人で分け、出入りのお知らせは山吹色。
	const FLinearColor SelfName(1.f, .74f, .42f), OtherName(.58f, .86f, 1.f), Body(.97f, .95f, .9f), Notice(1.f, .84f, .25f);
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
	// 閉じている間は、入力欄の出る所に開き方を出す。
	OpenHint = WidgetTree->ConstructWidget<UTextBlock>();
	OpenHint->SetText(FText::FromString(TEXT("Enter チャットを開く")));
	OpenHint->SetFont(FSlateFontInfo(PaperFont, 13));
	OpenHint->SetColorAndOpacity(FLinearColor(1, 1, 1, .75f));
	OpenHint->SetShadowOffset(FVector2D(1, 1));
	OpenHint->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, .8f));
	Panel->AddChildToVerticalBox(OpenHint)->SetPadding(FMargin(10, 4, 0, 0));
	SetResultsLayout(false);
	SetOpen(false);
}

void UTBChatBox::SetResultsLayout(bool bInResults)
{
	if (bLayoutApplied && bResults == bInResults)
	{
		return;
	}
	bLayoutApplied = true;
	bResults = bInResults;
	PanelSlot->SetPosition(FVector2D(PanelX, (bResults ? ResultsBottom : LobbyBottom) - PanelH));
}

void UTBChatBox::SetOpen(bool bInOpen)
{
	bOpen = bInOpen;
	Panel->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	LogBack->SetBrushColor(FLinearColor(0, 0, 0, bOpen ? .5f : 0.f));
	InputBack->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	OpenHint->SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (bOpen)
	{
		Input->SetKeyboardFocus();
	}
	Scroll->ScrollToEnd();
}

// 履歴は後ろに足すか、前から消すか、全部消すかだけ。消えた行を前から外し、増えた行を後ろに足す。
void UTBChatBox::Sync(const UTBChatLog& Log)
{
	BuiltRevision = Log.GetRevision();
	const auto& Lines = Log.GetLines();
	const uint64 FirstId = Lines.IsEmpty() ? MAX_uint64 : Lines[0].Id;
	int32 Removed = 0;
	while (Removed < LineIds.Num() && LineIds[Removed] < FirstId)
	{
		LineBox->RemoveChild(LineWidgets[Removed]);
		++Removed;
	}
	LineWidgets.RemoveAt(0, Removed);
	LineIds.RemoveAt(0, Removed);
	LineOpacity.RemoveAt(0, Removed);
	const uint64 LastId = LineIds.IsEmpty() ? 0 : LineIds.Last();
	for (const FTBChatLine& Line : Lines)
	{
		if (Line.Id > LastId)
		{
			AddLine(Line);
		}
	}
	Scroll->ScrollToEnd();
}

void UTBChatBox::AddLine(const FTBChatLine& Line)
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
	if (!Line.bNotice)
	{
		Row->AddChildToHorizontalBox(MakeText(Line.Name + TEXT(": "), Line.bSelf ? SelfName : OtherName));
	}
	auto* Text = MakeText(Line.Text, Line.bNotice ? Notice : Body);
	Text->SetAutoWrapText(true);
	Row->AddChildToHorizontalBox(Text)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LineBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 1));
	LineWidgets.Add(Row);
	LineIds.Add(Line.Id);
	LineOpacity.Add(-1.f); // まだ何も入れていない
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
		Sync(*Log);
	}
	// 開いている間は全部。閉じている間は新しい発言だけを出し、最後の1秒で薄れさせる。変わった行にだけ入れる。
	const double Now = FPlatformTime::Seconds();
	const double ShowSeconds = TB::Rules().Chat.ShowSeconds;
	const auto& Lines = Log->GetLines();
	bool bShownChanged = false;
	for (int32 Index = 0; Index < LineWidgets.Num() && Index < Lines.Num(); ++Index)
	{
		const float Opacity = bOpen ? 1.f : FMath::Clamp(float(ShowSeconds - (Now - Lines[Index].Time)), 0.f, 1.f);
		if (Opacity == LineOpacity[Index])
		{
			continue;
		}
		// 作ったばかりの行（-1）は、見せるか隠すかを必ず入れる。隠すのを忘れると、透明なまま場所を取る。
		if (LineOpacity[Index] < 0 || (Opacity > 0) != (LineOpacity[Index] > 0))
		{
			LineWidgets[Index]->SetVisibility(Opacity > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			bShownChanged = true;
		}
		LineWidgets[Index]->SetRenderOpacity(Opacity);
		LineOpacity[Index] = Opacity;
	}
	if (bShownChanged)
	{
		Scroll->ScrollToEnd();
	}
}

void UTBChatBox::TextChanged(const FText& Text)
{
	const FString Value = Text.ToString();
	const int32 MaxLength = TB::Rules().Chat.MaxLength;
	if (Value.Len() > MaxLength)
	{
		Input->SetText(FText::FromString(TB::Chat::Limit(Value, MaxLength)));
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
