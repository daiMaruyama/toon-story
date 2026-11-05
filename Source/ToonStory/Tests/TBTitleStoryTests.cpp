// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "GameFramework/PlayerController.h"
#include "Character/TBToyFacingPose.h"
#include "Character/TBToyPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/TBSession.h"
#include "Core/TBTitleEmote.h"
#include "Core/TBTitleGameMode.h"
#include "Core/TBTitleStory.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
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
	Menu->EnterStory();
	TestTrue(TEXT("Entering the story does nothing without a sequence"), Menu->GetState() == UTBTitleMenu::EState::Menu);
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
	PressStoryButton(Menu, TEXT("OptionsButton"));
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

	// 見直すときは EnterStory で Story に戻る。ループは止まる。
	Menu->EnterStory();
	TestTrue(TEXT("Entering the story plays the story"), Menu->GetState() == UTBTitleMenu::EState::Story);
	TestTrue(TEXT("The replayed story starts from the top"), Story->GetSequencePlayer()->GetCurrentTime().Time <= Story->GetSequencePlayer()->GetStartTime().Time + 1);
	TestFalse(TEXT("The loop stops during the story"), Loop->GetSequencePlayer()->IsPlaying());
	Menu->SkipStory();
	TestTrue(TEXT("Skipping returns to the menu"), Menu->GetState() == UTBTitleMenu::EState::Menu);
	Menu->RemoveFromParent();

	// 試合から戻ったとき（もう見せた）は、寸劇があってもすぐメニュー。
	Menu = Open();
	TestTrue(TEXT("Coming back goes straight to the menu"), Menu->GetState() == UTBTitleMenu::EState::Menu);

	// 部屋をさがすとカードが出る。探している間は、放っておいても寸劇で割り込まない。検索は終わるのを待たないので最後に。
	PressStoryButton(Menu, TEXT("FindButton"));
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

