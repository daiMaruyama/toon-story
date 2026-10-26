#include "Misc/AutomationTest.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/ScopeExit.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Core/TBController.h"
#include "Core/TBGameUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Input/TBInputSettings.h"
#include "Input/TBKeyLabels.h"
#include "Input/TBKeyPrompt.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "Tests/TBKeyRemapper.h"
#include "UI/TBKeyRow.h"
#include "UI/TBOptionsMenu.h"
#include "UI/TBOptionsPages.h"

namespace
{
	ATBController* KeyUiController()
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

	UEnhancedInputUserSettings* KeyUiUser(ATBController* PC)
	{
		auto* Input = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
		return Input ? Input->GetUserSettings() : nullptr;
	}

	UTBKeyRow* FindKeyRow(const UTBOptionsMenu* Menu, FName Name)
	{
		for (UTBKeyRow* Row : Menu->GetKeyRows())
		{
			if (Row->GetMappingName() == Name) return Row;
		}
		return nullptr;
	}

	FString RowText(const UTBOptionsMenu* Menu, FName Name)
	{
		const UTBKeyRow* Row = FindKeyRow(Menu, Name);
		return Row ? Row->GetKeyText() : FString();
	}

	// テスト中は本人の保存ファイルに書かず、保存が呼ばれた回数だけ数える。
	struct FKeySaveGuard
	{
		int32 Count = 0;
		FKeySaveGuard()
		{
			UTBOptionsMenu::KeySaveOverride = [this] { ++Count; };
		}
		~FKeySaveGuard()
		{
			UTBOptionsMenu::KeySaveOverride = nullptr;
		}
	};

	// 13行の今のキーを控え、破棄時に書き戻す。
	bool RememberAllKeys(FTBKeyRemapper& Remapper)
	{
		for (const FName& Name : TB::Keys::All())
		{
			if (!Remapper.Remember(Name)) return false;
		}
		return true;
	}
}

// 表の網羅。TB::Keys::Allがちょうど1回ずつ出て、見出しと表示名は空でない。見出しの並びとIMCの所属が合う。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBKeyTableTest, "ToonStory.Input.KeyTable",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBKeyTableTest::RunTest(const FString& Parameters)
{
	TArray<FName> Seen;
	for (const TB::FKeySection& Section : TB::KeySections())
	{
		TestFalse(TEXT("A heading is not empty"), Section.Heading.IsEmpty());
		for (const TB::FKeyRowInfo& Row : Section.Rows)
		{
			TestFalse(*FString::Printf(TEXT("The label of %s is not empty"), *Row.Name.ToString()), Row.Label.IsEmpty());
			Seen.Add(Row.Name);
		}
	}
	TestEqual(TEXT("The table has one entry per key row"), Seen.Num(), TB::Keys::All().Num());
	for (const FName& Name : TB::Keys::All())
	{
		TestEqual(*FString::Printf(TEXT("%s appears exactly once"), *Name.ToString()), Seen.FilterByPredicate([&Name](const FName& Other) { return Other == Name; }).Num(), 1);
	}

	// 見出し（共通、にんげん、おもちゃ、ロビー）の順は、IMC（Common、Human、Toy、Lobby）の順と同じ。行はその見出しのIMCに入っている。
	const auto* Settings = GetDefault<UTBInputSettings>();
	const TSoftObjectPtr<UInputMappingContext>* Contexts[] = {&Settings->CommonContext, &Settings->HumanContext,
	                                                          &Settings->ToyContext, &Settings->LobbyContext};
	if (!TestEqual(TEXT("One section per IMC"), TB::KeySections().Num(), static_cast<int32>(UE_ARRAY_COUNT(Contexts)))) return false;
	for (int32 Index = 0; Index < TB::KeySections().Num(); ++Index)
	{
		const UInputMappingContext* Context = Contexts[Index]->LoadSynchronous();
		if (!TestNotNull(*FString::Printf(TEXT("The IMC of %s loads"), *TB::KeySections()[Index].Heading), Context)) continue;
		for (const TB::FKeyRowInfo& Row : TB::KeySections()[Index].Rows)
		{
			const bool bInContext = Context->GetMappings().ContainsByPredicate([&Row](const FEnhancedActionKeyMapping& Mapping) {
				const UPlayerMappableKeySettings* Key = Mapping.GetPlayerMappableKeySettings();
				return Key && Key->Name == Row.Name;
			});
			TestTrue(*FString::Printf(TEXT("%s is in the IMC of %s"), *Row.Name.ToString(), *TB::KeySections()[Index].Heading), bInContext);
		}
	}
	return true;
}

