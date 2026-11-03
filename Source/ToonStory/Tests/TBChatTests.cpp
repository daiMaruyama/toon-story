#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Character/TBNPCController.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "UI/TBChatBox.h"
#include "Core/TBChat.h"
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBPlayerState.h"
#include "Data/TBRuleData.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

// 改行と制御文字は空白に、前後は詰める。1回の文字数で切り、絵文字の途中では切らない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBChatCleanTest, "ToonStory.Chat.Clean",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBChatCleanTest::RunTest(const FString& Parameters)
{
	using TB::Chat::Clean;
	const int32 MaxLength = TB::Rules().Chat.MaxLength;
	TestEqual(TEXT("Plain"), Clean(TEXT("よろしく")), FString(TEXT("よろしく")));
	TestEqual(TEXT("Trim"), Clean(TEXT("  gg  ")), FString(TEXT("gg")));
	TestEqual(TEXT("Newline and tab"), Clean(TEXT("a\nb\tc\r")), FString(TEXT("a b c")));
	TestEqual(TEXT("Only spaces"), Clean(TEXT(" \n ")), FString());
	TestEqual(TEXT("Empty"), Clean(TEXT("")), FString());

	const FString Long = FString::ChrN(MaxLength + 5, TEXT('あ'));
	TestEqual(TEXT("Cut at the limit"), Clean(Long).Len(), MaxLength);
	const FString Exact = FString::ChrN(MaxLength, TEXT('a'));
	TestEqual(TEXT("Exactly the limit stays"), Clean(Exact), Exact);
	// 59文字目までが普通の文字で、60〜61文字目が絵文字（サロゲートペア）。半分だけ残さない。
	const FString Emoji = FString::ChrN(MaxLength - 1, TEXT('a')) + TEXT("\U0001F600");
	TestEqual(TEXT("Surrogate pair is not split"), Clean(Emoji), FString::ChrN(MaxLength - 1, TEXT('a')));
	// 切った後ろの空白は詰める。
	const FString SpaceAtCut = FString::ChrN(MaxLength - 1, TEXT('a')) + TEXT(" b");
	TestEqual(TEXT("Trailing space after cut"), Clean(SpaceAtCut), FString::ChrN(MaxLength - 1, TEXT('a')));

	// 見えない幅・向きの文字は消す。それだけの発言は空になる。
	TestEqual(TEXT("Zero width space only"), Clean(TEXT("​​")), FString());
	TestEqual(TEXT("Right-to-left override removed"), Clean(TEXT("a‮b")), FString(TEXT("ab")));
	TestEqual(TEXT("BOM removed"), Clean(TEXT("﻿gg")), FString(TEXT("gg")));

	// 名前の長さでも、絵文字の途中では切らない。
	using TB::Chat::Limit;
	const FString Name = FString::ChrN(TB::Chat::NameLength - 1, TEXT('n')) + TEXT("\U0001F600");
	TestEqual(TEXT("Name keeps the pair whole"), Limit(Name, TB::Chat::NameLength), FString::ChrN(TB::Chat::NameLength - 1, TEXT('n')));
	TestEqual(TEXT("Short text stays"), Limit(TEXT("abc"), 5), FString(TEXT("abc")));
	TestEqual(TEXT("Zero length"), Limit(TEXT("abc"), 0), FString());
	return true;
}

