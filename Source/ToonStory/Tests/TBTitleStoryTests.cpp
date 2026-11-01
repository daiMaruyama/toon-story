// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "Core/TBSession.h"
#include "Core/TBTitleStory.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/ScopeExit.h"
#include "MovieScene.h"
#include "UI/TBOptionsMenu.h"
#include "UI/TBTitleMenu.h"

namespace
{
	// 試合のマップでもタイトル（ATBTitleController）でも動くよう、型は問わない。
	APlayerController* StoryTestController()
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World())
			{
				return Context.World()->GetFirstPlayerController();
			}
		}
		return nullptr;
	}

	// 10秒のからのシーケンスを持つアクター。
	ALevelSequenceActor* SpawnStoryActor(UWorld* World, FName Tag)
	{
		auto* Actor = World->SpawnActor<ALevelSequenceActor>();
		auto* Sequence = NewObject<ULevelSequence>(GetTransientPackage());
		Sequence->Initialize();
		Sequence->GetMovieScene()->SetPlaybackRange(FFrameNumber(0), 24000 * 10);
		Actor->SetSequence(Sequence);
		Actor->Tags.Add(Tag);
		return Actor;
	}

	void PressStoryButton(UTBTitleMenu* Menu, const TCHAR* Name)
	{
		if (auto* Button = Cast<UButton>(Menu->WidgetTree->FindWidget(FName(Name))))
		{
			Button->OnClicked.Broadcast();
		}
	}
}