// 結果ごとの板の文と、次に画面がすること。画面とは切り離した関数。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBKeyPromptTest, "ToonStory.Input.KeyPrompts",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBKeyPromptTest::RunTest(const FString& Parameters)
{
	using namespace TB;
	const auto Result = [](ERemapResult Kind, std::initializer_list<FName> Conflicting = {})
	{
		FRemapResult Out;
		Out.Result = Kind;
		Out.Conflicting = TArray<FName>(Conflicting);
		return Out;
	};

	TestEqual(TEXT("Row name"), KeyRowName(Keys::LobbyReady), FString(TEXT("ロビー：準備")));
	TestEqual(TEXT("Unknown row name"), KeyRowName(TEXT("No_Such_Row")), FString(TEXT("No_Such_Row")));
	const FString Waiting = WaitingText(Keys::Jump);
	TestEqual(TEXT("Waiting text"), Waiting, FString(TEXT("「共通：ジャンプ」にするキーを押してください　Escで取り消し")));

	TestTrue(TEXT("OK closes the plate"), PromptForRemap(Result(ERemapResult::OK), EKeys::C).Kind == EKeyPromptKind::Done);
	const FKeyPrompt Unusable = PromptForRemap(Result(ERemapResult::Unusable), EKeys::LeftCommand);
	TestTrue(TEXT("Unusable keeps waiting"), Unusable.Kind == EKeyPromptKind::Retry);
	TestEqual(TEXT("Unusable text"), Unusable.Text, FString(TEXT("そのキーは使えません。ほかのキーを押してください")));
	const FKeyPrompt Ask = PromptForRemap(Result(ERemapResult::Conflict, {Keys::LobbyReady}), EKeys::R);
	TestTrue(TEXT("One conflict asks for a swap"), Ask.Kind == EKeyPromptKind::AskSwap);
	TestEqual(TEXT("Swap question"), Ask.Text, FString(TEXT("「R」は「ロビー：準備」が使っています。入れ替えますか？")));
	const FKeyPrompt Multiple = PromptForRemap(Result(ERemapResult::Conflict, {Keys::HumanInteract, Keys::ToyInteract}), EKeys::E);
	TestTrue(TEXT("Two conflicts are a notice"), Multiple.Kind == EKeyPromptKind::Notice);
	TestEqual(TEXT("Two conflicts text names both rows"), Multiple.Text,
	          FString(TEXT("複数の操作と重なるので使えません\n「にんげん：つかむ・収納」「おもちゃ：救助」")));
	const FKeyPrompt Failed = PromptForRemap(Result(ERemapResult::Error), EKeys::C);
	TestTrue(TEXT("Error is a notice"), Failed.Kind == EKeyPromptKind::Notice);
	TestEqual(TEXT("Error text"), Failed.Text, FString(TEXT("変更できませんでした")));

	TestTrue(TEXT("Swap OK closes the plate"), PromptForSwap(Result(ERemapResult::OK)).Kind == EKeyPromptKind::Done);
	TestTrue(TEXT("No partner redoes the remap"), PromptForSwap(Result(ERemapResult::NoPartner)).Kind == EKeyPromptKind::Redo);
	const FKeyPrompt Left = PromptForSwap(Result(ERemapResult::Conflict, {Keys::HumanCrouch}));
	TestTrue(TEXT("A swap that leaves a conflict is a notice"), Left.Kind == EKeyPromptKind::Notice && !Left.Text.IsEmpty());
	TestTrue(TEXT("Swap error is a notice"), PromptForSwap(Result(ERemapResult::Error)).Kind == EKeyPromptKind::Notice);
	return true;
}

