// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Core/TBController.h"
#include "Core/TBTitleStory.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Misc/ScopeExit.h"
#include "MovieScene.h"
#include "UI/TBOptionsMenu.h"
#include "UI/TBTitleMenu.h"

namespace
{
	ATBController* StoryTestController()
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World())
			{
				return Cast<ATBController>(Context.World()->GetFirstPlayerController());
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

// 物語は最初の1回だけ。寸劇が無ければすぐメニュー。飛ばす入力で黒から明けてメニューへ。メニューを触るとループが止まり、放置すると寸劇。「ストーリーを見る」で戻る。
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
	ON_SCOPE_EXIT
	{
		Memory->bShown = bWasShown;
		for (ALevelSequenceActor* Actor : Spawned) Actor->Destroy();
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
	PressStoryButton(Menu, TEXT("Find"));
	TestTrue(TEXT("Finding rooms shows the cards"), Menu->AreRoomCardsShown());
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

	// メニューを触っても、ループ全体は止めない（止まるのは光を当てたおもちゃだけ）。触ると放置の数え直し。
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
	TestFalse(TEXT("The loop stops for the idle story"), Loop->GetSequencePlayer()->IsPlaying());
	Menu->SkipStory();

	// 「ストーリーを見る」でStoryに戻る。ループは止まる。
	PressStoryButton(Menu, TEXT("Story"));
	TestTrue(TEXT("The story button plays the story"), Menu->GetState() == UTBTitleMenu::EState::Story);
	TestFalse(TEXT("The loop stops during the story"), Loop->GetSequencePlayer()->IsPlaying());
	Menu->SkipStory();
	TestTrue(TEXT("Skipping returns to the menu"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	Menu->RemoveFromParent();

	// 試合から戻ったとき（もう見せた）は、寸劇があってもすぐメニュー。
	Menu = Open();
	TestTrue(TEXT("Coming back goes straight to the menu"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	Menu->RemoveFromParent();
	return true;
}
#endif
