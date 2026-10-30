// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBTitleMenu.h"
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBSession.h"
#include "Core/TBTitleStory.h"
#include "Components/Button.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "UI/TBOptionsMenu.h"
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
	// 物語はゲーム起動後の最初の1回だけ。寸劇が無ければ、その段階は飛ばす。
	auto* Memory = GetGameInstance()->GetSubsystem<UTBTitleStory>();
	if (Memory && !Memory->bShown)
	{
		EnterStory();
	}
	if (State != EState::Story)
	{
		EnterMenu();
	}
}

namespace
{
	const FName StoryTag(TEXT("TitleStory"));
	const FName LoopTag(TEXT("TitleLoop"));
	constexpr float MenuFadeSeconds = 0.8f;

	// タグの付いた、再生できるシーケンスのアクター。
	ALevelSequenceActor* FindSequence(const UObject* Context, FName Tag)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		if (!World) return nullptr;
		for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag) && It->GetSequencePlayer()) return *It;
		}
		return nullptr;
	}
}

UTexture2D* UTBTitleMenu::MakeSideShade(UObject* Outer)
{
	constexpr int32 Width = 256;
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, 1, PF_B8G8R8A8);
	if (!Texture) return nullptr;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->Filter = TF_Bilinear;
	uint8* Pixels = static_cast<uint8*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	for (int32 X = 0; X < Width; ++X)
	{
		// 左端は濃く、右へなめらかに消える。
		const float T = 1.f - static_cast<float>(X) / (Width - 1);
		const float Alpha = .72f * T * T * (3.f - 2.f * T);
		Pixels[X * 4 + 0] = 5;
		Pixels[X * 4 + 1] = 7;
		Pixels[X * 4 + 2] = 4;
		Pixels[X * 4 + 3] = static_cast<uint8>(Alpha * 255.f);
	}
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

float UTBTitleMenu::GetMenuOpacity() const
{
	return MenuFade;
}

void UTBTitleMenu::SetRoomCardsShown(bool bShown)
{
	bRoomCardsShown = bShown;
	for (UWidget* Widget : RoomCardWidgets)
	{
		Widget->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UTBTitleMenu::EnterStory()
{
	ALevelSequenceActor* Actor = FindSequence(this, StoryTag);
	if (!Actor) return;
	if (auto* Memory = GetGameInstance()->GetSubsystem<UTBTitleStory>()) Memory->bShown = true;
	State = EState::Story;
	if (ALevelSequenceActor* Loop = FindSequence(this, LoopTag)) Loop->GetSequencePlayer()->Stop();
	MenuLayer->SetVisibility(ESlateVisibility::Collapsed);
	HelpOverlay->SetVisibility(ESlateVisibility::Collapsed);
	TitleContent->SetIsEnabled(true);
	StoryActor = Actor;
	Actor->GetSequencePlayer()->OnFinished.AddUniqueDynamic(this, &ThisClass::StoryFinished);
	Actor->GetSequencePlayer()->Play();
	SetKeyboardFocus();
}

void UTBTitleMenu::SkipStory()
{
	if (State != EState::Story) return;
	if (ALevelSequenceActor* Actor = StoryActor.Get())
	{
		Actor->GetSequencePlayer()->OnFinished.RemoveDynamic(this, &ThisClass::StoryFinished);
		Actor->GetSequencePlayer()->Stop();
	}
	EnterMenu();
}

void UTBTitleMenu::StoryFinished()
{
	if (State == EState::Story) EnterMenu();
}

void UTBTitleMenu::WatchStory()
{
	EnterStory();
}

void UTBTitleMenu::StartLoop()
{
	if (ALevelSequenceActor* Loop = FindSequence(this, LoopTag))
	{
		if (!Loop->GetSequencePlayer()->IsPlaying()) Loop->GetSequencePlayer()->Play();
	}
}

void UTBTitleMenu::EnterMenu()
{
	State = EState::Menu;
	if (auto* Memory = GetGameInstance()->GetSubsystem<UTBTitleStory>()) Memory->bShown = true;
	StartLoop();
	MenuFade = 0.f;
	MenuLayer->SetRenderOpacity(0.f);
	MenuLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	HostButton->SetKeyboardFocus();
}

FReply UTBTitleMenu::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (State == EState::Story)
	{
		SkipStory();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(Geometry, MouseEvent);
}

FReply UTBTitleMenu::NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& InputEvent)
{
	if (State == EState::Story && FMath::Abs(InputEvent.GetAnalogValue()) > .5f)
	{
		SkipStory();
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(Geometry, InputEvent);
}

void UTBTitleMenu::NativeDestruct()
{
	if (Session) Session->Changed.RemoveDynamic(this, &ThisClass::Refresh);
	Super::NativeDestruct();
}

void UTBTitleMenu::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (State == EState::Menu && MenuFade < 1.f)
	{
		MenuFade = FMath::Min(1.f, MenuFade + DeltaTime / MenuFadeSeconds);
		MenuLayer->SetRenderOpacity(MenuFade);
	}
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
void UTBTitleMenu::Find()
{
	PlayMenuClick(GetGameInstance());
	SetRoomCardsShown(true);
	if (Session) Session->Find();
}
void UTBTitleMenu::Join() { PlayMenuClick(GetGameInstance()); if (Session) Session->Join(RoomList->GetSelectedIndex()); }
void UTBTitleMenu::Hover() { PlayMenuClick(GetGameInstance(), ETBSoundEvent::UIHover); }
void UTBTitleMenu::Cleanup() { PlayMenuClick(GetGameInstance(), ETBSoundEvent::UIBack); if (Session) Session->Leave(); }

void UTBTitleMenu::OpenOptions()
{
	PlayMenuClick(GetGameInstance());
	TWeakObjectPtr<UButton> Button = OptionsButton;
	UTBOptionsMenu::Open(GetOwningPlayer(), [Button]
	{
		if (Button.IsValid()) Button->SetKeyboardFocus();
	});
}
