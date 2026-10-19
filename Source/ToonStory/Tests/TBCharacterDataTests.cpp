#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Data/TBCharacterData.h"
#include "Data/TBDataSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBCharacterDataTest, "ToonStory.Data.Character",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBCharacterDataTest::RunTest(const FString& Parameters)
{
	// C++ の初期値は、移す前に実際に効いていた値と同じ。人形の速さは BP の上書き（歩き40×1.25、走り120）を引き継ぐ。
	const auto* Defaults = GetDefault<UTBCharacterData>();
	TestEqual(TEXT("Human walk"), Defaults->Human.WalkSpeed, 270.f);
	TestEqual(TEXT("Human carry"), Defaults->Human.CarrySpeed, 180.f);
	TestEqual(TEXT("Human crouch"), Defaults->Human.WalkSpeed * Defaults->Human.CrouchSpeedRatio, 270.f * .48f);
	TestEqual(TEXT("Human jump"), Defaults->Human.JumpVelocity, 420.f);
	TestEqual(TEXT("Human step"), Defaults->Human.MaxStepHeight, 45.f);
	TestEqual(TEXT("Toy walk"), Defaults->Toy.WalkSpeed, 40.f * 1.25f);
	TestEqual(TEXT("Toy sprint"), Defaults->Toy.SprintSpeed, 120.f);
	TestEqual(TEXT("Toy jump"), Defaults->Toy.JumpVelocity, 400.f);
	TestEqual(TEXT("Toy step"), Defaults->Toy.MaxStepHeight, 8.f);
	TestEqual(TEXT("Turn rate"), Defaults->View.TurnRate, 540.f);
	TestEqual(TEXT("Step camera"), FVector2D(Defaults->View.StepSmoothSpeed, Defaults->View.StepSmoothMax),
	          FVector2D(12, 60));
	TestEqual(TEXT("Carry camera pitch"), Defaults->CarryCamera.ViewPitch, -40.f);
	TestEqual(TEXT("Carry camera arm"), FVector2D(Defaults->CarryCamera.ArmLength, Defaults->CarryCamera.LookHeight),
	          FVector2D(450, 80));
	TestEqual(TEXT("Carry blend"), FVector2D(Defaults->CarryCamera.BlendTime, Defaults->CarryCamera.ReleaseBlendTime),
	          FVector2D(.45f, .12f));

	// プロジェクト設定でアセットを選んでいれば、初期値ではなくそのアセットを使う。
	const auto& Selected = GetDefault<UTBDataSettings>()->CharacterData;
	if (!Selected.IsNull())
	{
		TestTrue(TEXT("Selected character asset is used"), &TB::CharacterData() == Selected.LoadSynchronous());
	}
	return true;
}
#endif
