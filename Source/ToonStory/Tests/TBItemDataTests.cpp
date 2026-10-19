#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Data/TBNoiseRadioData.h"
#include "GamePlay/Items/TBNoiseRadio.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBItemDataTest, "ToonStory.Data.Items",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBItemDataTest::RunTest(const FString& Parameters)
{
	// C++ の初期値は、移す前にラジオへ直書きしていた値と同じ。
	const auto* Defaults = GetDefault<UTBNoiseRadioData>();
	TestEqual(TEXT("Radio volume"), Defaults->RadioVolume, .6f);
	TestEqual(TEXT("Audible distance"), Defaults->AudibleDistance, 1800.f);
	TestEqual(TEXT("Mask radius"), Defaults->MaskRadius, 1200.f);
	TestEqual(TEXT("Toy action volume near the radio"), Defaults->ToyActionVolume, .25f);
	TestNull(TEXT("Placeholder tone is used until a sound is chosen"), Defaults->LoopSound.Get());

	// 置いたラジオは、何も選ばなくても共通のアセットを使う。
	const auto& Selected = GetDefault<ATBNoiseRadio>()->RadioData;
	TestEqual(TEXT("Radio uses the shared data asset"), Selected.ToSoftObjectPath().ToString(),
	          FString(TEXT("/Game/Data/DA_NoiseRadio.DA_NoiseRadio")));
	TestNotNull(TEXT("Shared radio data asset exists"), Selected.LoadSynchronous());
	return true;
}
#endif
