// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBTitleMenu.h"
#include "Input/TBKeyLabels.h"
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
#include "InputCoreTypes.h"

void UTBTitleMenu::BuildPaperLayout()
{
	PaperFont = TBPaper::Font(this);
	LogoFont = TBPaper::Font(this, true);
	PaperTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/PaperClub/Panel.Panel"));
	// WBP が Root、TitleScale、TitleCanvas を置いていればそれを使う。足りなければ、ここで全部作る。
	auto* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	UScaleBox* Scale = TitleScale;
	UCanvasPanel* Canvas = TitleCanvas;
	if (Root && Scale && Canvas)
	{
		// 暗幕の上に重ねるので、いったん外して、あとで MenuLayer に入れ直す。
		Scale->RemoveFromParent();
		// 配置の基準の枠は 1600×900 に固定する。WBP の上書きが効いていなくても、中身の大きさで枠が変わらない。
		if (auto* Size = Cast<USizeBox>(Canvas->GetParent()))
		{
			Size->SetWidthOverride(1600);
			Size->SetHeightOverride(900);
		}
	}
	else
	{
		// 古いツリーは捨てるので、WBPのバインドも外す。
		HostButton = nullptr;
		FindButton = nullptr;
		HelpButton = nullptr;
		OptionsButton = nullptr;
		QuitButton = nullptr;
		LogoImage = nullptr;
		Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;
		Scale = WidgetTree->ConstructWidget<UScaleBox>();
		Scale->SetStretch(EStretch::ScaleToFit);
		auto* Size = WidgetTree->ConstructWidget<USizeBox>();
		// 配置の基準は 16:9 の 1600×900。画面の比率が違っても、この枠ごと収めるので切れない。
		Size->SetWidthOverride(1600);
		Size->SetHeightOverride(900);
		Scale->SetContent(Size);
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Size->SetContent(Canvas);
	}
	auto Fill = [&](UWidget* Widget)
	{
		auto* Slot = Root->AddChildToCanvas(Widget);
		Slot->SetAnchors(FAnchors(0, 0, 1, 1));
		Slot->SetOffsets(FMargin(0));
	};
	auto Place = [&](UWidget* Widget, float X, float Y, float W, float H)
	{
		auto* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetPosition(FVector2D(X, Y));
		Slot->SetSize(FVector2D(W, H));
	};
	// 後ろの3D（ループの映像）が主役なので、暗幕は敷かない。
	auto* Layer = WidgetTree->ConstructWidget<UCanvasPanel>();
	MenuLayer = Layer;
	Fill(Layer);
	auto* ScaleSlot = Layer->AddChildToCanvas(Scale);
	ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ScaleSlot->SetOffsets(FMargin(0));
	auto Label = [&](const TCHAR* Text, int32 FontSize, FLinearColor Color, bool Logo = false)
	{
		auto* L = WidgetTree->ConstructWidget<UTextBlock>();
		L->SetText(FText::FromString(Text));
		L->SetFont(FSlateFontInfo(Logo ? LogoFont : PaperFont, FontSize));
		L->SetColorAndOpacity(Color);
		L->SetAutoWrapText(true);
		return L;
	};
	// 仮のロゴ（文字）。本物のロゴは WBP の LogoImage に画像を設定すれば、文字は出さずにそちらを使う。
	// 置き場所は左上、横 6〜40%・縦 9〜54%（1600×900 で x 96〜640、y 81〜486）。
	const bool bLogoImage = LogoImage && LogoImage->GetBrush().GetResourceObject();
	if (LogoImage && !bLogoImage)
	{
		LogoImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!bLogoImage)
	{
		auto* Logo = Label(TEXT("TOON"), 90, TBPaper::Cream, true);
		Logo->SetShadowColorAndOpacity(FLinearColor(.16f, .05f, .025f, 1));
		Logo->SetShadowOffset(FVector2D(4, 5));
		Place(Logo, 100, 100, 520, 130);
		Logo->SetRenderTransformAngle(-5);
		auto* Story = Label(TEXT("STORY"), 90, TBPaper::Accent, true);
		Story->SetShadowColorAndOpacity(FLinearColor(.16f, .05f, .025f, 1));
		Story->SetShadowOffset(FVector2D(4, 5));
		Place(Story, 112, 212, 520, 140);
		Story->SetRenderTransformAngle(-5);
	}
	// デザイナー素材の3状態ボタン。文字は画像に入っている。読み込めなければ nullptr（紙のボタンに戻す）。
	// X、Y、W は、画像の透明な余白を除いた絵そのものの左上と幅。Art は元画像での絵の範囲（左、上、右、下の画素）。
	auto TryImageButton = [&](const TCHAR* Name, const TCHAR* Base, FVector2D PixelSize, FVector4 Art, float X, float Y, float W) -> UButton*
	{
		auto Load = [&](const FString& Name)
		{
			return Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr,
			    *FString::Printf(TEXT("/Game/UI/Designer/Imported/%s.%s"), *Name, *Name), nullptr, LOAD_Quiet | LOAD_NoWarn));
		};
		auto LoadState = [&](const TCHAR* Suffix) { return Load(FString::Printf(TEXT("T_UI_Button_%s_%s"), Base, Suffix)); };
		UTexture2D* Normal = LoadState(TEXT("Normal"));
		UTexture2D* Hover = LoadState(TEXT("Hover"));
		UTexture2D* Pressed = LoadState(TEXT("Pressed"));
		// 3状態の画像が無ければ、1枚絵（歯車・退室）として、乗せたときは明るく、押したときは暗くする。
		const bool bSingle = !Normal;
		FLinearColor HoverTint = FLinearColor::White, PressedTint = FLinearColor::White;
		if (bSingle)
		{
			Normal = Load(FString::Printf(TEXT("T_UI_Button_%s"), Base));
			Hover = Normal;
			Pressed = Normal;
			HoverTint = FLinearColor(1.35f, 1.35f, 1.35f, 1);
			PressedTint = FLinearColor(.6f, .6f, .6f, 1);
		}
		if (!Normal || !Hover || !Pressed)
		{
			return nullptr;
		}
		// 元の比率のまま、絵の幅が W になる倍率で描く。テクスチャの大きさは読み込み直後だと仮の値なので、元画像の寸法を使う。
		const float Scale = W / (Art.Z - Art.X);
		const float DrawW = PixelSize.X * Scale;
		const float DrawH = PixelSize.Y * Scale;
		auto Image = [&](UTexture2D* Texture, FLinearColor Tint = FLinearColor::White)
		{
			FSlateBrush Result;
			Result.SetResourceObject(Texture);
			Result.DrawAs = ESlateBrushDrawType::Image;
			Result.ImageSize = FVector2D(DrawW, DrawH);
			Result.TintColor = Tint;
			return Result;
		};
		auto* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(Name));
		FButtonStyle Style;
		Style.SetNormal(Image(Normal));
		Style.SetHovered(Image(Hover, HoverTint));
		Style.SetPressed(Image(Pressed, PressedTint));
		Style.SetDisabled(Image(Normal, FLinearColor(.46f, .46f, .43f)));
		B->SetStyle(Style);
		B->OnHovered.AddDynamic(this, &ThisClass::Hover);
		Place(B, X - Art.X * Scale, Y - Art.Y * Scale, DrawW, DrawH);
		return B;
	};
	auto AddButton = [&](const TCHAR* Name, const TCHAR* Text, float X, float Y, float W, bool Secondary = false,
	                     const TCHAR* ImageBase = nullptr, FVector2D ImagePixels = FVector2D::ZeroVector,
	                     FVector4 ImageArt = FVector4(0, 0, 1, 1))
	{
		if (ImageBase)
		{
			if (auto* ImageButton = TryImageButton(Name, ImageBase, ImagePixels, ImageArt, X, Y, W))
			{
				return ImageButton;
			}
		}
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
	// 配置は参照画像（TitleSample.png、793×448）の割合を、1600×900 の座標にしたもの。
	// 数値は、画像の透明な余白を除いた絵の左上（X、Y）と幅（W）。
	const FVector4 WideArt(22, 28, 412, 146);
	const FVector4 IconArt(10, 12, 152, 150);
	// WBP に置かれたボタンはそのまま使う（見た目と位置は WBP のもの）。無ければここで作る。
	auto Adopt = [&](UButton* Button)
	{
		Button->OnHovered.AddDynamic(this, &ThisClass::Hover);
		return Button;
	};
	HostButton = HostButton ? Adopt(HostButton)
	                        : AddButton(TEXT("HostButton"), TEXT("部屋をつくる"), 201.6f, 529.2f, 332.8f, false, TEXT("HostRoom"), FVector2D(433, 174), WideArt);
	FindButton = FindButton ? Adopt(FindButton)
	                        : AddButton(TEXT("FindButton"), TEXT("部屋をさがす"), 201.6f, 640.9f, 326.4f, true, TEXT("FindRoom"), FVector2D(433, 174), WideArt);
	HelpButton = HelpButton ? Adopt(HelpButton)
	                        : AddButton(TEXT("HelpButton"), TEXT("あそびかた"), 262.4f, 770.f, 201.6f, true, TEXT("HowToPlay"), FVector2D(260, 105),
	                                    FVector4(12, 8, 250, 95));
	OptionsButton = OptionsButton ? Adopt(OptionsButton)
	                              : AddButton(TEXT("OptionsButton"), TEXT("オプション"), 1307.2f, 747.f, 110.4f, true, TEXT("Gear"), FVector2D(160, 160), IconArt);
	QuitButton = QuitButton ? Adopt(QuitButton)
	                        : AddButton(TEXT("QuitButton"), TEXT("ゲームを閉じる"), 1446.4f, 747.f, 113.6f, true, TEXT("Leave"), FVector2D(160, 160), IconArt);
	auto* RoomCard = WidgetTree->ConstructWidget<UBorder>();
	RoomCard->SetBrush(TBPaper::Brush(PaperTexture));
	// 部屋をさがすの右側。ボタン（x 〜530）やアイコン（y 745〜）と重ならない。
	Place(RoomCard, 880, 330, 560, 328);
	RoomCard->SetPadding(FMargin(24));
	auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	RoomCard->SetContent(Column);
	Column->AddChildToVerticalBox(Label(TEXT("みんなの部屋"), 24, TBPaper::Accent, true));
	RoomList = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("Rooms"));
	RoomList->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::RoomLabel);
	Column->AddChildToVerticalBox(RoomList)->SetPadding(FMargin(0, 22, 0, 12));
	JoinButton = AddButton(TEXT("Join"), TEXT("選んだ部屋に入る"), 910, 495, 500);
	// 右のカードは、部屋をさがすときだけ出す。
	RoomCardWidgets = {RoomCard, JoinButton};
	SetRoomCardsShown(false);
	StatusText = Label(TEXT(""), 17, TBPaper::Cream);
	Place(StatusText, 96, 855, 1100, 40);
	HostButton->OnClicked.AddDynamic(this, &ThisClass::Host);
	FindButton->OnClicked.AddDynamic(this, &ThisClass::Find);
	JoinButton->OnClicked.AddDynamic(this, &ThisClass::Join);
	QuitButton->OnClicked.AddDynamic(this, &ThisClass::Quit);
	HelpButton->OnClicked.AddDynamic(this, &ThisClass::Help);
	OptionsButton->OnClicked.AddDynamic(this, &ThisClass::OpenOptions);
	RoomList->OnSelectionChanged.AddDynamic(this, &ThisClass::SelectionChanged);
	TitleContent = Scale;
	auto* Overlay = WidgetTree->ConstructWidget<UCanvasPanel>();
	HelpOverlay = Overlay;
	Fill(Overlay);
	auto* Dim = WidgetTree->ConstructWidget<UImage>();
	Dim->SetColorAndOpacity(FLinearColor(0.015f, .025f, .02f, .78f));
	auto* DimSlot = Overlay->AddChildToCanvas(Dim);
	DimSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	DimSlot->SetOffsets(FMargin(0));
	auto* PopupScale = WidgetTree->ConstructWidget<UScaleBox>();
	PopupScale->SetStretch(EStretch::ScaleToFit);
	auto* PopupSlot = Overlay->AddChildToCanvas(PopupScale);
	PopupSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	PopupSlot->SetOffsets(FMargin(0));
	auto* PopupSize = WidgetTree->ConstructWidget<USizeBox>();
	PopupSize->SetWidthOverride(1280);
	PopupSize->SetHeightOverride(800);
	PopupScale->SetContent(PopupSize);
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	PopupSize->SetContent(Canvas);
	auto* HelpCard = WidgetTree->ConstructWidget<UBorder>();
	HelpCard->SetBrush(TBPaper::Brush(PaperTexture, TBPaper::Cream));
	Place(HelpCard, 170, 60, 940, 680);
	Place(Label(TEXT("あそびかた"), 32, TBPaper::Accent), 220, 90, 840, 55);
	Place(Label(TEXT("おもちゃとにんげんに分かれて、かくれんぼ！"), 20, TBPaper::Ink), 220, 155, 840, 45);
	Place(Label(TEXT("おもちゃ"), 24, TBPaper::Accent), 220, 215, 390, 45);
	HelpToyText = Label(TEXT(""), 19, TBPaper::Ink);
	Place(HelpToyText, 220, 265, 390, 165);
	Place(Label(TEXT("にんげん"), 24, TBPaper::Accent), 660, 215, 390, 45);
	HelpHumanText = Label(TEXT(""), 19, TBPaper::Ink);
	Place(HelpHumanText, 660, 265, 390, 165);
	Place(Label(TEXT("基本操作"), 23, TBPaper::Accent), 220, 445, 840, 40);
	HelpControlsText = Label(TEXT(""), 18, TBPaper::Ink);
	Place(HelpControlsText, 220, 495, 840, 120);
	CloseHelpButton = AddButton(TEXT("CloseHelp"), TEXT("閉じる [Esc]"), 465, 640, 350);
	CloseHelpButton->OnClicked.AddDynamic(this, &ThisClass::CloseHelp);
	HelpOverlay->SetVisibility(ESlateVisibility::Collapsed);
	RefreshHelpText();
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

