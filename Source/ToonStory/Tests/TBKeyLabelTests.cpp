#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Input/TBKeyLabels.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "Misc/ScopeExit.h"
#include "Tests/TBKeyRemapper.h"
#include "UserSettings/EnhancedInputUserSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBFormatKeyTest, "ToonStory.Input.FormatKey",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBFormatKeyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("E"), TB::FormatKey(EKeys::E), FString(TEXT("E")));
	TestEqual(TEXT("F5"), TB::FormatKey(EKeys::F5), FString(TEXT("F5")));
	TestEqual(TEXT("Space"), TB::FormatKey(EKeys::SpaceBar), FString(TEXT("Space")));
	TestEqual(TEXT("Left shift"), TB::FormatKey(EKeys::LeftShift), FString(TEXT("Shift")));
	TestEqual(TEXT("Right shift"), TB::FormatKey(EKeys::RightShift), FString(TEXT("右Shift")));
	TestEqual(TEXT("Left ctrl"), TB::FormatKey(EKeys::LeftControl), FString(TEXT("Ctrl")));
	TestEqual(TEXT("Right ctrl"), TB::FormatKey(EKeys::RightControl), FString(TEXT("右Ctrl")));
	TestEqual(TEXT("Left alt"), TB::FormatKey(EKeys::LeftAlt), FString(TEXT("Alt")));
	TestEqual(TEXT("Right alt"), TB::FormatKey(EKeys::RightAlt), FString(TEXT("右Alt")));
	TestEqual(TEXT("Unassigned"), TB::FormatKey(EKeys::Invalid), FString(TEXT("未割り当て")));
	return true;
}

// {Mappable名}の印を置き換える。前後の文字は壊さず、知らない名前は残す。PCなしでもIMCの既定のキーが出る。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBExpandKeyTokensTest, "ToonStory.Input.ExpandKeyTokens",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBExpandKeyTokensTest::RunTest(const FString& Parameters)
{
	const auto Expand = [](const TCHAR* Text) { return TB::ExpandKeyTokens(nullptr, FString(Text)); };
	TestEqual(TEXT("Known name"), Expand(TEXT("A {Lobby_Ready} B")), FString(TEXT("A R B")));
	TestEqual(TEXT("Two names in a row"), Expand(TEXT("{Toy_Taunt}{Toy_Interact}")), FString(TEXT("FE")));
	TestEqual(TEXT("Japanese around"), Expand(TEXT("準備は{Lobby_Ready}で！")), FString(TEXT("準備はRで！")));
	TestEqual(TEXT("Unknown name stays"), Expand(TEXT("x {Typo} y")), FString(TEXT("x {Typo} y")));
	TestEqual(TEXT("Unclosed brace stays"), Expand(TEXT("x {Lobby_Ready")), FString(TEXT("x {Lobby_Ready")));
	TestEqual(TEXT("No token"), Expand(TEXT("plain text")), FString(TEXT("plain text")));
	TestEqual(TEXT("Empty"), Expand(TEXT("")), FString());

	// 境界。
	TestEqual(TEXT("Open brace only"), Expand(TEXT("{")), FString(TEXT("{")));
	TestEqual(TEXT("Close brace only"), Expand(TEXT("}")), FString(TEXT("}")));
	TestEqual(TEXT("Close before open"), Expand(TEXT("}{")), FString(TEXT("}{")));
	TestEqual(TEXT("Empty braces"), Expand(TEXT("{}")), FString(TEXT("{}")));
	TestEqual(TEXT("Nested braces"), Expand(TEXT("{a{Lobby_Ready}}")), FString(TEXT("{aR}")));
	TestEqual(TEXT("Double open"), Expand(TEXT("{{Lobby_Ready}")), FString(TEXT("{R")));
	TestEqual(TEXT("Double close at the end"), Expand(TEXT("{Lobby_Ready}}")), FString(TEXT("R}")));
	TestEqual(TEXT("Only closes"), Expand(TEXT("}}")), FString(TEXT("}}")));
	// 名前はFNAME_Findで引く。名前の比較は大文字小文字を区別しないので、小文字でも通る。
	TestEqual(TEXT("Lower case name"), Expand(TEXT("{lobby_ready}")), FString(TEXT("R")));
	const FString LongName = FString::ChrN(65, TEXT('a'));
	TestEqual(TEXT("Too long to be a token"), Expand(*(TEXT("{") + LongName + TEXT("}"))), TEXT("{") + LongName + TEXT("}"));

	// サーバーの文字列の印は、全部TB::Keys::Allの名前。スペルミスの{Typo}が画面に出ない。
	for (const TCHAR* Notice : TB::Notices::All())
	{
		const FString Expanded = Expand(Notice);
		TestFalse(*FString::Printf(TEXT("No token is left in: %s"), Notice), Expanded.Contains(TEXT("{")));
	}
	TestEqual(TEXT("Items notice"), Expand(TB::Notices::ItemsAutoCollect),
	          FString(TEXT("Items collect automatically while touching. E is for rescue.")));
	TestEqual(TEXT("Ready notice"), Expand(TB::Notices::ReadyRequired), FString(TEXT("Every player must press R to be ready.")));
	return true;
}

