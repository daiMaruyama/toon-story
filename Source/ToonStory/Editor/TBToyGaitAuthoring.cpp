#include "TBToyGaitAuthoring.h"

#if WITH_EDITOR
#include "Data/TBToyDefinition.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/BlendSpace.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
bool Save(UObject* Asset)
{
	Asset->MarkPackageDirty();
	FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Asset->GetOutermost(), Asset,
		*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}

TArray<FTransform> ComponentPose(const FReferenceSkeleton& Ref, const IAnimationDataModel* Model = nullptr, int32 Frame = 0)
{
	TArray<FTransform> Result;
	for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
	{
		FTransform T = Model && Model->IsValidBoneTrackName(Ref.GetBoneName(Bone))
			? Model->GetBoneTrackTransform(Ref.GetBoneName(Bone), FFrameNumber(Frame)) : Ref.GetRefBonePose()[Bone];
		if (Ref.GetParentIndex(Bone) >= 0) T *= Result[Ref.GetParentIndex(Bone)];
		Result.Add(T);
	}
	return Result;
}

UAnimSequence* MakeClip(UTBToyDefinition* Definition, const TCHAR* Motion, bool bStraightLegs)
{
	const FString Folder = FPackageName::GetLongPackagePath(Definition->Walk->GetOutermost()->GetName());
	const FString SourceName = FString(TEXT("AN_")) + Motion;
	auto* Source = LoadObject<UAnimSequence>(nullptr, *(Folder / SourceName + TEXT(".") + SourceName));
	if (!Source) return nullptr;
	const FString OutputName = SourceName + (bStraightLegs ? TEXT("_Forward") : TEXT("_ArmSwing"));
	const FString PackageName = Folder / OutputName;
	auto* Output = FindObject<UAnimSequence>(nullptr, *(PackageName + TEXT(".") + OutputName));
	if (!Output && FPackageName::DoesPackageExist(PackageName)) Output = LoadObject<UAnimSequence>(nullptr, *(PackageName + TEXT(".") + OutputName));
	if (!Output) Output = DuplicateObject<UAnimSequence>(Source, CreatePackage(*PackageName), *OutputName);
	Output->SetFlags(RF_Public | RF_Standalone);
	const auto& Ref = Definition->SkeletalMesh->GetRefSkeleton();
	const auto Reference = ComponentPose(Ref);
	const auto* Model = Source->GetDataModel();
	TArray<TArray<FTransform>> Poses;
	for (int32 Frame = 0; Frame <= Model->GetNumberOfFrames(); ++Frame) Poses.Add(ComponentPose(Ref, Model, Frame));
	const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
	const int32 Spine = Ref.FindBoneIndex(TEXT("spine"));
	if (Pelvis == INDEX_NONE || Spine == INDEX_NONE) return nullptr;
	for (const TCHAR* Side : {TEXT("L"), TEXT("R")})
	{
		const int32 Thigh = Ref.FindBoneIndex(*FString::Printf(TEXT("thigh_%s"), Side));
		const int32 Calf = Ref.FindBoneIndex(*FString::Printf(TEXT("calf_%s"), Side));
		const int32 Foot = Ref.FindBoneIndex(*FString::Printf(TEXT("foot_%s"), Side));
		if (Thigh == INDEX_NONE || Calf == INDEX_NONE || Foot == INDEX_NONE) return nullptr;
		if (bStraightLegs)
		{
			for (auto& Pose : Poses)
			{
				// Keep the source forward stride/lift, but place hip, knee and ankle in one sagittal plane.
				const double Lane = Pose[Pelvis].GetLocation().Y + FMath::Sign(Reference[Thigh].GetLocation().Y) * .85;
				for (int32 Bone : {Thigh, Calf, Foot})
				{
					FVector P = Pose[Bone].GetLocation(); P.Y = Lane; Pose[Bone].SetLocation(P);
				}
				for (const auto Pair : {TPair<int32, int32>(Thigh, Calf), TPair<int32, int32>(Calf, Foot)})
					Pose[Pair.Key].SetRotation(FQuat::FindBetweenVectors(
						Reference[Pair.Value].GetLocation() - Reference[Pair.Key].GetLocation(),
						Pose[Pair.Value].GetLocation() - Pose[Pair.Key].GetLocation()) * Reference[Pair.Key].GetRotation());
				const FVector Forward = (Pose[Foot].GetRotation() * Reference[Foot].GetRotation().Inverse()).RotateVector(FVector::ForwardVector);
				Pose[Foot].SetRotation(FQuat(FVector::YAxisVector, FMath::Atan2(-Forward.Z, Forward.X)) * Reference[Foot].GetRotation());
			}
		}
		else
		{
			// An arm advances with the opposite leg. Derive phase from the source mocap, not a separate timer.
			const int32 Arm = Ref.FindBoneIndex(*FString::Printf(TEXT("arm_%s"), FString(Side) == TEXT("L") ? TEXT("R") : TEXT("L")));
			if (Arm == INDEX_NONE) return nullptr;
			TArray<double> Angles; double Sum = 0;
			for (const auto& Pose : Poses)
			{
				const FVector Direction = Pose[Calf].GetLocation() - Pose[Thigh].GetLocation();
				const double Angle = FMath::Atan2(Direction.X, -Direction.Z);
				Angles.Add(Angle); Sum += Angle;
			}
			const double Mean = Sum / Angles.Num(); double Peak = .001;
			for (double Angle : Angles) Peak = FMath::Max(Peak, FMath::Abs(Angle - Mean));
			const double Amplitude = FMath::DegreesToRadians(FString(Motion) == TEXT("Run") ? 38.0 : 24.0);
			for (int32 Frame = 0; Frame < Poses.Num(); ++Frame)
			{
				auto& Pose = Poses[Frame];
				const FQuat Chest = Pose[Spine].GetRotation() * Reference[Spine].GetRotation().Inverse();
				const double Swing = -(Angles[Frame] - Mean) / Peak * Amplitude;
				Pose[Arm].SetRotation(Chest * FQuat(FVector::YAxisVector, Swing) * Reference[Arm].GetRotation());
			}
		}
	}
	Poses.Last() = Poses[0];
	auto& Controller = Output->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Tune toy gait")), false);
	Controller.SetFrameRate(Model->GetFrameRate(), false);
	Controller.SetNumberOfFrames(FFrameNumber(Model->GetNumberOfFrames()), false);
	bool bValid = true;
	for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
	{
		const FName Name = Ref.GetBoneName(Bone);
		if (!Output->GetDataModel()->IsValidBoneTrackName(Name)) Controller.AddBoneCurve(Name, false);
		TArray<FVector3f> Positions, Scales; TArray<FQuat4f> Rotations;
		for (const auto& Pose : Poses)
		{
			const int32 Parent = Ref.GetParentIndex(Bone);
			const FTransform T = Parent < 0 ? Pose[Bone] : Pose[Bone].GetRelativeTransform(Pose[Parent]);
			Positions.Add(FVector3f(T.GetLocation())); Scales.Add(FVector3f(T.GetScale3D())); Rotations.Add(FQuat4f(T.GetRotation()));
		}
		bValid &= Controller.SetBoneTrackKeys(Name, Positions, Rotations, Scales, false);
	}
	Controller.CloseBracket(false);
	Output->PostEditChange();
	return bValid && Save(Output) ? Output : nullptr;
}