// オプション画面（タイトルと試合中は同じ画面）の操作ページに、キーの行が全部あり、それぞれ今のキー名を出す。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBOptionsKeyRowsTest, "ToonStory.Input.OptionsKeyRows",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBOptionsKeyRowsTest::RunTest(const FString& Parameters)
{
	auto* PC = KeyUiController();
	auto* User = KeyUiUser(PC);
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User)) return false;

	auto* Menu = CreateWidget<UTBOptionsMenu>(PC);
	if (!TestNotNull(TEXT("Options menu"), Menu)) return false;
	TestEqual(TEXT("One key row per mappable name"), Menu->GetKeyRows().Num(), TB::Keys::All().Num());
	for (const FName& Name : TB::Keys::All())
	{
		if (TestNotNull(*FString::Printf(TEXT("%s has a row"), *Name.ToString()), FindKeyRow(Menu, Name)))
		{
			TestEqual(*FString::Printf(TEXT("%s shows the current key"), *Name.ToString()), RowText(Menu, Name), TB::KeyLabel(PC, Name));
		}
	}
	TestEqual(TEXT("Jump shows Space"), RowText(Menu, TB::Keys::Jump), FString(TEXT("Space")));
	TestEqual(TEXT("Lobby ready shows R"), RowText(Menu, TB::Keys::LobbyReady), FString(TEXT("R")));

	// 割り当てが変わっても、タブを切り替えれば引き直される。
	FTBKeyRemapper Remapper(*this, User);
	Remapper.Remap(TB::Keys::Jump, EKeys::C);
	TestEqual(TEXT("The row keeps the old key until the page is shown"), RowText(Menu, TB::Keys::Jump), FString(TEXT("Space")));
	Menu->ShowPage(TBOptionsPage::Controls);
	TestEqual(TEXT("Jump shows C after ShowPage"), RowText(Menu, TB::Keys::Jump), FString(TEXT("C")));
	return true;
}

// 「変える」を押して待ちに入り、キーを受け取ると付け替わる。使えないキーでは待ちが続く。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBKeyCaptureTest, "ToonStory.Input.KeyCapture",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBKeyCaptureTest::RunTest(const FString& Parameters)
{
	auto* PC = KeyUiController();
	auto* User = KeyUiUser(PC);
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User)) return false;
	auto* Menu = CreateWidget<UTBOptionsMenu>(PC);
	if (!TestNotNull(TEXT("Options menu"), Menu)) return false;
	FTBKeyRemapper Remapper(*this, User);
	FKeySaveGuard Saves;
	if (!RememberAllKeys(Remapper)) return false;

	// 行を押すと待ちに入り、板に行の名前とEscの案内が出る。
	UTBKeyRow* JumpRow = FindKeyRow(Menu, TB::Keys::Jump);
	if (!TestNotNull(TEXT("Jump row"), JumpRow) || !TestTrue(TEXT("The row has a press handler"), static_cast<bool>(JumpRow->Pressed))) return false;
	JumpRow->Pressed();
	TestTrue(TEXT("Pressing the row starts waiting"), Menu->IsCapturing());
	TestEqual(TEXT("The plate asks for a key"), Menu->GetKeyPromptText(), TB::WaitingText(TB::Keys::Jump));

	// ジャンプをCにする。
	Menu->ReceiveCapturedKey(EKeys::C);
	TestFalse(TEXT("The plate closes after a remap"), Menu->IsCapturing());
	TestEqual(TEXT("Jump is C"), TB::KeyLabel(PC, TB::Keys::Jump), FString(TEXT("C")));
	TestEqual(TEXT("The row shows C"), RowText(Menu, TB::Keys::Jump), FString(TEXT("C")));
	TestEqual(TEXT("A confirmed change is saved once"), Saves.Count, 1);

	// 使えないキーでは待ちが続き、赤い文が出る。次のキーで決まる。
	FindKeyRow(Menu, TB::Keys::HumanCrouch)->Pressed();
	Menu->ReceiveCapturedKey(EKeys::LeftCommand);
	TestTrue(TEXT("An unusable key keeps waiting"), Menu->IsCapturing() && !Menu->IsAwaitingSwap());
	TestEqual(TEXT("The error line"), Menu->GetKeyErrorText(), FString(TEXT("そのキーは使えません。ほかのキーを押してください")));
	TestEqual(TEXT("The plate still asks for a key"), Menu->GetKeyPromptText(), TB::WaitingText(TB::Keys::HumanCrouch));
	Menu->ReceiveCapturedKey(EKeys::K);
	TestFalse(TEXT("A usable key ends the wait"), Menu->IsCapturing());
	TestEqual(TEXT("Human_Crouch is K"), RowText(Menu, TB::Keys::HumanCrouch), FString(TEXT("K")));
	TestEqual(TEXT("The unusable key did not save"), Saves.Count, 2);

	// 待ちの間は、ほかの行を押しても何も起きない。
	JumpRow->Pressed();
	const FString Waiting = Menu->GetKeyPromptText();
	FindKeyRow(Menu, TB::Keys::LobbyStart)->Pressed();
	TestEqual(TEXT("Another row cannot start while waiting"), Menu->GetKeyPromptText(), Waiting);
	Menu->ReceiveCapturedKey(EKeys::Escape);
	TestFalse(TEXT("Esc cancels the wait"), Menu->IsCapturing());
	return true;
}

