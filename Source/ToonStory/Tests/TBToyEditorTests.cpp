#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Data/TBToyDefinition.h"
#include "Animation/BlendSpace.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBToyEditorSetupTest, "ToonStory.Appearance.EditorSetup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBToyEditorSetupTest::RunTest(const FString& Parameters)
{
	auto* Source = LoadObject<UTBToyDefinition>(nullptr,
		TEXT("/Game/Characters/ToyDefinitions/DA_SpaceRanger_Realistic.DA_SpaceRanger_Realistic"));
	if (!TestNotNull(TEXT("Source toy definition"), Source)) return false;
	const FString Name = FString::Printf(TEXT("DA_ToySetupTest_%s"), *FGuid::NewGuid().ToString());
	auto* Package = CreatePackage(*(TEXT("/Game/") + Name));
	auto* Definition = NewObject<UTBToyDefinition>(Package, *Name, RF_Transactional);
	FString Message;
	TestFalse(TEXT("Missing mesh rejected"), Definition->ValidateDefinition(Message));
	Definition->SkeletalMesh = Source->SkeletalMesh;
	Definition->Idle = Source->Idle;
	Definition->Walk = Source->Walk;
	Definition->Run = Source->Run;
	Definition->WalkSpeed = 30.f;
	Definition->RunSpeed = 20.f;
	Definition->CreateLocomotion();
	TestNull(TEXT("Invalid speeds cannot create assets"), Definition->Locomotion.Get());
	Definition->RunSpeed = 120.f;
	Definition->CreateLocomotion();
	auto* Blend = Definition->Locomotion.Get();
	if (TestNotNull(TEXT("Editor action creates locomotion without Python"), Blend))
	{
		TestEqual(TEXT("Three animation samples"), Blend->GetNumberOfBlendSamples(), 3);
		TestEqual(TEXT("Axis matches run speed"), Blend->GetBlendParameter(0).Max, 120.f);
		TestTrue(TEXT("Created definition validates"), Definition->ValidateDefinition(Message));
		Definition->CreateLocomotion();
		TestEqual(TEXT("Repeated action preserves existing asset"), Definition->Locomotion.Get(), Blend);
		for (float Speed : {0.f, 30.f, 120.f})
		{
			TArray<FBlendSampleData> Samples;
			int32 Segment = INDEX_NONE;
			TestTrue(TEXT("Created blend space evaluates"), Blend->GetSamplesFromBlendInput(FVector(Speed, 0, 0), Samples, Segment, true) && Samples.Num() > 0);
		}
		// The test creates in-memory assets only; keep Content and user materials untouched.
		FAssetRegistryModule::AssetDeleted(Blend);
		Blend->GetOutermost()->SetDirtyFlag(false);
		Blend->ClearFlags(RF_Public | RF_Standalone);
	}
	Package->SetDirtyFlag(false);
	return true;
}
#endif