bool YellowDuck(UTBToyDefinition* Definition)
{
	auto* Material = Definition->SkeletalMesh->GetMaterials()[0].MaterialInterface->GetMaterial();
	UMaterialExpressionTextureSample* Texture = nullptr;
	for (UMaterialExpression* Expression : Material->GetExpressions())
		if (auto* Sample = Cast<UMaterialExpressionTextureSample>(Expression))
			if (Sample->Texture && Sample->Texture->GetName().Contains(TEXT("Texture_0"))) Texture = Sample;
	if (!Texture) return false;
	Material->Modify();
	auto* Colour = FindObject<UMaterialExpressionCustom>(Material, TEXT("ToyDuckYellow"));
	if (!Colour)
	{
		Colour = NewObject<UMaterialExpressionCustom>(Material, TEXT("ToyDuckYellow"));
		Material->GetExpressionCollection().AddExpression(Colour);
	}
	Colour->Description = TEXT("Yellow rubber body; preserve orange beak, eyes and painted detail");
	Colour->OutputType = CMOT_Float3;
	Colour->Inputs.SetNum(1); Colour->Inputs[0].InputName = TEXT("Colour"); Colour->Inputs[0].Input.Connect(0, Texture);
	Colour->Code = TEXT("float mask = smoothstep(0.12, 0.30, Colour.g / max(Colour.r, 0.001)) * saturate((Colour.r - Colour.b) * 10); return lerp(Colour, float3(Colour.r, max(Colour.g, Colour.r * 0.86), Colour.b), mask);");
	Material->GetEditorOnlyData()->BaseColor.Connect(0, Colour);
	Material->PostEditChange();
	return Save(Material);
}
}

bool TBToyGaitAuthoring::Tune(UTBToyDefinition* Definition, const FString& CollectionName)
{
	if (CollectionName.StartsWith(TEXT("Duck"))) return YellowDuck(Definition);
	const bool bArmy = CollectionName.StartsWith(TEXT("Army"));
	if (!bArmy && !CollectionName.StartsWith(TEXT("Space"))) return true;
	UAnimSequence* Old[] = {Definition->Idle, Definition->Walk, Definition->Run};
	UAnimSequence* Clips[] = {bArmy ? MakeClip(Definition, TEXT("Idle"), true) : Old[0],
		MakeClip(Definition, TEXT("Walk"), bArmy), MakeClip(Definition, TEXT("Run"), bArmy)};
	for (auto* Clip : Clips) if (!Clip) return false;
	for (int32 Sample = 0; Sample < Definition->Locomotion->GetNumberOfBlendSamples(); ++Sample)
		for (int32 Motion = 0; Motion < 3; ++Motion)
			if (Definition->Locomotion->GetBlendSample(Sample).Animation == Old[Motion])
			{ Definition->Locomotion->ReplaceSampleAnimation(Sample, Clips[Motion]); break; }
	for (int32 Motion = 0; Motion < 3; ++Motion) if (Definition->Jump == Old[Motion]) Definition->Jump = Clips[Motion];
	Definition->Idle = Clips[0]; Definition->Walk = Clips[1]; Definition->Run = Clips[2];
	Definition->Locomotion->ResampleData(); Definition->Locomotion->PostEditChange();
	return Save(Definition->Locomotion) && Save(Definition);
}
#endif