// かぶりが1行なら入れ替えを聞く。入れ替えると2行が入れ替わる。2行以上のかぶりはお知らせ。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBKeyCaptureSwapTest, "ToonStory.Input.KeyCaptureSwap",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBKeyCaptureSwapTest::RunTest(const FString& Parameters)
{
	auto* PC = KeyUiController();
	auto* User = KeyUiUser(PC);
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User)) return false;
	auto* Menu = CreateWidget<UTBOptionsMenu>(PC);
	if (!TestNotNull(TEXT("Options menu"), Menu)) return false;
	FTBKeyRemapper Remapper(*this, User);
	FKeySaveGuard Saves;
	if (!RememberAllKeys(Remapper)) return false;

	// ジャンプにRを渡すと、ロビーの準備と重なる。入れ替えを聞く。
	FindKeyRow(Menu, TB::Keys::Jump)->Pressed();
	Menu->ReceiveCapturedKey(EKeys::R);
	TestTrue(TEXT("A single conflict asks for a swap"), Menu->IsAwaitingSwap());
	TestEqual(TEXT("The question names the other row"), Menu->GetKeyPromptText(),
	          FString(TEXT("「R」は「ロビー：準備」が使っています。入れ替えますか？")));
	TestEqual(TEXT("Nothing changed before the answer"), TB::KeyLabel(PC, TB::Keys::Jump), FString(TEXT("Space")));
	Menu->ConfirmKeySwap();
	TestFalse(TEXT("The plate closes after the swap"), Menu->IsCapturing());
	TestEqual(TEXT("Jump got R"), RowText(Menu, TB::Keys::Jump), FString(TEXT("R")));
	TestEqual(TEXT("Lobby_Ready got Space"), RowText(Menu, TB::Keys::LobbyReady), FString(TEXT("Space")));

	// 「やめる」（Esc）なら、何も変わらない。
	FindKeyRow(Menu, TB::Keys::HumanInteract)->Pressed();
	Menu->ReceiveCapturedKey(EKeys::Q);
	TestTrue(TEXT("Q asks for a swap with Human_Drop"), Menu->IsAwaitingSwap());
	Menu->ReceiveCapturedKey(EKeys::Escape);
	TestFalse(TEXT("Esc answers no"), Menu->IsCapturing());
	TestEqual(TEXT("Human_Interact is unchanged"), RowText(Menu, TB::Keys::HumanInteract), FString(TEXT("E")));

	// 2行以上と重なるキーはお知らせ。前へ（Common）にEは、人間と人形の2行と重なる。
	FindKeyRow(Menu, TB::Keys::MoveForward)->Pressed();
	Menu->ReceiveCapturedKey(EKeys::E);
	TestTrue(TEXT("Two conflicts show a notice"), Menu->IsCapturing() && !Menu->IsAwaitingSwap());
	TestEqual(TEXT("The notice names both rows"), Menu->GetKeyPromptText(),
	          FString(TEXT("複数の操作と重なるので使えません\n「にんげん：つかむ・収納」「おもちゃ：救助」")));
	// お知らせを閉じるとキー待ちに戻る。もう一度Escで取り消す。
	Menu->ReceiveCapturedKey(EKeys::Escape);
	TestTrue(TEXT("Closing the notice returns to waiting"), Menu->IsCapturing() && !Menu->IsAwaitingSwap());
	TestEqual(TEXT("The plate asks for a key again"), Menu->GetKeyPromptText(), TB::WaitingText(TB::Keys::MoveForward));
	Menu->ReceiveCapturedKey(EKeys::Escape);
	TestFalse(TEXT("Esc again cancels the wait"), Menu->IsCapturing());
	TestEqual(TEXT("Move forward is unchanged"), RowText(Menu, TB::Keys::MoveForward), FString(TEXT("W")));

	// 入れ替える相手がいなくなっていたら（NoPartner）、普通の付け替えをやり直す。
	FindKeyRow(Menu, TB::Keys::HumanCrouch)->Pressed();
	Menu->ReceiveCapturedKey(EKeys::Q);
	TestTrue(TEXT("Q asks for a swap with Human_Drop"), Menu->IsAwaitingSwap());
	Remapper.Remap(TB::Keys::HumanDrop, EKeys::K);
	Menu->ConfirmKeySwap();
	TestFalse(TEXT("The redo finishes the change"), Menu->IsCapturing());
	TestEqual(TEXT("Human_Crouch got Q by the redo"), RowText(Menu, TB::Keys::HumanCrouch), FString(TEXT("Q")));
	TestEqual(TEXT("Human_Drop keeps K"), TB::KeyLabel(PC, TB::Keys::HumanDrop), FString(TEXT("K")));

	// 確認の間は、ほかの行の「変える」も受け付けない。
	FindKeyRow(Menu, TB::Keys::HumanInteract)->Pressed();
	Menu->ReceiveCapturedKey(EKeys::Q);
	TestTrue(TEXT("Q asks for a swap with Human_Crouch"), Menu->IsAwaitingSwap());
	const FString Question = Menu->GetKeyPromptText();
	FindKeyRow(Menu, TB::Keys::LobbyStart)->Pressed();
	TestTrue(TEXT("A new capture is refused while asking"), Menu->IsAwaitingSwap() && Menu->GetKeyPromptText() == Question);
	Menu->ReceiveCapturedKey(EKeys::Escape);
	TestFalse(TEXT("Esc answers no"), Menu->IsCapturing());
	return true;
}

