#include "TBToyAssetCommandlet.h"

#if WITH_EDITOR
#include "TBToyGaitAuthoring.h"
#include "Data/TBToyDefinition.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Engine/World.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "MaterialShared.h"
#include "RenderingThread.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/BlendSpace.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Character/TBToyCharacter.h"
#include "Character/TBCharacterAppearanceComponent.h"
#include "Character/TBToyAnimationComponent.h"
#include "Components/CapsuleComponent.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

static bool SaveToyAsset(UObject* Asset)
{
	Asset->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Asset->GetOutermost(), Asset,
		*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}

template<typename T>
static T* ToyMaterialNode(UMaterial* Material, const TCHAR* Name)
{
	if (auto* Existing = FindObject<T>(Material, Name)) return Existing;
	auto* Node = NewObject<T>(Material, Name);
	Material->GetExpressionCollection().AddExpression(Node);
	return Node;
}

static bool RepairBear(UTBToyDefinition* Definition)
{
	for (const auto& Slot : Definition->SkeletalMesh->GetMaterials())
	{
		auto* Surface = Slot.MaterialInterface->GetMaterial();
		Surface->TwoSided = true;
		Surface->PostEditChange();
		if (!SaveToyAsset(Surface)) return false;
	}
	auto* Material = Definition->SkeletalMesh->GetMaterials()[0].MaterialInterface->GetMaterial();
	Material->Modify();
	Material->SetShadingModel(MSM_Cloth);
	auto* Data = Material->GetEditorOnlyData();
	Data->Roughness.Expression = nullptr; Data->Roughness.UseConstant = true; Data->Roughness.Constant = .95f;
	Data->Specular.Expression = nullptr; Data->Specular.UseConstant = true; Data->Specular.Constant = .18f;
	Data->ClearCoat.Expression = nullptr; Data->ClearCoat.UseConstant = true; Data->ClearCoat.Constant = .65f;
	Data->SubsurfaceColor.Expression = nullptr; Data->SubsurfaceColor.UseConstant = true;
	Data->SubsurfaceColor.Constant = FColor(115, 77, 45);
	// Keep only a fine, shallow fibre pattern; the original was oversized cable knit.
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (auto* UV = Cast<UMaterialExpressionTextureCoordinate>(Expression)) UV->UTiling = UV->VTiling = 28.f;
		if (auto* Strength = Cast<UMaterialExpressionConstant3Vector>(Expression))
			if (FMath::IsNearlyEqual(Strength->Constant.B, 1.f) && FMath::IsNearlyEqual(Strength->Constant.R, Strength->Constant.G))
				Strength->Constant = FLinearColor(.06f, .06f, 1.f);
		if (auto* Normal = Cast<UMaterialExpressionNormalize>(Expression)) Data->Normal.Connect(0, Normal);
	}
	Data->Normal.UseConstant = false;
	// Subtle fibre colour variation prevents the cloth from reading as smooth moulded plastic.
	auto* Fibre = ToyMaterialNode<UMaterialExpressionNoise>(Material, TEXT("ToyFibreNoise"));
	auto* FibreUV = ToyMaterialNode<UMaterialExpressionTextureCoordinate>(Material, TEXT("ToyFibreCoordinates"));
	FibreUV->UTiling = FibreUV->VTiling = 1.f;
	auto* UVPosition = ToyMaterialNode<UMaterialExpressionAppendVector>(Material, TEXT("ToyFibreUVPosition"));
	auto* Zero = ToyMaterialNode<UMaterialExpressionConstant>(Material, TEXT("ToyFibreZero"));
	Zero->R = 0.f;
	UVPosition->A.Connect(0, FibreUV); UVPosition->B.Connect(0, Zero);
	Fibre->Position.Connect(0, UVPosition);
	Fibre->Scale = 512.f;
	Fibre->Levels = 1;
	Fibre->OutputMin = 0.f; Fibre->OutputMax = 1.f;
	auto* Dark = ToyMaterialNode<UMaterialExpressionConstant3Vector>(Material, TEXT("ToyFibreDark"));
	Dark->Constant = FLinearColor(.105f, .05f, .022f);
	auto* Light = ToyMaterialNode<UMaterialExpressionConstant3Vector>(Material, TEXT("ToyFibreLight"));
	Light->Constant = FLinearColor(.18f, .09f, .041f);
	auto* FabricColour = ToyMaterialNode<UMaterialExpressionLinearInterpolate>(Material, TEXT("ToyFabricColour"));
	FabricColour->A.Connect(0, Dark); FabricColour->B.Connect(0, Light); FabricColour->Alpha.Connect(0, Fibre);
	Data->BaseColor.Connect(0, FabricColour);
	Material->PostEditChange();
	if (!SaveToyAsset(Material)) return false;
	auto* Nose = Definition->SkeletalMesh->GetMaterials()[2].MaterialInterface->GetMaterial();
	Nose->Modify();
	Nose->GetEditorOnlyData()->Normal.Expression = nullptr;
	Nose->GetEditorOnlyData()->Normal.UseConstant = true;
	Nose->GetEditorOnlyData()->Normal.Constant = FVector3f(0, 0, 1);
	Nose->GetEditorOnlyData()->Roughness.Expression = nullptr;
	Nose->GetEditorOnlyData()->Roughness.UseConstant = true;
	Nose->GetEditorOnlyData()->Roughness.Constant = .72f;
	Nose->PostEditChange();
	if (!SaveToyAsset(Nose)) return false;

	const FString PackageName = TEXT("/Game/Characters/ToyCollectionRealistic/TeddyBear/AN_Idle_Seated");
	auto* Clip = LoadObject<UAnimSequence>(nullptr, *(PackageName + TEXT(".AN_Idle_Seated")));
	if (!Clip) Clip = DuplicateObject<UAnimSequence>(Definition->Idle, CreatePackage(*PackageName), TEXT("AN_Idle_Seated"));
	Clip->SetFlags(RF_Public | RF_Standalone);
	const auto& Ref = Definition->SkeletalMesh->GetRefSkeleton();
	TArray<FTransform> Reference;
	for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
	{
		FTransform T = Ref.GetRefBonePose()[Bone];
		if (Ref.GetParentIndex(Bone) >= 0) T *= Reference[Ref.GetParentIndex(Bone)];
		Reference.Add(T);
	}
	TArray<TArray<FTransform>> Frames;
	for (int32 Frame = 0; Frame <= 60; ++Frame)
	{
		TArray<FTransform> Pose;
		const float Breath = FMath::Sin(2.f * PI * Frame / 60.f);
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			const int32 Parent = Ref.GetParentIndex(Bone);
			FTransform T = Ref.GetRefBonePose()[Bone];
			if (Parent >= 0) T *= Pose[Parent];
			const FString BoneName = Ref.GetBoneName(Bone).ToString();
			if (BoneName == TEXT("pelvis")) T.AddToTranslation(FVector(0, 0, -2.1));
			if (BoneName.StartsWith(TEXT("thigh")))
				T.SetRotation(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(-78.f)) * T.GetRotation());
			if (BoneName == TEXT("spine"))
				T.SetRotation(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Breath * .6f)) * T.GetRotation());
			Pose.Add(T);
		}
		float MinZ = FLT_MAX;
		for (const auto& Section : Definition->SkeletalMesh->GetImportedModel()->LODModels[0].Sections)
			for (const auto& Vertex : Section.SoftVertices)
			{
				FVector Position = FVector::ZeroVector;
				for (int32 Influence = 0; Influence < MAX_TOTAL_INFLUENCES; ++Influence)
				{
					if (!Vertex.InfluenceWeights[Influence]) continue;
					const int32 Bone = Section.BoneMap[Vertex.InfluenceBones[Influence]];
					Position += Pose[Bone].TransformPosition(Reference[Bone].InverseTransformPosition(FVector(Vertex.Position)))
						* (Vertex.InfluenceWeights[Influence] / 65535.f);
				}
				MinZ = FMath::Min(MinZ, static_cast<float>(Position.Z));
			}
		for (auto& T : Pose) T.AddToTranslation(FVector(0, 0, -MinZ));
		TArray<FTransform> Local;
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
			Local.Add(Ref.GetParentIndex(Bone) < 0 ? Pose[Bone] : Pose[Bone].GetRelativeTransform(Pose[Ref.GetParentIndex(Bone)]));
		Frames.Add(MoveTemp(Local));
	}
	auto& Controller = Clip->GetController();
	Controller.OpenBracket(FText::FromString(TEXT("Author seated plush idle")), false);
	Controller.SetFrameRate(FFrameRate(30, 1), false);
	Controller.SetNumberOfFrames(FFrameNumber(60), false);
	bool bTracksValid = true;
	for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
	{
		const FName Name = Ref.GetBoneName(Bone);
		if (!Clip->GetDataModel()->IsValidBoneTrackName(Name)) Controller.AddBoneCurve(Name, false);
		TArray<FVector3f> Positions, Scales; TArray<FQuat4f> Rotations;
		for (const auto& Frame : Frames)
		{
			Positions.Add(FVector3f(Frame[Bone].GetLocation()));
			Rotations.Add(FQuat4f(Frame[Bone].GetRotation()));
			Scales.Add(FVector3f(Frame[Bone].GetScale3D()));
		}
		bTracksValid &= Controller.SetBoneTrackKeys(Name, Positions, Rotations, Scales, false);
	}
	Controller.CloseBracket(false);
	if (!bTracksValid) return false;
	Clip->PostEditChange();
	if (!SaveToyAsset(Clip)) return false;
	Definition->Idle = Clip;
	Definition->Locomotion->ReplaceSampleAnimation(0, Clip);
	Definition->Locomotion->TargetWeightInterpolationSpeedPerSec = 3.f;
	Definition->Locomotion->ResampleData();
	Definition->Locomotion->PostEditChange();
	return SaveToyAsset(Definition->Locomotion) && SaveToyAsset(Definition);
}

