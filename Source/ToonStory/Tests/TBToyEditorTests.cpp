#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Data/TBToyDefinition.h"
#include "Animation/BlendSpace.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBToySeatedIdleTest, "ToonStory.Appearance.SeatedIdle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBToySeatedIdleTest::RunTest(const FString& Parameters)
{
	auto* Bear = LoadObject<UTBToyDefinition>(nullptr,
		TEXT("/Game/Characters/ToyDefinitions/DA_TeddyBear_Realistic.DA_TeddyBear_Realistic"));
	if (!TestNotNull(TEXT("Bear definition"), Bear) || !Bear->Idle || !Bear->Locomotion) return false;
	const auto* Model = Bear->Idle->GetDataModel();
	const auto& Ref = Bear->SkeletalMesh->GetRefSkeleton();
	TArray<FTransform> Pose;
	for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
	{
		const FName Name = Ref.GetBoneName(Bone);
		if (!TestTrue(TEXT("Seated idle animates every bone"), Model->IsValidBoneTrackName(Name))) return false;
		auto Transform = Model->GetBoneTrackTransform(Name, FFrameNumber(0));
		TestTrue(TEXT("Idle loop closes without a pose jump"), Transform.Equals(
			Model->GetBoneTrackTransform(Name, FFrameNumber(Model->GetNumberOfFrames())), .001f));
		if (Ref.GetParentIndex(Bone) >= 0) Transform *= Pose[Ref.GetParentIndex(Bone)];
		Pose.Add(Transform);
	}
	const int32 Hip = Ref.FindBoneIndex(TEXT("pelvis"));
	for (const TCHAR* FootName : {TEXT("foot_L"), TEXT("foot_R")})
	{
		const int32 Foot = Ref.FindBoneIndex(FootName);
		if (Hip == INDEX_NONE || Foot == INDEX_NONE) return false;
		TestTrue(TEXT("Seated feet extend ahead of hips"), Pose[Foot].GetLocation().X - Pose[Hip].GetLocation().X > 2.f);
	}
	TArray<FBlendSampleData> Samples; int32 Segment = INDEX_NONE;
	Bear->Locomotion->GetSamplesFromBlendInput(FVector::ZeroVector, Samples, Segment, true);
	if (!TestEqual(TEXT("Rest has one idle sample"), Samples.Num(), 1)) return false;
	TestEqual(TEXT("Rest plays the saved seated clip"), Bear->Locomotion->GetBlendSample(Samples[0].SampleDataIndex).Animation.Get(), Bear->Idle.Get());
	TestTrue(TEXT("Body uses cloth shading"), Bear->SkeletalMesh->GetMaterials()[0].MaterialInterface->GetMaterial()->GetShadingModels().HasShadingModel(MSM_Cloth));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBToyGaitTest, "ToonStory.Appearance.TunedGait",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBToyGaitTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Name : {TEXT("ArmyMan_StarterPack"), TEXT("SpaceRanger_Realistic")})
	{
		auto* Definition = LoadObject<UTBToyDefinition>(nullptr, *FString::Printf(TEXT("/Game/Characters/ToyDefinitions/DA_%s.DA_%s"), Name, Name));
		if (!TestNotNull(TEXT("Tuned definition exists"), Definition)) return false;
		const bool bArmy = FString(Name).StartsWith(TEXT("Army"));
		const auto& Ref = Definition->SkeletalMesh->GetRefSkeleton();
		TArray<FTransform> Reference;
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			auto T = Ref.GetRefBonePose()[Bone];
			if (Ref.GetParentIndex(Bone) >= 0) T *= Reference[Ref.GetParentIndex(Bone)];
			Reference.Add(T);
		}
		for (UAnimSequence* Clip : {Definition->Walk.Get(), Definition->Run.Get()})
		{
			const auto* Model = Clip->GetDataModel();
			double MaxArmAngle = 0;
			for (int32 Frame = 0; Frame <= Model->GetNumberOfFrames(); ++Frame)
			{
				TArray<FTransform> Pose;
				for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
				{
					auto T = Model->GetBoneTrackTransform(Ref.GetBoneName(Bone), FFrameNumber(Frame));
					if (Ref.GetParentIndex(Bone) >= 0) T *= Pose[Ref.GetParentIndex(Bone)];
					Pose.Add(T);
				}
				for (const TCHAR* Side : {TEXT("L"), TEXT("R")})
				{
					if (bArmy)
					{
						const int32 Hip = Ref.FindBoneIndex(*FString::Printf(TEXT("thigh_%s"), Side));
						const int32 Knee = Ref.FindBoneIndex(*FString::Printf(TEXT("calf_%s"), Side));
						const int32 Foot = Ref.FindBoneIndex(*FString::Printf(TEXT("foot_%s"), Side));
						TestTrue(TEXT("Knee stays in the hip/ankle forward plane"), FMath::Abs(Pose[Hip].GetLocation().Y - Pose[Knee].GetLocation().Y) < .001 && FMath::Abs(Pose[Knee].GetLocation().Y - Pose[Foot].GetLocation().Y) < .001);
						const auto Direction = (Pose[Foot].GetRotation() * Reference[Foot].GetRotation().Inverse()).RotateVector(FVector::ForwardVector);
						TestTrue(TEXT("Toe stays forward without inward yaw"), FMath::Abs(Direction.Y) < .001 && Direction.X > .9);
					}
					else
					{
						const int32 Arm = Ref.FindBoneIndex(*FString::Printf(TEXT("arm_%s"), Side));
						const int32 Spine = Ref.FindBoneIndex(TEXT("spine"));
						const auto Chest = Pose[Spine].GetRotation() * Reference[Spine].GetRotation().Inverse();
						const auto Swing = Chest.Inverse() * Pose[Arm].GetRotation() * Reference[Arm].GetRotation().Inverse();
						MaxArmAngle = FMath::Max(MaxArmAngle, Swing.AngularDistance(FQuat::Identity));
					}
				}
			}
			if (!bArmy)
			{
				AddInfo(FString::Printf(TEXT("%s arm swing peak %.1f degrees"), *Clip->GetName(), FMath::RadiansToDegrees(MaxArmAngle)));
				TestTrue(TEXT("Arm swing has visible amplitude"), MaxArmAngle > FMath::DegreesToRadians(20.0));
			}
		}
	}
	return true;
}
#endif
