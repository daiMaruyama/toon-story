// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBTitleMenu.h"
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBSession.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "TBPaperStyle.h"

void UTBTitleMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildPaperLayout();
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
	const bool Ready = !Session->IsBusy();
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
	CleanupButton->SetIsEnabled(!Session->IsBusy());
	StatusText->SetText(FText::FromString(TBPaper::Status(Session->Status)));
}

void UTBTitleMenu::SelectionChanged(FString Item, ESelectInfo::Type SelectionType)
{
	if (SelectionType != ESelectInfo::Direct) Hover();
	if (Session) JoinButton->SetIsEnabled(!Session->IsBusy() && Session->Rooms.IsValidIndex(RoomList->GetSelectedIndex()));
}
namespace
{
	void PlayMenuClick(UGameInstance* GameInstance, ETBSoundEvent Event = ETBSoundEvent::UIClick)
	{
		if (GameInstance)
			if (auto* Audio = GameInstance->GetSubsystem<UTBAudioSubsystem>()) Audio->PlaySoundEvent(Event);
	}
}

void UTBTitleMenu::Host() { PlayMenuClick(GetGameInstance()); if (Session) Session->Host(); }
void UTBTitleMenu::Find() { PlayMenuClick(GetGameInstance()); if (Session) Session->Find(); }
void UTBTitleMenu::Join() { PlayMenuClick(GetGameInstance()); if (Session) Session->Join(RoomList->GetSelectedIndex()); }
void UTBTitleMenu::Hover() { PlayMenuClick(GetGameInstance(), ETBSoundEvent::UIHover); }
void UTBTitleMenu::Cleanup() { PlayMenuClick(GetGameInstance(), ETBSoundEvent::UIBack); if (Session) Session->Leave(); }
