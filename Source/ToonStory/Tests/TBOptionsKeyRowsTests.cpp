#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Core/TBController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Input/TBKeyLabels.h"
#include "Tests/TBKeyRemapper.h"
#include "UI/TBKeyRow.h"
#include "UI/TBOptionsMenu.h"

// 表の網羅。TB::Keys::Allの13名がちょうど1回ずつ出て、見出しと表示名は空でない。
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
	return true;
}

// オプション画面（タイトルと試合中は同じ画面）の操作ページに、キーの行が13個あり、それぞれ今のキー名を出す。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBOptionsKeyRowsTest, "ToonStory.Input.OptionsKeyRows",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBOptionsKeyRowsTest::RunTest(const FString& Parameters)
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

	auto* Menu = CreateWidget<UTBOptionsMenu>(PC);
	if (!TestNotNull(TEXT("Options menu"), Menu)) return false;
	const TArray<TObjectPtr<UTBKeyRow>>& Rows = Menu->GetKeyRows();
	TestEqual(TEXT("13 key rows"), Rows.Num(), 13);
	for (const FName& Name : TB::Keys::All())
	{
		const TObjectPtr<UTBKeyRow>* Found = Rows.FindByPredicate([&Name](const UTBKeyRow* Row) { return Row->GetMappingName() == Name; });
		if (TestTrue(*FString::Printf(TEXT("%s has a row"), *Name.ToString()), Found != nullptr))
		{
			TestEqual(*FString::Printf(TEXT("%s shows the current key"), *Name.ToString()), (*Found)->GetKeyText(), TB::KeyLabel(PC, Name));
		}
	}
	const auto TextOf = [&Rows](FName Name)
	{
		const TObjectPtr<UTBKeyRow>* Found = Rows.FindByPredicate([&Name](const UTBKeyRow* Row) { return Row->GetMappingName() == Name; });
		return Found ? (*Found)->GetKeyText() : FString();
	};
	TestEqual(TEXT("Jump shows Space"), TextOf(TB::Keys::Jump), FString(TEXT("Space")));
	TestEqual(TEXT("Lobby ready shows R"), TextOf(TB::Keys::LobbyReady), FString(TEXT("R")));

	// 割り当てが変われば、引き直したときに変わる。
	FTBKeyRemapper Remapper(*this, User);
	Remapper.Remap(TB::Keys::Jump, EKeys::C);
	for (UTBKeyRow* Row : Rows)
	{
		Row->Refresh();
	}
	TestEqual(TEXT("Jump shows C after a refresh"), TextOf(TB::Keys::Jump), FString(TEXT("C")));
	return true;
}
#endif