// サーバーからの通知は、ホスト自身も同じ経路（ClientNotice）を通り、自分の割り当てのキー名で出る。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNoticeKeyTest, "ToonStory.Input.NoticeKeys",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNoticeKeyTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	}
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	auto* Input = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	auto* User = Input ? Input->GetUserSettings() : nullptr;
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User)) return false;
	const FString SavedNotice = PC->Notice;
	ON_SCOPE_EXIT
	{
		PC->Notice = SavedNotice;
	};

	// IMCの既定のキーではなく、このプレイヤーの割り当てが出る。取り違えも捕まえる。
	FTBKeyRemapper Remapper(*this, User);
	Remapper.Remap(TB::Keys::LobbyReady, EKeys::T);
	Remapper.Remap(TB::Keys::ToyInteract, EKeys::U);
	PC->ClientNotice(TB::Notices::ReadyRequired, false);
	TestEqual(TEXT("Ready notice follows Lobby_Ready"), PC->Notice, FString(TEXT("Every player must press T to be ready.")));
	PC->ClientNotice(TB::Notices::ItemsAutoCollect, false);
	TestEqual(TEXT("Items notice follows Toy_Interact, not Human_Interact"), PC->Notice,
	          FString(TEXT("Items collect automatically while touching. U is for rescue.")));
	return true;
}

