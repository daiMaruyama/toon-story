#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBChat.h"
#include "Data/TBRuleData.h"
#include "Engine/GameInstance.h"

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

	const uint32 BeforeClear = Log->GetRevision();
	Log->Clear();
	TestEqual(TEXT("Cleared"), Log->GetLines().Num(), 0);
	TestNotEqual(TEXT("Revision moves on clear"), Log->GetRevision(), BeforeClear);
	return true;
}
#endif