static bool RenderToy(UTBToyDefinition* Definition, const FString& Name, const FString& Params)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	// CreateWorld initializes the persistent level.
	auto* Actor = World->SpawnActor<ATBToyCharacter>();
	Actor->GetCapsuleComponent()->SetCapsuleSize(10.f, 13.f);
	Actor->SetActorLocation(FVector(0, 0, 15.15));
	auto* Appearance = Actor->FindComponentByClass<UTBCharacterAppearanceComponent>();
	Appearance->SetDefinition(Definition);
	Appearance->Apply(*Actor, *Actor->ToyAnimation, true);
	auto* Mesh = Actor->GetMesh();
	Mesh->SetOwnerNoSee(false);
	Mesh->SetCastShadow(true);
	FString Motion = TEXT("Idle"); FParse::Value(*Params, TEXT("Motion="), Motion);
	float Phase = 0.f; FParse::Value(*Params, TEXT("Phase="), Phase);
	auto* Clip = Motion == TEXT("Walk") ? Definition->Walk.Get() : Motion == TEXT("Run") ? Definition->Run.Get() : Definition->Idle.Get();
	Mesh->PlayAnimation(Clip, true);
	Mesh->SetPosition(Clip->GetPlayLength() * Phase, false);
	Mesh->TickAnimation(0.f, false);
	Mesh->RefreshBoneTransforms();
	Mesh->bPauseAnims = true;
	Mesh->UpdateBounds();
	auto* Floor = World->SpawnActor<AStaticMeshActor>();
	Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Floor->SetActorScale3D(FVector(2, 2, .002));
	Floor->SetActorLocation(FVector(0, 0, -.1));
	for (int32 I = 0; I < 3; ++I)
	{
		auto* Light = World->SpawnActor<ADirectionalLight>();
		Light->SetActorRotation(FRotator(-40, I == 0 ? 145 : (I == 1 ? -35 : 65), 0));
		Light->GetLightComponent()->SetIntensity(I ? 1.f : 3.f);
		Light->GetLightComponent()->SetCastShadows(I == 0);
	}
	auto* Capture = World->SpawnActor<ASceneCapture2D>();
	Capture->SetActorLocation(FVector(45, -42, 26));
	if (FParse::Param(*Params, TEXT("Front"))) Capture->SetActorLocation(FVector(65, 0, 22));
	Capture->SetActorRotation((FVector(0, 0, 12) - Capture->GetActorLocation()).Rotation());
	auto* Camera = Capture->GetCaptureComponent2D();
	auto* Target = NewObject<UTextureRenderTarget2D>(Capture);
	Target->InitAutoFormat(900, 900);
	Camera->TextureTarget = Target;
	Camera->CaptureSource = SCS_FinalColorLDR;
	Camera->FOVAngle = 32.f;
	Camera->bCaptureEveryFrame = false;
	Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Camera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	Camera->PostProcessSettings.AutoExposureMinBrightness = 0.f;
	Camera->PostProcessSettings.AutoExposureMaxBrightness = 0.f;
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	for (const auto& Slot : Definition->SkeletalMesh->GetMaterials())
	{
		const auto* Resource = Slot.MaterialInterface->GetMaterial()->GetMaterialResource(GMaxRHIShaderPlatform);
		if (!Resource || !Resource->GetCompileErrors().IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("Cannot render %s: material %s has no valid shader."), *Name, *GetNameSafe(Slot.MaterialInterface));
			World->DestroyWorld(false);
			return false;
		}
	}
	Mesh->MarkRenderStateDirty();
	World->UpdateWorldComponents(true, false);
	for (int32 Frame = 0; Frame < 8; ++Frame)
	{
		++GFrameCounter;
		World->Tick(LEVELTICK_All, 1.f / 30.f);
		World->SendAllEndOfFrameUpdates();
		Camera->CaptureScene();
		FlushRenderingCommands();
	}
	TArray<FColor> Pixels;
	const bool bRead = Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
	if (!bRead || Pixels.Num() != 900 * 900) { World->DestroyWorld(false); return false; }
	TArray64<uint8> PNG;
	FImageUtils::PNGCompressImageArray(900, 900, Pixels, PNG);
	FString Shot = Name + TEXT("_UE"); FParse::Value(*Params, TEXT("Shot="), Shot);
	const bool bSaved = FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir() / TEXT("ToyCollectionRealistic") / (Shot + TEXT(".png"))));
	World->DestroyWorld(false);
	return bSaved;
}
#endif