void UTBTitleMenu::RefreshHelpText()
{
	const auto* PC = GetOwningPlayer();
	const FString Move = TB::MoveKeysLabel(PC), Jump = TB::KeyLabel(PC, TB::Keys::Jump);
	const FString Interact = TB::KeyLabel(PC, TB::Keys::HumanInteract), ToyInteract = TB::KeyLabel(PC, TB::Keys::ToyInteract);
	const FString Drop = TB::KeyLabel(PC, TB::Keys::HumanDrop);
	HelpToyText->SetText(FText::FromString(FString::Printf(
	    TEXT("電池を集めよう。にんげんに見られると動けなくなるよ。\n%sで目の前の仲間や小物を押せるよ。\n仲間が箱に捕まったら、箱のそばで%sを長押しして救助！"),
	    *TB::KeyLabel(PC, TB::Keys::ToyPush), *ToyInteract)));
	HelpHumanText->SetText(FText::FromString(FString::Printf(
	    TEXT("おもちゃに近づいて%sでつかもう。\n箱の正面まで運び、%sを長押しして箱にしまおう。%sでおろせるよ。"), *Interact, *Interact, *Drop)));
	HelpControlsText->SetText(FText::FromString(FString::Printf(
	    TEXT("%s：移動 ／ マウス：見回す ／ %s：ジャンプ\n%s：ダッシュ（おもちゃ） ／ %s：しゃがむ（にんげん）\n%s：つかむ・収納（にんげん） ／ %s：救助（おもちゃ）\n%s：おろす ／ %s：音（おもちゃ） ／ %s：押す ／ Esc：メニュー"),
	    *Move, *Jump, *TB::KeyLabel(PC, TB::Keys::ToySprint), *TB::KeyLabel(PC, TB::Keys::HumanCrouch), *Interact, *ToyInteract,
	    *Drop, *TB::KeyLabel(PC, TB::Keys::ToyTaunt), *TB::KeyLabel(PC, TB::Keys::ToyPush))));
}

void UTBTitleMenu::Help()
{
	RefreshHelpText();
	TitleContent->SetIsEnabled(false);
	HelpOverlay->SetVisibility(ESlateVisibility::Visible);
	CloseHelpButton->SetKeyboardFocus();
}

void UTBTitleMenu::CloseHelp()
{
	HelpOverlay->SetVisibility(ESlateVisibility::Collapsed);
	TitleContent->SetIsEnabled(true);
	HelpButton->SetKeyboardFocus();
}

FReply UTBTitleMenu::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (State == EState::Story)
	{
		SkipStory();
		return FReply::Handled();
	}
	NoteInput();
	if (HelpOverlay && HelpOverlay->GetVisibility() == ESlateVisibility::Visible && KeyEvent.GetKey() == EKeys::Escape)
	{
		CloseHelp();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}
