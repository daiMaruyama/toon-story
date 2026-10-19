#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Character/TBToyPresentation.h"
#include "Core/TBGameHelpers.h"
#include "Data/TBDataSettings.h"
#include "Data/TBRuleData.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
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

	// ゲームのカメラは、判定と同じ比率に固定される。
	auto* Camera = NewObject<UCameraComponent>();
	TB::ConstrainToRuleAspect(Camera);
	TestTrue(TEXT("Camera is constrained to the rule aspect"), Camera->bConstrainAspectRatio);
	TestEqual(TEXT("Camera uses the rule aspect"), Camera->AspectRatio, TB::Rules().Vision.AspectRatio());

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
	return true;
}
#endif
#endif
