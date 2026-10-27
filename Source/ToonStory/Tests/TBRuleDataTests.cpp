#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBToyPresentation.h"
#include "Data/TBDataSettings.h"
#include "Data/TBRuleData.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include <limits>
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRuleDataTest, "ToonStory.Data.Rules",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBRuleDataTest::RunTest(const FString& Parameters)
{
	// C++ の初期値は、マスターデータへ移す前に各クラスへ直書きしていた値と同じ。
	const auto* Defaults = GetDefault<UTBRuleData>();
	TestEqual(TEXT("Horizontal judge angle"), Defaults->Vision.FieldOfView * .5f - Defaults->Vision.EdgeMargin, 40.f);
	TestEqual(TEXT("Body sample ratio"), Defaults->Vision.BodySampleRatio, .85f);
	TestEqual(TEXT("Aim angle"), Defaults->Aim.AimAngle, 45.f);
	TestEqual(TEXT("Capture distance"), Defaults->Aim.CaptureDistance, 180.f);
	TestEqual(TEXT("Grab seconds"), Defaults->Aim.GrabSeconds, .25f);
	TestEqual(TEXT("Drop distance"), Defaults->Aim.DropDistance, 100.f);
	TestEqual(TEXT("Rescue seconds"), Defaults->Box.RescueSeconds, 5.f);
	TestEqual(TEXT("Rescuer bonus"), Defaults->Box.AdditionalRescuerBonus, .5f);
	TestEqual(TEXT("Rescue distance"), Defaults->Box.RescueDistance, 180.f);
	TestEqual(TEXT("Store seconds"), Defaults->Box.StoreSeconds, 3.f);
	TestEqual(TEXT("Store reach"), Defaults->Box.StoreReach, FVector(180, 140, 220));
	TestEqual(TEXT("Drop spacing"), Defaults->Box.DropSpacing, 30.f);
	TestEqual(TEXT("Collect seconds"), Defaults->Pickup.CollectSeconds, 3.f);
	TestEqual(TEXT("Lobby duration range"), FVector(Defaults->Match.MinDuration, Defaults->Match.MaxDuration,
	                                                 Defaults->Match.DurationStep), FVector(60, 3600, 60));
	TestEqual(TEXT("Toy voice reaches humans only nearby"), Defaults->Voice.ToyToHumanDistance, 400.f);
	TestEqual(TEXT("Base screen aspect is 16:9"), Defaults->Vision.AspectRatio(), 16.f / 9.f);

	// プロジェクト設定でアセットを選んでいれば、初期値ではなくそのアセットを使う。
	const auto& Selected = GetDefault<UTBDataSettings>()->RuleData;
	if (!Selected.IsNull())
	{
		TestTrue(TEXT("Selected rule asset is used"), &TB::Rules() == Selected.LoadSynchronous());
	}

	// 使っているデータ（アセットか初期値）を書き換えると、選ばれやすさに反映される。終わったら戻す。
	auto& Rules = const_cast<UTBRuleData&>(TB::Rules());
	const FTBToyRules Saved = Rules.Toys;
	ON_SCOPE_EXIT
	{
		Rules.Toys = Saved;
	};
	Rules.Toys.DefaultToyWeight = 7;
	TestEqual(TEXT("Unlisted look uses the default weight"), TBToyPresentation::SelectionWeight(AActor::StaticClass()), 7);
	Rules.Toys.ToyWeights.Add(TSoftClassPtr<AActor>(AActor::StaticClass()), 2);
	TestEqual(TEXT("Listed look uses its own weight"), TBToyPresentation::SelectionWeight(AActor::StaticClass()), 2);
	Rules.Toys.ToyWeights.Add(TSoftClassPtr<AActor>(AActor::StaticClass()), -3);
	TestEqual(TEXT("Negative weight is never selected"), TBToyPresentation::SelectionWeight(AActor::StaticClass()), 0);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRuleDataValidationTest, "ToonStory.Data.RulesValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBRuleDataValidationTest::RunTest(const FString& Parameters)
{
	auto Passes = [](const UTBRuleData& Rules)
	{
		FDataValidationContext Context;
		return Rules.IsDataValid(Context) == EDataValidationResult::Valid;
	};
	TestTrue(TEXT("Defaults are valid"), Passes(*GetDefault<UTBRuleData>()));
	TestTrue(TEXT("Selected rule asset is valid"), Passes(TB::Rules()));

	auto* Rules = NewObject<UTBRuleData>();
	Rules->Vision.FieldOfView = 80.f;
	TestFalse(TEXT("Aim wider than the screen edge is rejected"), Passes(*Rules));
	Rules->Aim.AimAngle = 40.f;
	TestTrue(TEXT("Aim at the screen edge is accepted"), Passes(*Rules));

	Rules->Match.DefaultDuration = Rules->Match.MaxDuration + 60.f;
	TestFalse(TEXT("Default duration outside the lobby range is rejected"), Passes(*Rules));
	Rules->Match.DefaultDuration = 600.f;
	Rules->Match.MinDuration = Rules->Match.MaxDuration + 60.f;
	TestFalse(TEXT("Reversed duration range is rejected"), Passes(*Rules));
	auto* TauntRules = NewObject<UTBRuleData>();
	for (float Invalid : {-1.f, 0.f, .0999f, std::numeric_limits<float>::quiet_NaN(),
	                      std::numeric_limits<float>::infinity()})
	{
		TauntRules->Toys.TauntCooldownSeconds = Invalid;
		TestFalse(TEXT("Too short or non-finite taunt cooldown is rejected"), Passes(*TauntRules));
	}
	TauntRules->Toys.TauntCooldownSeconds = .1f;
	TestTrue(TEXT("Taunt cooldown at lower bound is accepted"), Passes(*TauntRules));
	TauntRules->Toys.TauntCooldownSeconds = .2f;
	TestTrue(TEXT("Longer taunt cooldown is accepted"), Passes(*TauntRules));
	for (float Invalid : {-1.f, 0.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
	{
		auto* TimingRules = NewObject<UTBRuleData>();
		TimingRules->Match.HidePreparationSeconds = Invalid;
		TestFalse(TEXT("Invalid hiding duration is rejected"), Passes(*TimingRules));
		TimingRules->Match.HidePreparationSeconds = 6.f;
		TestTrue(TEXT("Non-default timing settings are accepted"), Passes(*TimingRules));
	}
	return true;
}
#endif
#endif