// Escで取り消しても、メニューは開いたまま。待ちの間は閉じるボタンも効かない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBKeyCaptureCancelTest, "ToonStory.Input.KeyCaptureCancel",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBKeyCaptureCancelTest::RunTest(const FString& Parameters)
{
	auto* PC = KeyUiController();
	auto* User = KeyUiUser(PC);
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User)) return false;
	bool bClosed = false;
	auto* Menu = UTBOptionsMenu::Open(PC, [&bClosed] { bClosed = true; });
	if (!TestNotNull(TEXT("Options menu"), Menu)) return false;
	ON_SCOPE_EXIT { Menu->Dismiss(); };
	FTBKeyRemapper Remapper(*this, User);
	FKeySaveGuard Saves;
	if (!RememberAllKeys(Remapper)) return false;

	FindKeyRow(Menu, TB::Keys::Jump)->Pressed();
	Menu->Close();
	TestFalse(TEXT("The close button does nothing while waiting"), bClosed);
	TestTrue(TEXT("Still waiting after the close button"), Menu->IsCapturing());
	Menu->ReceiveCapturedKey(EKeys::Escape);
	TestFalse(TEXT("Esc cancels the wait"), Menu->IsCapturing());
	TestFalse(TEXT("The menu stays open after Esc"), bClosed);
	TestTrue(TEXT("The menu is still on the screen"), Menu->IsInViewport());
	TestEqual(TEXT("Nothing changed"), TB::KeyLabel(PC, TB::Keys::Jump), FString(TEXT("Space")));

	// 待ちを終えたあとは、閉じるボタンが効く。
	Menu->Close();
	TestTrue(TEXT("The menu closes when nothing is pending"), bClosed);
	return true;
}