// ループのおもちゃはゲームと同じエモートを出す。クリックした1体だけ次の種類へ回り、子どもに見られている間は固まって変わらない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTitleToyEmoteTest, "ToonStory.Title.ToyEmote",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBTitleToyEmoteTest::RunTest(const FString& Parameters)
{
	auto* PC = Cast<ATBTitleController>(StoryTestController());
	if (!PC || !PC->GetWorld()->GetMapName().EndsWith(TEXT("Title")))
	{
		AddInfo(TEXT("Run with /Game/Maps/Title -game to check the title toys."));
		return true;
	}
	UWorld* World = PC->GetWorld();
	TArray<UUserWidget*> Menus;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Menus, UTBTitleMenu::StaticClass(), false);
	// 前のテストが作ったメニューも残るので、画面に出ているものを使う。
	UUserWidget** Shown = Menus.FindByPredicate([](const UUserWidget* Widget) { return Widget->IsInViewport(); });
	if (!TestNotNull(TEXT("The title menu is shown"), Shown)) return false;
	auto* Menu = Cast<UTBTitleMenu>(*Shown);
	// 寸劇を最後のコマまで飛ばして、タイトルのカメラを本番の位置にする。
	if (Menu->GetState() == UTBTitleMenu::EState::Menu) Menu->EnterStory();
	if (Menu->GetState() == UTBTitleMenu::EState::Story) Menu->SkipStory();
	ALevelSequenceActor* Loop = nullptr;
	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("TitleLoop"))) Loop = *It;
	}
	if (!TestNotNull(TEXT("Loop actor"), Loop)) return false;
	TArray<UTBTitleEmote*> Toys;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("TitleToy")))
		{
			auto* Emote = It->FindComponentByClass<UTBTitleEmote>();
			TestNotNull(*FString::Printf(TEXT("%s can emote"), *It->GetActorLabel()), Emote);
			if (Emote) Toys.Add(Emote);
		}
	}
	if (!TestTrue(TEXT("Title toys"), Toys.Num() > 0)) return false;
	auto Step = [&](float Seconds)
	{
		for (float T = 0; T < Seconds; T += .05f)
		{
			for (auto* Toy : Toys) Toy->TickComponent(.05f, LEVELTICK_All, nullptr);
		}
	};
	auto Jump = [&](int32 Frame)
	{
		Loop->GetSequencePlayer()->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(Frame), EUpdatePositionMethod::Jump));
		if (!Loop->GetSequencePlayer()->IsPlaying()) Loop->GetSequencePlayer()->Play();
	};
	// 踊っている間は全員がエモートを出し、骨を回す姿勢ができる。
	Jump(200);
	Step(1.f);
	for (auto* Toy : Toys)
	{
		USkeletalMeshComponent* Mesh = nullptr;
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Toy->GetOwner());
		for (auto* Part : Parts)
		{
			if (Part && Part->IsVisible() && !Part->bHiddenInGame && !Part->LeaderPoseComponent.IsValid())
			{
				Mesh = Part;
				break;
			}
		}
		if (!TestNotNull(TEXT("Visible toy mesh"), Mesh)) continue;
		const FString MeshName = Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetName() : FString();
		if (MeshName.Contains(TEXT("Duck")) || MeshName.Contains(TEXT("TeddyBear")) || MeshName.Contains(TEXT("TinRobot")))
		{
			// アヒル・テディ・ロボットは体ごと動く。見えているメッシュの位置か向きが変わる。
			TestTrue(*FString::Printf(TEXT("%s moves its whole body"), *MeshName), Toy->UsesBodyMoves());
			// 始まりはランダムで、ジャンプの後半は止まって見えるので、動く途中に揃えてから比べる。
			Toy->SetMidJumpForTest();
			Step(.05f);
			const FTransform Before = Mesh->GetRelativeTransform();
			Step(.3f);
			TestFalse(*FString::Printf(TEXT("%s body moved"), *MeshName), Mesh->GetRelativeTransform().Equals(Before, .01f));
			// 腕は肩が割れない角度だけ振り、頭と胴の骨は回さない。
			FTBEmotePose Swing;
			Toy->BuildArmSwing(Mesh, Swing);
			for (const auto& Bone : Swing.Bones)
			{
				TestTrue(TEXT("Only arms swing"), Bone.Name == TEXT("arm_L") || Bone.Name == TEXT("arm_R"));
				TestTrue(TEXT("Arm swings stay small"), FMath::RadiansToDegrees(Bone.Rotation.GetAngle()) <= 12.5f);
			}
			continue;
		}
		FTBEmoteSample Sample;
		TestTrue(TEXT("Dancing toys emote"), Toy->GetSample(Sample) && Sample.Weight > .9f);
		// 骨を回す後処理が実際に付いていないと、状態だけ動いて見た目は変わらない。
		TestNotNull(*FString::Printf(TEXT("%s runs the emote post-process"), *Toy->GetOwner()->GetActorLabel()),
		            Mesh ? Cast<UTBToyFacingPose>(Mesh->GetPostProcessInstance()) : nullptr);
		FTBEmotePose Pose;
		TBToyPresentation::BuildEmotePose(Mesh, Sample, Toy->GetOwner()->GetActorForwardVector(), Toy->GetOwner()->GetActorRightVector(), Pose);
		TestTrue(*FString::Printf(TEXT("%s gets an emote pose"), *Toy->GetOwner()->GetActorLabel()), Pose.Bones.Num() > 0);
	}
	// カメラから狙ったおもちゃだけが選ばれ、4種類を順に回る。
	UTBTitleEmote* Target = Toys[0];
	// 前のテストで視点が変わっていることがあるので、カメラの方を向いているおもちゃの正面から狙う。
	FVector Center, Extent;
	Target->GetOwner()->GetActorBounds(false, Center, Extent);
	const FVector Eye = Center + Target->GetOwner()->GetActorForwardVector() * 60.;
	UTBTitleEmote* Picked = PC->PickToy(Eye, (Center - Eye).GetSafeNormal());
	TestNotNull(TEXT("A ray toward a toy picks a toy"), Picked);
	if (Picked) Target = Picked;
	TArray<ETBEmoteKind> Others;
	for (auto* Toy : Toys) Others.Add(Toy->GetKind());
	const ETBEmoteKind First = Target->GetKind();
	TSet<ETBEmoteKind> Seen{First};
	for (int32 i = 0; i < 4; ++i)
	{
		TestTrue(TEXT("Clicking a dancing toy is accepted"), Target->Next());
		Step(.5f);
		Seen.Add(Target->GetKind());
	}
	TestEqual(TEXT("Four clicks go round all four emotes"), Seen.Num(), 4);
	TestTrue(TEXT("Four clicks come back to the first emote"), Target->GetKind() == First);
	for (int32 i = 0; i < Toys.Num(); ++i)
	{
		if (Toys[i] != Target) TestTrue(TEXT("Other toys keep their emote"), Toys[i]->GetKind() == Others[i]);
	}
	// タイトルのカメラから見て、どのおもちゃも、その体の中心を押せばそのおもちゃが選ばれる（手前の箱に吸われない）。
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("TitleCamera"))) continue;
		for (auto* Toy : Toys)
		{
			FVector ToyCenter, ToyExtent;
			Toy->GetOwner()->GetActorBounds(false, ToyCenter, ToyExtent);
			const FVector Camera = It->GetActorLocation();
			TestTrue(*FString::Printf(TEXT("Clicking %s picks it"), *Toy->GetOwner()->GetActorLabel()),
			         PC->PickToy(Camera, (ToyCenter - Camera).GetSafeNormal()) == Toy);
		}
		break;
	}
	// ドアが開いている間は固まり、クリックしても変わらない。
	Jump(500);
	Step(.2f);
	TestTrue(TEXT("Toys hold while the child watches"), Target->IsHolding());
	TestFalse(TEXT("A watched toy ignores clicks"), Target->Next());
	// ジャンプの途中で固まっても、空中で止まらない。高さだけ床まで下りて、明けたらなめらかに動きへ戻る。
	UTBTitleEmote* Jumper = nullptr;
	for (auto* Toy : Toys)
	{
		if (Toy->UsesBodyMoves()) { Jumper = Toy; break; }
	}
	if (TestNotNull(TEXT("A toy that moves its whole body"), Jumper))
	{
		Jump(200);
		Step(.3f);
		Jumper->SetMidJumpForTest();
		Step(.05f);
		const float InAir = Jumper->GetBodyLift();
		TestTrue(TEXT("The toy is in the air before holding"), InAir > 5.f);
		Jump(500);
		Step(.05f);
		TestTrue(TEXT("The toy holds"), Jumper->IsHolding());
		TestTrue(TEXT("The height eases down, not a jump cut"), Jumper->GetBodyLift() > 0.f && Jumper->GetBodyLift() < InAir);
		Step(1.f);
		TestTrue(TEXT("A holding toy rests on the floor"), FMath::Abs(Jumper->GetBodyLift()) < .5f);
		Jump(800);
		Step(.05f);
		TestTrue(TEXT("The height does not snap back when the hold ends"), Jumper->GetBodyLift() < 3.f);
		Step(1.f);
	}
	// 子どもが去ったら、全員また動き出す（止まったままにならない）。
	Jump(800);
	Step(1.f);
	for (auto* Toy : Toys)
	{
		TestFalse(TEXT("Toys stop holding after the child leaves"), Toy->IsHolding());
		if (Toy->UsesBodyMoves()) continue;
		FTBEmoteSample A, B;
		Toy->GetSample(A);
		Step(.2f);
		Toy->GetSample(B);
		TestTrue(TEXT("Emotes keep going after the child leaves"), A.Weight > .9f && !FMath::IsNearlyEqual(A.Progress, B.Progress));
	}
	// ループが止まれば（寸劇に入れば）すぐ消える。
	Loop->GetSequencePlayer()->Stop();
	Step(.05f);
	FTBEmoteSample After;
	TestFalse(TEXT("No emote outside the loop"), Target->GetSample(After));
	Loop->GetSequencePlayer()->Play();
	return true;
}
#endif
