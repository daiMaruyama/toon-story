#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Data/TBDataSettings.h"
#include "Data/TBPresentationData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBPresentationDataTest, "ToonStory.Data.Presentation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBPresentationDataTest::RunTest(const FString& Parameters)
{
	// C++ の初期値は、移す前に実際に効いていた値と同じ。蓋の角度と光は、箱の部屋に置いた蓋の上書き（60°・30000）を引き継ぐ。
	const auto* Defaults = GetDefault<UTBPresentationData>();
	TestEqual(TEXT("Lid open angle"), Defaults->Lid.OpenAngle, 60.f);
	TestEqual(TEXT("Lid light"), Defaults->Lid.PeakIntensity, 30000.f);
	TestEqual(TEXT("Lid timing"), FVector(Defaults->Lid.OpenSeconds, Defaults->Lid.HoldSeconds, Defaults->Lid.CloseSeconds),
	          FVector(.25f, .9f, .8f));
	TestEqual(TEXT("Room flash"), FVector2D(Defaults->Lid.FlashExposure, Defaults->Lid.FlashSeconds), FVector2D(2.5f, .35f));
	TestEqual(TEXT("TV resolution"), Defaults->Television.CaptureResolution, FIntPoint(512, 288));
	TestEqual(TEXT("TV capture rate"), Defaults->Television.CaptureRate, 15.f);
	TestEqual(TEXT("TV channel static"), Defaults->Television.NoiseSeconds, .4f);
	TestEqual(TEXT("Arrival look above"), Defaults->Television.ArrivalLookAbove, 100.f);
	TestEqual(TEXT("Remote cooldown"), Defaults->Remote.PressCooldown, .5f);

	// プロジェクト設定でアセットを選んでいれば、初期値ではなくそのアセットを使う。
	const auto& Selected = GetDefault<UTBDataSettings>()->PresentationData;
	if (!Selected.IsNull())
	{
		TestTrue(TEXT("Selected presentation asset is used"), &TB::PresentationData() == Selected.LoadSynchronous());
	}
	return true;
}
#endif