// 物語は最初の1回だけ。寸劇が無ければすぐメニュー。飛ばす入力で黒から明けてメニューへ。触ってもループは止めず、放置すると寸劇（オプションや検索の途中は流さない）。「ストーリーを見る」で戻る。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTitleStoryTest, "ToonStory.Title.Story",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBTitleStoryTest::RunTest(const FString& Parameters)
{
	auto* PC = StoryTestController();
	if (!TestNotNull(TEXT("Player controller"), PC)) return false;
	UWorld* World = PC->GetWorld();
	auto* Memory = PC->GetGameInstance()->GetSubsystem<UTBTitleStory>();
	if (!TestNotNull(TEXT("Story memory"), Memory)) return false;
	const bool bWasShown = Memory->bShown;
	TArray<ALevelSequenceActor*> Spawned;
	// タイトルのマップで回すときは、本物の寸劇とループを仮のものと取り違えないよう、テストの間だけ印を外す。
	TArray<TPair<ALevelSequenceActor*, FName>> Untagged;
	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		for (const FName Tag : {FName(TEXT("TitleStory")), FName(TEXT("TitleLoop"))})
		{
			if (It->Tags.Remove(Tag) > 0) Untagged.Emplace(*It, Tag);
		}
	}
	ON_SCOPE_EXIT
	{
		Memory->bShown = bWasShown;
		for (ALevelSequenceActor* Actor : Spawned) Actor->Destroy();
		for (const auto& [Actor, Tag] : Untagged) Actor->Tags.Add(Tag);
	};
	auto Open = [&]
	{
		auto* Menu = CreateWidget<UTBTitleMenu>(PC);
		Menu->AddToViewport();
		return Menu;
	};

	// 寸劇が無いときは、その段階を飛ばしてすぐメニュー。右のカードは隠れている。
	Memory->bShown = false;
	UTBTitleMenu* Menu = Open();
	TestTrue(TEXT("No sequence goes straight to the menu"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	TestTrue(TEXT("The story counts as shown"), Memory->bShown);
	TestFalse(TEXT("The room cards are hidden"), Menu->AreRoomCardsShown());
	PressStoryButton(Menu, TEXT("Story"));
	TestTrue(TEXT("The story button does nothing without a sequence"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	Menu->RemoveFromParent();

	// 寸劇があれば、最初の1回だけStory。どのキーでも飛ばせて、飛ばすと後ろのループが始まる。
	ALevelSequenceActor* Story = SpawnStoryActor(World, TEXT("TitleStory"));
	ALevelSequenceActor* Loop = SpawnStoryActor(World, TEXT("TitleLoop"));
	Spawned = {Story, Loop};
	if (!TestNotNull(TEXT("Sequence players"), Story->GetSequencePlayer()) || !TestNotNull(TEXT("Loop player"), Loop->GetSequencePlayer()))
	{
		return false;
	}
	Memory->bShown = false;
	Menu = Open();
	TestTrue(TEXT("The first time plays the story"), Menu->GetState() == UTBTitleMenu::EState::Story);
	TestTrue(TEXT("The story is playing"), Story->GetSequencePlayer()->IsPlaying());
	TestFalse(TEXT("The loop waits"), Loop->GetSequencePlayer()->IsPlaying());
	TestTrue(TEXT("The story catches clicks to skip"), Menu->IsCatchingClicks());
	const FKeyEvent AnyKey(EKeys::K, FModifierKeysState(), 0, false, 0, 0);
	TestTrue(TEXT("The skip key is handled"), Menu->SendKeyDownForTest(AnyKey).IsEventHandled());
	TestTrue(TEXT("Any key skips to the menu"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	TestFalse(TEXT("The story stopped"), Story->GetSequencePlayer()->IsPlaying());
	TestTrue(TEXT("The loop starts"), Loop->GetSequencePlayer()->IsPlaying());
	TestEqual(TEXT("The menu starts transparent"), Menu->GetMenuOpacity(), 0.f);
	TestEqual(TEXT("Skipping starts from black"), Menu->GetBlackoutOpacity(), 1.f);
	TestFalse(TEXT("The menu lets clicks through to the buttons"), Menu->IsCatchingClicks());

	// メニューを触っても、ループは止めない。触ると放置の数え直し。
	Menu->AdvanceIdleForTest(40.f);
	TestFalse(TEXT("Keys in the menu are left to the buttons"), Menu->SendKeyDownForTest(AnyKey).IsEventHandled());
	TestTrue(TEXT("Touching the menu keeps the loop playing"), Loop->GetSequencePlayer()->IsPlaying());
	Menu->AdvanceIdleForTest(40.f);
	TestTrue(TEXT("Touching restarts the idle count"), Menu->GetState() == UTBTitleMenu::EState::Menu);

	// オプションを開いている間は、放っておいても寸劇で割り込まない。
	PressStoryButton(Menu, TEXT("Options"));
	Menu->AdvanceIdleForTest(60.f);
	TestTrue(TEXT("Reading the options holds off the story"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	TArray<UUserWidget*> Opened;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Opened, UTBOptionsMenu::StaticClass(), false);
	TestEqual(TEXT("The options opened"), Opened.Num(), 1);
	for (UUserWidget* Widget : Opened) Widget->RemoveFromParent();

	// 放っておくと寸劇を流す。
	Menu->AdvanceIdleForTest(60.f);
	TestTrue(TEXT("Idling plays the story"), Menu->GetState() == UTBTitleMenu::EState::Story);
	TestTrue(TEXT("The idle story starts from the top"), Story->GetSequencePlayer()->GetCurrentTime().Time <= Story->GetSequencePlayer()->GetStartTime().Time + 1);
	TestFalse(TEXT("The loop stops for the idle story"), Loop->GetSequencePlayer()->IsPlaying());
	Menu->SkipStory();

	// 「ストーリーを見る」でStoryに戻る。ループは止まる。
	PressStoryButton(Menu, TEXT("Story"));
	TestTrue(TEXT("The story button plays the story"), Menu->GetState() == UTBTitleMenu::EState::Story);
	TestTrue(TEXT("The replayed story starts from the top"), Story->GetSequencePlayer()->GetCurrentTime().Time <= Story->GetSequencePlayer()->GetStartTime().Time + 1);
	TestFalse(TEXT("The loop stops during the story"), Loop->GetSequencePlayer()->IsPlaying());
	Menu->SkipStory();
	TestTrue(TEXT("Skipping returns to the menu"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	Menu->RemoveFromParent();

	// 試合から戻ったとき（もう見せた）は、寸劇があってもすぐメニュー。
	Menu = Open();
	TestTrue(TEXT("Coming back goes straight to the menu"), Menu->GetState() == UTBTitleMenu::EState::Menu);

	// 部屋をさがすとカードが出る。探している間は、放っておいても寸劇で割り込まない。検索は終わるのを待たないので最後に。
	PressStoryButton(Menu, TEXT("Find"));
	TestTrue(TEXT("Finding rooms shows the cards"), Menu->AreRoomCardsShown());
	if (PC->GetGameInstance()->GetSubsystem<UTBSession>()->IsBusy())
	{
		Menu->AdvanceIdleForTest(60.f);
		TestTrue(TEXT("Searching holds off the story"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	}
	Menu->RemoveFromParent();
	return true;
}

// 本物のタイトル: 寸劇とループとタイトル用カメラがそろい、寸劇を飛ばすと最後のコマで止まって、ループとメニューに移る。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTitleMapTest, "ToonStory.Title.Map",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBTitleMapTest::RunTest(const FString& Parameters)
{
	auto* PC = StoryTestController();
	if (!TestNotNull(TEXT("Player controller"), PC)) return false;
	UWorld* World = PC->GetWorld();
	if (!World->GetMapName().EndsWith(TEXT("Title")))
	{
		AddInfo(TEXT("Run with /Game/Maps/Title -game to check the real title."));
		return true;
	}
	auto Find = [World](FName Tag) -> ALevelSequenceActor*
	{
		for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag)) return *It;
		}
		return nullptr;
	};
	ALevelSequenceActor* Story = Find(TEXT("TitleStory"));
	ALevelSequenceActor* Loop = Find(TEXT("TitleLoop"));
	if (!TestNotNull(TEXT("Story actor"), Story) || !TestNotNull(TEXT("Loop actor"), Loop)) return false;
	TestNotNull(TEXT("Story sequence"), Story->GetSequence());
	TestNotNull(TEXT("Loop sequence"), Loop->GetSequence());
	TestTrue(TEXT("The story keeps its last frame"),
	         Story->PlaybackSettings.FinishCompletionStateOverride == EMovieSceneCompletionModeOverride::ForceKeepState);
	bool bTitleCamera = false;
	for (TActorIterator<AActor> It(World); It; ++It) bTitleCamera |= It->ActorHasTag(TEXT("TitleCamera"));
	TestTrue(TEXT("A camera is tagged for the title"), bTitleCamera);
	TArray<UUserWidget*> Menus;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Menus, UTBTitleMenu::StaticClass(), false);
	if (!TestEqual(TEXT("One title menu"), Menus.Num(), 1)) return false;
	auto* Menu = Cast<UTBTitleMenu>(Menus[0]);
	if (Menu->GetState() == UTBTitleMenu::EState::Menu) Menu->EnterStory();
	TestTrue(TEXT("The real story plays"), Menu->GetState() == UTBTitleMenu::EState::Story && Story->GetSequencePlayer()->IsPlaying());
	Menu->SkipStory();
	ULevelSequencePlayer* Player = Story->GetSequencePlayer();
	TestFalse(TEXT("Skipping stops the story"), Player->IsPlaying());
	TestTrue(TEXT("Skipping lands on the last frame"),
	         Player->GetCurrentTime().Time.FrameNumber >= Player->GetEndTime().Time.FrameNumber - 1);
	TestTrue(TEXT("The real loop plays behind the menu"), Loop->GetSequencePlayer()->IsPlaying());
	TestTrue(TEXT("The menu is up"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	return true;
}
#endif