// 遊んでいる最中だけ使えない。連投は間隔で弾き、最初の1回は通す。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBChatRulesTest, "ToonStory.Chat.Rules",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBChatRulesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Lobby"), TB::Chat::IsAvailable(ETBPhase::Lobby));
	TestFalse(TEXT("Playing"), TB::Chat::IsAvailable(ETBPhase::Playing));
	TestTrue(TEXT("Results"), TB::Chat::IsAvailable(ETBPhase::Results));
	TestTrue(TEXT("Aborted"), TB::Chat::IsAvailable(ETBPhase::Aborted));

	TestFalse(TEXT("First message"), TB::Chat::IsTooSoon(-1, 0, 1));
	TestTrue(TEXT("Too soon"), TB::Chat::IsTooSoon(10, 10.5, 1));
	TestFalse(TEXT("Just the interval"), TB::Chat::IsTooSoon(10, 11, 1));
	TestTrue(TEXT("Server is looser than the sender"), TB::Chat::ServerSlack < 1);

	// Steamで遊んでいないときの伏せ字は、そのまま返す。
	TestEqual(TEXT("Filter without a world"), TB::Chat::Filter(nullptr, TEXT("gg"), TEXT("76561197960287930")), FString(TEXT("gg")));
	TestEqual(TEXT("Filter without a sender"), TB::Chat::Filter(nullptr, TEXT("gg"), FString()), FString(TEXT("gg")));
	return true;
}

// 履歴は上限を超えたら古いものから消す。変わるたびにRevisionが進む。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBChatLogTest, "ToonStory.Chat.Log",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBChatLogTest::RunTest(const FString& Parameters)
{
	// GameInstanceSubsystemはGameInstanceの中にしか作れない。
	auto* Log = NewObject<UTBChatLog>(NewObject<UGameInstance>());
	const uint32 Start = Log->GetRevision();
	Log->Add(TEXT("A"), TEXT("hi"), true);
	TestEqual(TEXT("One line"), Log->GetLines().Num(), 1);
	TestTrue(TEXT("Self flag"), Log->GetLines()[0].bSelf);
	TestNotEqual(TEXT("Revision moves on add"), Log->GetRevision(), Start);

	for (int32 Index = 0; Index < TB::Chat::MaxLines + 10; ++Index)
	{
		Log->Add(TEXT("B"), FString::FromInt(Index), false);
	}
	TestEqual(TEXT("Capped"), Log->GetLines().Num(), TB::Chat::MaxLines);
	TestEqual(TEXT("Oldest dropped"), Log->GetLines()[0].Text, FString::FromInt(10));
	TestEqual(TEXT("Newest kept"), Log->GetLines().Last().Text, FString::FromInt(TB::Chat::MaxLines + 9));

	Log->AddNotice(TEXT("notice"));
	TestTrue(TEXT("Notice line"), Log->GetLines().Last().bNotice && Log->GetLines().Last().Name.IsEmpty());
	TestFalse(TEXT("Chat line is not a notice"), Log->GetLines()[0].bNotice);

	const uint32 BeforeClear = Log->GetRevision();
	Log->Clear();
	TestEqual(TEXT("Cleared"), Log->GetLines().Num(), 0);
	TestNotEqual(TEXT("Revision moves on clear"), Log->GetRevision(), BeforeClear);
	return true;
}

// 出入りのお知らせの文言。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBChatNoticeTest, "ToonStory.Chat.Notice",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBChatNoticeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Joined"), TB::Chat::NoticeText(ETBChatNotice::Joined, TEXT("A")), FString(TEXT("Aがあそびにきた！")));
	TestEqual(TEXT("Left"), TB::Chat::NoticeText(ETBChatNotice::Left, TEXT("A")), FString(TEXT("Aがかえっていった")));
	TestEqual(TEXT("Kicked"), TB::Chat::NoticeText(ETBChatNotice::Kicked, TEXT("A")), FString(TEXT("Aはホストにおうちへ帰された")));
	return true;
}

