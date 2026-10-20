#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBToyPresentation.h"
#include "Data/TBDataSettings.h"
#include "Data/TBRuleData.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

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
	TestEqual(TEXT("View widens up to 21:9 monitors"), Defaults->Vision.MaxViewAspect, 2.4f);

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
#endif
