#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Core/TBKeyLabels.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayTagContainer.h"
#include "Misc/ScopeExit.h"
#include "UserSettings/EnhancedInputUserSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBFormatKeyTest, "ToonStory.Input.FormatKey",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBFormatKeyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("E"), TB::FormatKey(EKeys::E), FString(TEXT("E")));
	TestEqual(TEXT("F5"), TB::FormatKey(EKeys::F5), FString(TEXT("F5")));
	TestEqual(TEXT("Space"), TB::FormatKey(EKeys::SpaceBar), FString(TEXT("Space")));
	TestEqual(TEXT("Left shift"), TB::FormatKey(EKeys::LeftShift), FString(TEXT("Shift")));
	TestEqual(TEXT("Right shift"), TB::FormatKey(EKeys::RightShift), FString(TEXT("Shift")));
	TestEqual(TEXT("Left ctrl"), TB::FormatKey(EKeys::LeftControl), FString(TEXT("Ctrl")));
	TestEqual(TEXT("Right ctrl"), TB::FormatKey(EKeys::RightControl), FString(TEXT("Ctrl")));
	TestEqual(TEXT("Unassigned"), TB::FormatKey(EKeys::Invalid), FString(TEXT("未割り当て")));
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

	// 割り当てを変えるとキー名が変わる。保存はしない。変える前のキーを控え、最後に書き戻す。
	struct FSavedKey
	{
		FName Name;
		EPlayerMappableKeySlot Slot;
		FKey Key;
	};
	TArray<FSavedKey> Saved;
	const auto KeyboardMapping = [Profile](FName Name) -> const FPlayerKeyMapping*
	{
		const FKeyMappingRow* Row = Profile->GetPlayerMappingRows().Find(Name);
		if (!Row) return nullptr;
		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			if (!Mapping.GetCurrentKey().IsGamepadKey()) return &Mapping;
		}
		return nullptr;
	};
	const auto MakeArgs = [Profile](FName Name, EPlayerMappableKeySlot Slot, const FKey& Key)
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = Name;
		Args.Slot = Slot;
		Args.NewKey = Key;
		Args.ProfileIdString = Profile->GetProfileIdString();
		return Args;
	};
	ON_SCOPE_EXIT
	{
		for (const FSavedKey& Item : Saved)
		{
			FGameplayTagContainer Failure;
			User->MapPlayerKey(MakeArgs(Item.Name, Item.Slot, Item.Key), Failure);
		}
	};
	const auto Remap = [&](FName Name, const FKey& NewKey)
	{
		const FPlayerKeyMapping* Current = KeyboardMapping(Name);
		if (!TestNotNull(*FString::Printf(TEXT("%s has a keyboard slot"), *Name.ToString()), Current)) return;
		Saved.Add({Name, Current->GetSlot(), Current->GetCurrentKey()});
		FGameplayTagContainer Failure;
		User->MapPlayerKey(MakeArgs(Name, Current->GetSlot(), NewKey), Failure);
	};

	Remap(TB::Keys::LobbyReady, EKeys::T);
	TestEqual(TEXT("Lobby_Ready after remap"), TB::KeyLabel(PC, TB::Keys::LobbyReady), FString(TEXT("T")));

	// 人間と人形の取り違えを捕まえる。片方だけ変えると、もう片方の表示は変わらない。
	Remap(TB::Keys::HumanInteract, EKeys::Y);
	Remap(TB::Keys::HumanCrouch, EKeys::C);
	Remap(TB::Keys::ToySprint, EKeys::G);
	const FString HumanHint = TB::MatchHintText(PC, true);
	const FString ToyHint = TB::MatchHintText(PC, false);
	TestTrue(TEXT("Human hint follows Human_Interact"), HumanHint.Contains(TEXT("Y つかむ・収納")));
	TestTrue(TEXT("Toy hint keeps Toy_Interact"), ToyHint.Contains(TEXT("E 救助")));
	TestTrue(TEXT("Human hint follows Human_Crouch"), HumanHint.Contains(TEXT("C しゃがむ")));
	TestTrue(TEXT("Toy hint follows Toy_Sprint"), ToyHint.Contains(TEXT("G ダッシュ")));
	TestTrue(TEXT("Human hint is not the toy sprint"), !HumanHint.Contains(TEXT("ダッシュ")));
	TestTrue(TEXT("Toy hint is not the human crouch"), !ToyHint.Contains(TEXT("しゃがむ")));

	// 割り当てを外すと「未割り当て」になる。
	Remap(TB::Keys::LobbyStart, EKeys::Invalid);
	TestEqual(TEXT("Unassigned row"), TB::KeyLabel(PC, TB::Keys::LobbyStart), FString(TEXT("未割り当て")));
	return true;
}
#endif