// 閉じている間に行が消えても、行の場所は残す（Collapsedにすると高さが減り、残りの行が一瞬ずれる）。配置そのものは見ない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBChatFadeLayoutTest, "ToonStory.Chat.FadeKeepsLayout",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBChatFadeLayoutTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World || !World->GetGameInstance()) return false;
	auto* Log = World->GetGameInstance()->GetSubsystem<UTBChatLog>();
	auto* Box = CreateWidget<UTBChatBox>(World->GetGameInstance());
	if (!Log || !Box) return false;
	Log->Clear();
	Log->Add(TEXT("A"), TEXT("one"), false);
	Log->Add(TEXT("A"), TEXT("two"), false);
	const double Shown = Log->GetLines().Last().Time;
	TArray<UWidget*> Rows;
	Box->RefreshLines(*Log, Shown);
	Box->WidgetTree->ForEachWidget([&Rows](UWidget* Widget)
	{
		if (Cast<UHorizontalBox>(Widget)) Rows.Add(Widget);
	});
	TestEqual(TEXT("Both lines built"), Rows.Num(), 2);
	for (UWidget* Row : Rows)
	{
		TestEqual(TEXT("Shown line is visible"), Row->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("Shown line is opaque"), Row->GetRenderOpacity(), 1.f);
	}

	Box->RefreshLines(*Log, Shown + TB::Rules().Chat.ShowSeconds + 5);
	for (UWidget* Row : Rows)
	{
		TestNotEqual(TEXT("Expired line keeps its place"), Row->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("Expired line is transparent"), Row->GetRenderOpacity(), 0.f);
	}
	// 場所を残すとスクロール位置が0を超えるので、上下の影（罫線に見える）は描かない設定にする。
	bool bFoundScroll = false;
	Box->WidgetTree->ForEachWidget([this, &bFoundScroll](UWidget* Widget)
	{
		if (const auto* Scroll = Cast<UScrollBox>(Widget))
		{
			bFoundScroll = true;
			TestEqual(TEXT("Top shadow is not drawn"), Scroll->GetWidgetStyle().TopShadowBrush.DrawAs, ESlateBrushDrawType::NoDrawType);
			TestEqual(TEXT("Bottom shadow is not drawn"), Scroll->GetWidgetStyle().BottomShadowBrush.DrawAs, ESlateBrushDrawType::NoDrawType);
		}
	});
	TestTrue(TEXT("Scroll box exists"), bFoundScroll);
	Log->Clear();
	return true;
}

// 抜けたお知らせは本物のプレイヤーだけ。NPCのコントローラーを壊しても（RemoveNPCsと同じ）出さない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBChatLeaveNoticeTest, "ToonStory.Chat.LeaveNotice",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBChatLeaveNoticeTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World || World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ATBGameMode>())
	{
		AddError(FString::Printf(TEXT("Needs a standalone lobby world (world=%s mode=%s)"), World ? *World->GetMapName() : TEXT("none"),
		                         World && World->GetAuthGameMode() ? *World->GetAuthGameMode()->GetClass()->GetName() : TEXT("none")));
		return false;
	}
	auto* Log = World->GetGameInstance()->GetSubsystem<UTBChatLog>();
	if (!Log) return false;

	// 本物が抜けたときは、残った人（このテストの本人）にお知らせが届く。ここが届かないと、下のNPCの確認が意味を持たない。
	const int32 Before = Log->GetLines().Num();
	auto* Real = World->SpawnActor<ATBController>();
	if (!Real || !Real->PlayerState) return false;
	Real->PlayerState->SetPlayerName(TEXT("Real"));
	Real->Destroy();
	TestEqual(TEXT("Real player leave is announced"), Log->GetLines().Num(), Before + 1);
	if (Log->GetLines().Num() > Before)
	{
		TestEqual(TEXT("Real player leave text"), Log->GetLines().Last().Text, TB::Chat::NoticeText(ETBChatNotice::Left, TEXT("Real")));
	}

	const int32 BeforeBot = Log->GetLines().Num();
	auto* Bot = World->SpawnActor<ATBNPCController>();
	auto* Info = Bot ? Bot->GetPlayerState<ATBPlayerState>() : nullptr;
	if (!Info) return false;
	Info->SetIsABot(true);
	Info->SetPlayerName(TEXT("NPC Human 1"));
	Bot->Destroy();
	TestEqual(TEXT("NPC leave is not announced"), Log->GetLines().Num(), BeforeBot);
	return true;
}
#endif