// 登録後の実機で、画面のキー名が実際の割り当てから引けること。割り当てを変えると変わること。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBKeyLabelTest, "ToonStory.Input.KeyLabels",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBKeyLabelTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	}
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	auto* Input = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	auto* User = Input ? Input->GetUserSettings() : nullptr;
	auto* Profile = User ? User->GetActiveKeyProfile() : nullptr;
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User) ||
	    !TestNotNull(TEXT("Active key profile"), Profile))
		return false;

	TestEqual(TEXT("Lobby_Ready"), TB::KeyLabel(PC, TB::Keys::LobbyReady), FString(TEXT("R")));
	TestEqual(TEXT("Lobby_Start"), TB::KeyLabel(PC, TB::Keys::LobbyStart), FString(TEXT("F5")));
	TestEqual(TEXT("Human_Interact"), TB::KeyLabel(PC, TB::Keys::HumanInteract), FString(TEXT("E")));
	TestEqual(TEXT("Toy_Interact"), TB::KeyLabel(PC, TB::Keys::ToyInteract), FString(TEXT("E")));
	TestEqual(TEXT("Toy_Taunt"), TB::KeyLabel(PC, TB::Keys::ToyTaunt), FString(TEXT("F")));
	TestEqual(TEXT("Unknown row"), TB::KeyLabel(PC, TEXT("No_Such_Row")), FString(TEXT("?")));
	TestEqual(TEXT("Move keys"), TB::MoveKeysLabel(PC), FString(TEXT("WASD")));

	// 並べた名前が、登録後のプロファイルの行に全部ある。キーボードとパッドの行の共有も見る。
	for (const FName& Name : TB::Keys::All())
	{
		TArray<FKey> Keys;
		Profile->GetMappedKeysInRow(Name, Keys);
		TestTrue(*FString::Printf(TEXT("%s is a row of the profile"), *Name.ToString()), Keys.Num() > 0);
		const bool bGamepad = Keys.ContainsByPredicate([](const FKey& Key) { return Key.IsGamepadKey(); });
		const bool bKeyboard = Keys.ContainsByPredicate([](const FKey& Key) { return !Key.IsGamepadKey(); });
		AddInfo(FString::Printf(TEXT("row %s: %d keys, keyboard=%d, gamepad=%d"), *Name.ToString(), Keys.Num(), bKeyboard, bGamepad));
		const bool bHasPadRow = Name != TB::Keys::MoveForward && Name != TB::Keys::MoveBack && Name != TB::Keys::MoveLeft &&
		                        Name != TB::Keys::MoveRight && Name != TB::Keys::LobbyReady && Name != TB::Keys::LobbyStart;
		if (bHasPadRow)
		{
			TestTrue(*FString::Printf(TEXT("%s shares one row for keyboard and gamepad"), *Name.ToString()), bKeyboard && bGamepad);
		}
	}

	// UserSettingsが引けないときは、IMCの既定のキーが出る。どこにも無いときだけ「?」。
	TestEqual(TEXT("Fallback to the IMC default"), TB::KeyLabel(nullptr, TB::Keys::LobbyReady), FString(TEXT("R")));
	TestEqual(TEXT("Fallback move keys"), TB::MoveKeysLabel(nullptr), FString(TEXT("WASD")));
	TestEqual(TEXT("Nowhere"), TB::KeyLabel(nullptr, TEXT("No_Such_Row")), FString(TEXT("?")));

	// 割り当てを変えるとキー名が変わる。
	FTBKeyRemapper Remapper(*this, User);
	Remapper.Remap(TB::Keys::LobbyReady, EKeys::T);
	TestEqual(TEXT("Lobby_Ready after remap"), TB::KeyLabel(PC, TB::Keys::LobbyReady), FString(TEXT("T")));

	// 人間と人形の取り違えを捕まえる。片方だけ変えると、もう片方の表示は変わらない。
	Remapper.Remap(TB::Keys::HumanInteract, EKeys::Y);
	Remapper.Remap(TB::Keys::HumanCrouch, EKeys::C);
	Remapper.Remap(TB::Keys::ToySprint, EKeys::G);
	const FString HumanHint = TB::MatchHintText(PC, true);
	const FString ToyHint = TB::MatchHintText(PC, false);
	TestTrue(TEXT("Human hint follows Human_Interact"), HumanHint.Contains(TEXT("Y つかむ・収納")));
	TestTrue(TEXT("Toy hint keeps Toy_Interact"), ToyHint.Contains(TEXT("E 救助")));
	TestTrue(TEXT("Human hint follows Human_Crouch"), HumanHint.Contains(TEXT("C しゃがむ")));
	TestTrue(TEXT("Toy hint follows Toy_Sprint"), ToyHint.Contains(TEXT("G ダッシュ")));
	TestTrue(TEXT("Human hint is not the toy sprint"), !HumanHint.Contains(TEXT("ダッシュ")));
	TestTrue(TEXT("Toy hint is not the human crouch"), !ToyHint.Contains(TEXT("しゃがむ")));

	// 割り当てを外すと「未割り当て」になる。
	Remapper.Remap(TB::Keys::LobbyStart, EKeys::Invalid);
	TestEqual(TEXT("Unassigned row"), TB::KeyLabel(PC, TB::Keys::LobbyStart), FString(TEXT("未割り当て")));
	return true;
}
#endif