UTBToyAssetCommandlet::UTBToyAssetCommandlet()
{
	IsClient = true;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UTBToyAssetCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString Only;
	FParse::Value(*Params, TEXT("Only="), Only);
	for (const TCHAR* Name : {TEXT("SpaceRanger_Realistic"), TEXT("TeddyBear_Realistic"), TEXT("Duck_Realistic"),
	                         TEXT("TinRobot_Realistic"), TEXT("Dinosaur_Realistic"), TEXT("ArmyMan_StarterPack"), TEXT("Nutcracker")})
	{
		if (!Only.IsEmpty() && !FString(Name).StartsWith(Only)) continue;
		if (FParse::Param(*Params, TEXT("ReviewGait")) && !FString(Name).StartsWith(TEXT("Army")) && !FString(Name).StartsWith(TEXT("Space")) && !FString(Name).StartsWith(TEXT("Duck"))) continue;
		auto* Definition = LoadObject<UTBToyDefinition>(nullptr, *FString::Printf(TEXT("/Game/Characters/ToyDefinitions/DA_%s.DA_%s"), Name, Name));
		if (!Definition || !Definition->SkeletalMesh) return 1;
		auto* Mesh = Definition->SkeletalMesh.Get();
		if (FParse::Param(*Params, TEXT("Tune")) && !TBToyGaitAuthoring::Tune(Definition, Name)) return 1;
		if (FParse::Param(*Params, TEXT("Repair")) && (FString(Name).StartsWith(TEXT("Space")) || FString(Name).StartsWith(TEXT("Teddy"))))
		{
			if (FString(Name).StartsWith(TEXT("Space")))
				for (const auto& Slot : Mesh->GetMaterials())
				{
					auto* Surface = Slot.MaterialInterface->GetMaterial();
					Surface->Modify();
					Surface->TwoSided = true;
					Surface->SetMaterialUsage(MATUSAGE_SkeletalMesh);
					Surface->PostEditChange();
					if (!SaveToyAsset(Surface)) return 1;
				}
			Mesh->Modify();
			for (int32 LOD = 0; LOD < Mesh->GetLODNum(); ++LOD)
			{
				auto& Settings = Mesh->GetLODInfo(LOD)->BuildSettings;
				Settings.bRecomputeNormals = FString(Name).StartsWith(TEXT("Teddy"));
				Settings.bRecomputeTangents = Settings.bRecomputeNormals;
				Settings.bComputeWeightedNormals = Settings.bRecomputeNormals;
				Settings.bUseMikkTSpace = true;
			}
			Mesh->PostEditChange();
			FAssetCompilingManager::Get().FinishAllCompilation();
			if (!SaveToyAsset(Mesh)) return 1;
		}
		if (FParse::Param(*Params, TEXT("Repair")) && FString(Name).StartsWith(TEXT("Teddy")) && !RepairBear(Definition)) return 1;
		UE_LOG(LogTemp, Display, TEXT("OVERRIDES %d"), Definition->MaterialOverrides.Num());
		if (FParse::Param(*Params, TEXT("Render")))
			if (!RenderToy(Definition, Name, Params)) return 1;
		if (FParse::Param(*Params, TEXT("ReviewGait")))
		{
			if (FString(Name).StartsWith(TEXT("Duck")))
			{ if (!RenderToy(Definition, Name, TEXT("-Shot=duck_yellow"))) return 1; }
			else
			{
				const bool bArmy = FString(Name).StartsWith(TEXT("Army"));
				if (!RenderToy(Definition, Name, bArmy ? TEXT("-Motion=Walk -Phase=0.25 -Front -Shot=army_forward_front") : TEXT("-Motion=Walk -Phase=0.25 -Shot=space_walk_swing"))) return 1;
				if (!RenderToy(Definition, Name, bArmy ? TEXT("-Motion=Run -Phase=0.75 -Shot=army_forward_run") : TEXT("-Motion=Run -Phase=0.75 -Shot=space_run_swing"))) return 1;
			}
		}
		const auto Bounds = Mesh->GetBounds();
		UE_LOG(LogTemp, Display, TEXT("TOY %s bounds origin=%s extent=%s"), Name, *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString());
		int32 Index = 0;
		for (const auto& Slot : Mesh->GetMaterials())
		{
			UE_LOG(LogTemp, Display, TEXT("SLOT %d name=%s imported=%s material=%s"), Index++, *Slot.MaterialSlotName.ToString(),
				*Slot.ImportedMaterialSlotName.ToString(), *GetPathNameSafe(Slot.MaterialInterface));
			if (auto* Mat = Slot.MaterialInterface ? Slot.MaterialInterface->GetMaterial() : nullptr)
			{
				UE_LOG(LogTemp, Display, TEXT("BASE expr=%s"), *GetNameSafe(Mat->GetEditorOnlyData()->BaseColor.Expression));
				if (auto* Color = Cast<UMaterialExpressionConstant3Vector>(Mat->GetEditorOnlyData()->BaseColor.Expression))
					UE_LOG(LogTemp, Display, TEXT("COLOR %s"), *Color->Constant.ToString());
				for (UMaterialExpression* Expr : Mat->GetExpressions())
					if (auto* Sample = Cast<UMaterialExpressionTextureSample>(Expr))
					{
						UE_LOG(LogTemp, Display, TEXT("TEXTURE %s uv=%d"), *GetPathNameSafe(Sample->Texture), Sample->ConstCoordinate);
						if (auto* Texture = FParse::Param(*Params, TEXT("ExportTextures")) ? Cast<UTexture2D>(Sample->Texture) : nullptr)
						{
							FImage Image;
							if (Texture->Source.GetMipImage(Image, 0)) FImageUtils::SaveImageByExtension(
								*(FPaths::ProjectSavedDir() / TEXT("ToyCollectionRealistic") / (FString(Name) + TEXT("_") + Texture->GetName() + TEXT("_source.png"))), Image);
						}
					}
			}
		}
		const auto& Ref = Mesh->GetRefSkeleton();
		TArray<FTransform> ComponentPose;
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			FTransform Transform = Ref.GetRefBonePose()[Bone];
			const int32 Parent = Ref.GetParentIndex(Bone);
			if (Parent >= 0) Transform *= ComponentPose[Parent];
			ComponentPose.Add(Transform);
			UE_LOG(LogTemp, Display, TEXT("BONE %s position=%s rotation=%s"), *Ref.GetBoneName(Bone).ToString(), *Transform.GetLocation().ToString(), *Transform.Rotator().ToString());
		}
		const auto* Model = Mesh->GetImportedModel();
		if (Model && Model->LODModels.Num())
		{
			const auto& LOD = Model->LODModels[0];
			for (const auto& Section : LOD.Sections)
			{
				FBox VertexBounds(ForceInit);
				FVector2f MinUV(FLT_MAX), MaxUV(-FLT_MAX);
				for (const auto& Vertex : Section.SoftVertices)
				{
					VertexBounds += FVector(Vertex.Position);
					MinUV.X = FMath::Min(MinUV.X, Vertex.UVs[0].X); MinUV.Y = FMath::Min(MinUV.Y, Vertex.UVs[0].Y);
					MaxUV.X = FMath::Max(MaxUV.X, Vertex.UVs[0].X); MaxUV.Y = FMath::Max(MaxUV.Y, Vertex.UVs[0].Y);
				}
				UE_LOG(LogTemp, Display, TEXT("SECTION material=%d vertices=%d bounds=%s UV=%s..%s channels=%d"),
					Section.MaterialIndex, Section.SoftVertices.Num(), *VertexBounds.ToString(), *MinUV.ToString(), *MaxUV.ToString(), LOD.NumTexCoords);
			}
		}
	}
	return 0;
#else
	return 1;
#endif
}
