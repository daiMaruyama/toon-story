// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBTitleMenu.h"
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBGameUserSettings.h"
#include "Core/TBSession.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "TBPaperStyle.h"
#include "Blueprint/WidgetTree.h"

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

void UTBTitleMenu::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	WidgetTree->ForEachWidget([DeltaTime](UWidget* Widget)
	{
		if (auto* Button = Cast<UButton>(Widget))
		{
			const float Target = !Button->GetIsEnabled() ? 0.f : Button->IsPressed() ? -7.f :
			    Button->IsHovered() || Button->HasKeyboardFocus() ? -3.f : 0.f;
			const float Y = FMath::FInterpTo(Button->GetRenderTransform().Translation.Y, Target, DeltaTime, 18.f);
			Button->SetRenderTranslation(FVector2D(0, Y));
			Button->SetRenderScale(FVector2D(1.f - Y * .002f));
		}
	});
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

void UTBTitleMenu::OpenAudio()
{
	PlayMenuClick(GetGameInstance());
	RefreshAudio();
	TitleContent->SetIsEnabled(false);
	AudioOverlay->SetVisibility(ESlateVisibility::Visible);
	CloseAudioButton->SetKeyboardFocus();
}

void UTBTitleMenu::CloseAudio()
{
	PlayMenuClick(GetGameInstance(), ETBSoundEvent::UIBack);
	AudioOverlay->SetVisibility(ESlateVisibility::Collapsed);
	TitleContent->SetIsEnabled(true);
	AudioButton->SetKeyboardFocus();
}

void UTBTitleMenu::ResetAudio()
{
	if (auto* Settings = UTBGameUserSettings::Get())
	{
		Settings->ResetVolumes();
		Settings->CommitVolumes(GetGameInstance());
	}
	PlayMenuClick(GetGameInstance());
	RefreshAudio();
}

void UTBTitleMenu::StepVolume(ETBVolume Kind, int32 Steps)
{
	if (auto* Settings = UTBGameUserSettings::Get())
	{
		Settings->StepVolume(Kind, Steps);
		Settings->CommitVolumes(GetGameInstance());
	}
	// 変えたあとの音量で鳴らして、聞こえ方を確かめられるようにする。
	PlayMenuClick(GetGameInstance());
	RefreshAudio();
}

void UTBTitleMenu::MasterDown() { StepVolume(ETBVolume::Master, -1); }
void UTBTitleMenu::MasterUp() { StepVolume(ETBVolume::Master, 1); }
void UTBTitleMenu::MusicDown() { StepVolume(ETBVolume::Music, -1); }
void UTBTitleMenu::MusicUp() { StepVolume(ETBVolume::Music, 1); }
void UTBTitleMenu::EffectsDown() { StepVolume(ETBVolume::Effects, -1); }
void UTBTitleMenu::EffectsUp() { StepVolume(ETBVolume::Effects, 1); }
void UTBTitleMenu::VoiceDown() { StepVolume(ETBVolume::Voice, -1); }
void UTBTitleMenu::VoiceUp() { StepVolume(ETBVolume::Voice, 1); }

void UTBTitleMenu::RefreshAudio()
{
	const auto* Settings = UTBGameUserSettings::Get();
	if (!Settings) return;
	for (int32 Index = 0; Index < VolumeBars.Num(); ++Index)
	{
		const auto Kind = ETBVolume(Index);
		const auto& Range = TB::ConfigData().Audio.Get(Kind);
		const float Volume = Settings->GetVolume(Kind);
		VolumeBars[Index]->SetPercent(Volume);
		VolumeTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Volume * 100))));
		VolumeDownButtons[Index]->SetIsEnabled(Volume > Range.Clamp(0.f) + KINDA_SMALL_NUMBER);
		VolumeUpButtons[Index]->SetIsEnabled(Volume < Range.Clamp(1.f) - KINDA_SMALL_NUMBER);
	}
}
