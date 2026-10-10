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

void UTBTitleMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	auto* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(.025f, .045f, .065f, 1));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;
	auto* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(620);
	Background->SetContent(Width);
	auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Width->SetContent(Column);
	auto AddText = [&](const TCHAR* Text, int32 FontSize)
	{
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetAutoWrapText(true);
		Column->AddChildToVerticalBox(Label)->SetPadding(FMargin(8));
		return Label;
	};
	AddText(TEXT("TOON STORY"), 44);
	AddText(TEXT("Create a room or join your friends."), 18);
	auto AddButton = [&](const TCHAR* Name, const TCHAR* Text)
	{
		auto* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(Name));
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		Button->SetContent(Label);
		Column->AddChildToVerticalBox(Button)->SetPadding(FMargin(8, 6));
		return Button;
	};
	HostButton = AddButton(TEXT("Host"), TEXT("Host room"));
	FindButton = AddButton(TEXT("Find"), TEXT("Find rooms / Refresh"));
	RoomList = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("Rooms"));
	Column->AddChildToVerticalBox(RoomList)->SetPadding(FMargin(8, 6));
	JoinButton = AddButton(TEXT("Join"), TEXT("Join selected room"));
	CleanupButton = AddButton(TEXT("Cleanup"), TEXT("Retry connection cleanup"));
	StatusText = AddText(TEXT(""), 16);
	HostButton->OnClicked.AddDynamic(this, &ThisClass::Host);
	FindButton->OnClicked.AddDynamic(this, &ThisClass::Find);
	JoinButton->OnClicked.AddDynamic(this, &ThisClass::Join);
	CleanupButton->OnClicked.AddDynamic(this, &ThisClass::Cleanup);
	RoomList->OnSelectionChanged.AddDynamic(this, &ThisClass::SelectionChanged);
}

void UTBTitleMenu::NativeConstruct()
{
	Super::NativeConstruct();
	Session = GetGameInstance()->GetSubsystem<UTBSession>();
	if (Session) Session->Changed.AddUniqueDynamic(this, &ThisClass::Refresh);
	Refresh(); // A late subscriber also needs the current state.
}

void UTBTitleMenu::NativeDestruct()
{
	if (Session) Session->Changed.RemoveDynamic(this, &ThisClass::Refresh);
	Super::NativeDestruct();
}

void UTBTitleMenu::Refresh()
{
	if (!Session) return;
	const bool Ready = !Session->IsBusy() && !Session->HasSession();
	HostButton->SetIsEnabled(Ready);
	FindButton->SetIsEnabled(Ready);
	RoomList->SetIsEnabled(Ready && !Session->Rooms.IsEmpty());
	// Prefix with the index: two rooms can have identical display names.
	const int32 Previous = RoomList->GetSelectedIndex();
	RoomList->ClearOptions();
	for (int32 Index = 0; Index < Session->Rooms.Num(); ++Index)
		RoomList->AddOption(FString::Printf(TEXT("%d  %s"), Index + 1, *Session->Rooms[Index]));
	if (!Session->Rooms.IsEmpty()) RoomList->SetSelectedIndex(FMath::Clamp(Previous, 0, Session->Rooms.Num() - 1));
	JoinButton->SetIsEnabled(Ready && Session->Rooms.IsValidIndex(RoomList->GetSelectedIndex()));
	CleanupButton->SetVisibility(Session->HasSession() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CleanupButton->SetIsEnabled(!Session->IsBusy());
	StatusText->SetText(FText::FromString(Session->Status));
}

void UTBTitleMenu::SelectionChanged(FString Item, ESelectInfo::Type SelectionType)
{
	if (Session) JoinButton->SetIsEnabled(!Session->IsBusy() && !Session->HasSession() && Session->Rooms.IsValidIndex(RoomList->GetSelectedIndex()));
}
void UTBTitleMenu::Host() { if (Session) Session->Host(); }
void UTBTitleMenu::Find() { if (Session) Session->Find(); }
void UTBTitleMenu::Join() { if (Session) Session->Join(RoomList->GetSelectedIndex()); }
void UTBTitleMenu::Cleanup() { if (Session) Session->Leave(); }