// 行の「戻す」。かぶりがなければすぐ戻して保存、1行とかぶれば入れ替えを聞き、複数ならお知らせ。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBKeyResetRowTest, "ToonStory.Input.KeyResetRow",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBKeyResetRowTest::RunTest(const FString& Parameters)
{
	auto* PC = KeyUiController();
	auto* User = KeyUiUser(PC);
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User)) return false;
	auto* Menu = CreateWidget<UTBOptionsMenu>(PC);
	if (!TestNotNull(TEXT("Options menu"), Menu)) return false;
	FKeySaveGuard Saves;
	FTBKeyRemapper Remapper(*this, User);
	if (!RememberAllKeys(Remapper)) return false;

	// かぶりなし。Cにしたジャンプをすぐ戻す。
	Remapper.Remap(TB::Keys::Jump, EKeys::C);
	FindKeyRow(Menu, TB::Keys::Jump)->Reset();
	TestFalse(TEXT("No plate when nothing conflicts"), Menu->IsCapturing());
	TestEqual(TEXT("Jump is back to Space"), TB::KeyLabel(PC, TB::Keys::Jump), FString(TEXT("Space")));
	TestEqual(TEXT("The reset is saved"), Saves.Count, 1);

	// 1行とかぶる。ジャンプがRになり、ロビー準備（既定R）が元の座を失う…ロビー準備をSpaceにして、ジャンプを戻す。
	Remapper.Remap(TB::Keys::Jump, EKeys::R);
	Remapper.Remap(TB::Keys::LobbyReady, EKeys::SpaceBar);
	FindKeyRow(Menu, TB::Keys::LobbyReady)->Reset();
	TestTrue(TEXT("A single conflict asks for a swap"), Menu->IsAwaitingSwap());
	TestEqual(TEXT("The question names the other row"), Menu->GetKeyPromptText(),
	          FString(TEXT("「R」は「共通：ジャンプ」が使っています。入れ替えて戻しますか？")));
	TestEqual(TEXT("Nothing changed before the answer"), Saves.Count, 1);
	Menu->ConfirmKeySwap();
	TestFalse(TEXT("The plate closes"), Menu->IsCapturing());
	TestEqual(TEXT("Lobby_Ready is R"), TB::KeyLabel(PC, TB::Keys::LobbyReady), FString(TEXT("R")));
	TestEqual(TEXT("Jump is Space"), TB::KeyLabel(PC, TB::Keys::Jump), FString(TEXT("Space")));
	TestEqual(TEXT("The swap is saved"), Saves.Count, 2);

	// 「やめる」なら何も変わらず、保存もしない。
	Remapper.Remap(TB::Keys::Jump, EKeys::R);
	Remapper.Remap(TB::Keys::LobbyReady, EKeys::SpaceBar);
	FindKeyRow(Menu, TB::Keys::LobbyReady)->Reset();
	Menu->ReceiveCapturedKey(EKeys::Escape);
	TestFalse(TEXT("Esc closes the question"), Menu->IsCapturing());
	TestEqual(TEXT("Nothing changed"), TB::KeyLabel(PC, TB::Keys::LobbyReady), FString(TEXT("Space")));
	TestEqual(TEXT("Nothing was saved"), Saves.Count, 2);

	// 2行以上とかぶるときはお知らせ。閉じたらキー待ちには戻らず終わる。
	Remapper.Remap(TB::Keys::MoveForward, EKeys::R);
	FindKeyRow(Menu, TB::Keys::LobbyReady)->Reset();
	TestTrue(TEXT("Several conflicts show a notice"), Menu->IsCapturing() && !Menu->IsAwaitingSwap());
	TestTrue(TEXT("The notice names the rows"), Menu->GetKeyPromptText().StartsWith(TEXT("複数の操作と重なるので戻せません")));
	Menu->ReceiveCapturedKey(EKeys::Escape);
	TestFalse(TEXT("Closing the notice ends it"), Menu->IsCapturing());
	TestEqual(TEXT("Nothing was saved by the notice"), Saves.Count, 2);
	return true;
}

// 実際のキーイベントの経路（NativeOnPreviewKeyDown）を通す。リピート、Tab、パッドのB、後ろの画面の無効化。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBKeyCaptureEventsTest, "ToonStory.Input.KeyCaptureEvents",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBKeyCaptureEventsTest::RunTest(const FString& Parameters)
{
	auto* PC = KeyUiController();
	auto* User = KeyUiUser(PC);
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User)) return false;
	bool bClosed = false;
	auto* Menu = UTBOptionsMenu::Open(PC, [&bClosed] { bClosed = true; });
	if (!TestNotNull(TEXT("Options menu"), Menu)) return false;
	ON_SCOPE_EXIT { Menu->Dismiss(); };
	FTBKeyRemapper Remapper(*this, User);
	FKeySaveGuard Saves;
	if (!RememberAllKeys(Remapper)) return false;
	auto Down = [Menu](const FKey& Key, bool bRepeat)
	{
		return Menu->SendKeyDownForTest(FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0)).IsEventHandled();
	};
	auto Up = [Menu](const FKey& Key)
	{
		return Menu->SendKeyUpForTest(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0)).IsEventHandled();
	};
	Menu->ShowPage(2);

	// 待ちの間、後ろのページ・タブ・リセット・閉じるは無効。タブの切り替えとリセットとCloseも受け付けない。
	TestTrue(TEXT("The background is enabled at first"), Menu->IsBackgroundEnabled());
	Remapper.Remap(TB::Keys::Jump, EKeys::C);
	FindKeyRow(Menu, TB::Keys::MoveForward)->Pressed();
	TestFalse(TEXT("The background is disabled while waiting"), Menu->IsBackgroundEnabled());
	const int32 PageBefore = Menu->GetPage();
	FOutputDeviceNull Null;
	Menu->CallFunctionByNameWithArguments(TEXT("ShowAudio"), Null, nullptr, true);
	Menu->CallFunctionByNameWithArguments(TEXT("ResetPage"), Null, nullptr, true);
	Menu->Close();
	TestEqual(TEXT("The tab does not switch while waiting"), Menu->GetPage(), PageBefore);
	TestEqual(TEXT("Reset does nothing while waiting"), TB::KeyLabel(PC, TB::Keys::Jump), FString(TEXT("C")));
	TestFalse(TEXT("Close does nothing while waiting"), bClosed);
	bool bVideoChanged = false;
	Menu->ChangeVideoMode([&bVideoChanged](UTBGameUserSettings&) { bVideoChanged = true; });
	TestFalse(TEXT("The video mode is not changed while waiting"), bVideoChanged);

	// パッドのBで取り消し。Escの長押しのリピートは、取り消したあとでもメニューを閉じない。
	TestTrue(TEXT("Pad B is handled"), Down(EKeys::Gamepad_FaceButton_Right, false));
	TestFalse(TEXT("Pad B cancels the wait"), Menu->IsCapturing());
	TestTrue(TEXT("The background is enabled again"), Menu->IsBackgroundEnabled());
	Up(EKeys::Gamepad_FaceButton_Right);
	FindKeyRow(Menu, TB::Keys::MoveForward)->Pressed();
	TestTrue(TEXT("Esc is handled"), Down(EKeys::Escape, false));
	TestFalse(TEXT("Esc cancels the wait"), Menu->IsCapturing());
	TestTrue(TEXT("The Esc repeat is swallowed"), Down(EKeys::Escape, true));
	TestTrue(TEXT("The Esc repeat is swallowed again"), Down(EKeys::Escape, true));
	TestFalse(TEXT("The menu stays open on the Esc repeat"), bClosed);
	Up(EKeys::Escape);
	TestFalse(TEXT("Still open after the Esc is released"), bClosed);

	// 確認の間は、Tabで板の外へ出られない。Enterのリピートでは確定しない。
	FindKeyRow(Menu, TB::Keys::HumanInteract)->Pressed();
	Down(EKeys::Q, false);
	TestTrue(TEXT("Q asks for a swap"), Menu->IsAwaitingSwap());
	TestTrue(TEXT("Tab is swallowed while asking"), Down(EKeys::Tab, false));
	TestTrue(TEXT("The Enter repeat is swallowed"), Down(EKeys::Enter, true));
	TestTrue(TEXT("Still asking after the repeat"), Menu->IsAwaitingSwap());
	TestEqual(TEXT("Nothing was swapped"), RowText(Menu, TB::Keys::HumanInteract), FString(TEXT("E")));
	TestTrue(TEXT("The Q repeat that started the question is swallowed"), Down(EKeys::Q, true));
	TestTrue(TEXT("Still asking after the Q repeat"), Menu->IsAwaitingSwap());
	Up(EKeys::Q);
	Down(EKeys::Escape, false);
	Up(EKeys::Escape);
	TestFalse(TEXT("Esc answers no"), Menu->IsCapturing());
	TestFalse(TEXT("The menu is still open"), bClosed);

	// 待ちが終わったら、Escはふつうにメニューを閉じる。
	Down(EKeys::Escape, false);
	TestTrue(TEXT("Esc closes the menu when nothing is pending"), bClosed);
	return true;
}
#endif
