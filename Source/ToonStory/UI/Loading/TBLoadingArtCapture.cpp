// Copyright (c) 2026 Mamodou. All Rights Reserved.

// ロード画面に出すおもちゃの絵を撮る、開発用のコマンド。ゲームには入らない（Shipping では無効）。
// 使い方: -game で立ち上げ、コンソールで TBCaptureLoadingArt。Saved/LoadingArt/ に背景が透明な PNG が出る。
#if !UE_BUILD_SHIPPING
#include "CoreMinimal.h"
#include "Components/ChildActorComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Containers/Ticker.h"
#include "Engine/PointLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "TextureResource.h"

namespace
{
	const TCHAR* const ArtToyClasses[] = {
	    TEXT("/Game/Characters/Toys/BP_TeddyBear_Realistic.BP_TeddyBear_Realistic_C"),
	    TEXT("/Game/Characters/Toys/BP_Dinosaur_Realistic.BP_Dinosaur_Realistic_C"),
	    TEXT("/Game/Characters/Toys/BP_TinRobot_Realistic.BP_TinRobot_Realistic_C"),
	    TEXT("/Game/Characters/Toys/BP_Duck_Realistic.BP_Duck_Realistic_C"),
	    TEXT("/Game/Characters/Toys/BP_Nutcracker.BP_Nutcracker_C"),
	};
	constexpr int32 ArtSize = 512;
	// 家から離れた空の上で撮る。ほかの物は写さない。
	const FVector ArtStage(0, 0, 60000);

	// 1体ずつ出して、アニメが落ち着くのを待ってから撮る。
	struct FArtCapture
	{
		TWeakObjectPtr<UWorld> World;
		int32 Index = 0;
		double ShootAt = 0;
		TWeakObjectPtr<AActor> Toy;
		TArray<TWeakObjectPtr<AActor>> Props;
		TObjectPtr<UTextureRenderTarget2D> Target;
	};
	TSharedPtr<FArtCapture> ArtRun;

	void ClearArt(FArtCapture& Capture)
	{
		if (Capture.Toy.IsValid()) Capture.Toy->Destroy();
		for (auto& Prop : Capture.Props)
			if (Prop.IsValid()) Prop->Destroy();
		Capture.Props.Reset();
	}

	// 横から少し前に回った位置。おもちゃは画面の右を向く。
	void ShootArt(FArtCapture& Capture, UWorld& World)
	{
		AActor* Toy = Capture.Toy.Get();
		FVector Origin, Extent;
		Toy->GetActorBounds(false, Origin, Extent, true);
		const float Radius = FMath::Max(Extent.Size(), 10.f);
		const float Fov = 30.f;
		const FRotator View(-8.f, -110.f, 0.f);
		const FVector Camera = Origin - View.Vector() * Radius / FMath::Sin(FMath::DegreesToRadians(Fov * .5f)) * 1.05f;
		auto* Shot = World.SpawnActor<ASceneCapture2D>(Camera, View);
		auto* Lens = Shot->GetCaptureComponent2D();
		Lens->FOVAngle = Fov;
		Lens->CaptureSource = SCS_SceneColorHDR;
		Lens->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		// 見た目が子アクターに分かれていることがあるので、ぶら下がっているアクターもまとめて写す。
		TArray<AActor*> Parts{Toy};
		for (int32 I = 0; I < Parts.Num(); ++I)
		{
			Parts[I]->GetAttachedActors(Parts, false, true);
			TArray<UChildActorComponent*> Children;
			Parts[I]->GetComponents(Children);
			for (auto* Child : Children)
				if (Child->GetChildActor()) Parts.AddUnique(Child->GetChildActor());
		}
		for (AActor* Part : Parts) Lens->ShowOnlyActors.Add(Part);
		Lens->bCaptureEveryFrame = false;
		Lens->bCaptureOnMovement = false;
		Lens->TextureTarget = Capture.Target;
		Lens->CaptureScene();
		Capture.Props.Add(Shot);

		TArray<FFloat16Color> Raw;
		Capture.Target->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Raw);
		TArray<float> Lum;
		for (const auto& P : Raw)
			if (1.f - P.A.GetFloat() > .5f)
				Lum.Add(.2126f * P.R.GetFloat() + .7152f * P.G.GetFloat() + .0722f * P.B.GetFloat());
		Lum.Sort();
		// 写っている部分の真ん中の明るさをそろえる。明るい側に合わせると、白っぽいおもちゃ（テディ）が色あせる。
		const float Middle = Lum.Num() ? FMath::Max(Lum[Lum.Num() / 2], 1e-4f) : 1.f;
		const float Exposure = .7f / Middle;
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(Raw.Num());
		for (int32 I = 0; I < Raw.Num(); ++I)
		{
			const FFloat16Color& P = Raw[I];
			const float Alpha = FMath::Clamp(1.f - P.A.GetFloat(), 0.f, 1.f);
			auto Tone = [Exposure, Alpha](float V)
			{
				// 透明な縁の色を薄めずに戻してから、やわらかく白に寄せる。
				const float X = V * Exposure / FMath::Max(Alpha, 1e-3f);
				return X / (1.f + X);
			};
			FLinearColor Color(Tone(P.R.GetFloat()), Tone(P.G.GetFloat()), Tone(P.B.GetFloat()), Alpha);
			Pixels[I] = Color.ToFColor(true);
		}
		// 写っている部分のぎりぎりで切り抜く。ロード画面では絵の下端を床にそろえるので、足元に余白を残さない。
		int32 MinX = ArtSize, MinY = ArtSize, MaxX = -1, MaxY = -1;
		for (int32 Y = 0; Y < ArtSize; ++Y)
			for (int32 X = 0; X < ArtSize; ++X)
				if (Pixels[Y * ArtSize + X].A > 4)
				{
					MinX = FMath::Min(MinX, X), MaxX = FMath::Max(MaxX, X);
					MinY = FMath::Min(MinY, Y), MaxY = FMath::Max(MaxY, Y);
				}
		if (MaxX < 0) MinX = MinY = 0, MaxX = MaxY = ArtSize - 1;
		MinX = FMath::Max(MinX - 2, 0), MinY = FMath::Max(MinY - 2, 0);
		MaxX = FMath::Min(MaxX + 2, ArtSize - 1), MaxY = FMath::Min(MaxY + 1, ArtSize - 1);
		// 切り抜いた絵を、正方形の下の真ん中に置く。縦横の違う絵は取り込みで比率が崩れることがあるので、正方形にそろえる。
		const int32 CropW = MaxX - MinX + 1, CropH = MaxY - MinY + 1;
		const int32 Square = FMath::Max(CropW, CropH), Left = (Square - CropW) / 2, Top = Square - CropH;
		TArray<FColor> Cropped;
		Cropped.Init(FColor(0, 0, 0, 0), Square * Square);
		for (int32 Y = 0; Y < CropH; ++Y)
			FMemory::Memcpy(&Cropped[(Top + Y) * Square + Left], &Pixels[(MinY + Y) * ArtSize + MinX], CropW * sizeof(FColor));
		const FString Name = FPaths::GetBaseFilename(FString(ArtToyClasses[Capture.Index])).Replace(TEXT("BP_"), TEXT(""));
		const FString File = FPaths::ProjectSavedDir() / TEXT("LoadingArt") / (TEXT("T_LoadToy_") + Name + TEXT(".png"));
		const bool bSaved = FImageUtils::SaveImageByExtension(*File, FImageView(Cropped.GetData(), Square, Square));
		UE_LOG(LogTemp, Display, TEXT("TBCaptureLoadingArt: %s -> %s"), *Name, bSaved ? *File : TEXT("保存できませんでした"));
	}

	bool SpawnArt(FArtCapture& Capture, UWorld& World)
	{
		UClass* Class = LoadClass<AActor>(nullptr, ArtToyClasses[Capture.Index]);
		if (!Class)
		{
			UE_LOG(LogTemp, Warning, TEXT("TBCaptureLoadingArt: 読めませんでした %s"), ArtToyClasses[Capture.Index]);
			return false;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Capture.Toy = World.SpawnActor<AActor>(Class, ArtStage, FRotator::ZeroRotator, Params);
		// 出せなかったら、ライトを出す前にやめる。残ったライトで、あとの撮影が明るくならないように。
		if (!Capture.Toy.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("TBCaptureLoadingArt: 出せませんでした %s"), ArtToyClasses[Capture.Index]);
			return false;
		}
		// 正面上からの暖かいキーライトと、反対側からの弱い補助光。家の部屋の電球の色に寄せる。
		struct FArtLight
		{
			FVector Offset;
			float Intensity;
			FColor Color;
		};
		for (const FArtLight& Spec : {FArtLight{FVector(200, 150, 200), 60000.f, FColor(255, 214, 170)},
		                              FArtLight{FVector(-150, 200, 60), 15000.f, FColor(225, 230, 255)}})
		{
			auto* Light = World.SpawnActor<APointLight>(ArtStage + Spec.Offset, FRotator::ZeroRotator);
			Light->PointLightComponent->SetIntensity(Spec.Intensity);
			Light->PointLightComponent->SetLightColor(Spec.Color);
			Light->PointLightComponent->SetAttenuationRadius(2000.f);
			Light->PointLightComponent->SetCastShadows(false);
			Capture.Props.Add(Light);
		}
		return true;
	}

	bool TickArt(float)
	{
		FArtCapture& Capture = *ArtRun;
		UWorld* World = Capture.World.Get();
		if (!World)
		{
			Capture.Target->RemoveFromRoot();
			ArtRun.Reset();
			return false;
		}
		if (!Capture.Toy.IsValid())
		{
			// 撮る前に消えたおもちゃの分のライトを片付けてから、出し直す。
			ClearArt(Capture);
			while (Capture.Index < UE_ARRAY_COUNT(ArtToyClasses) && !SpawnArt(Capture, *World)) ++Capture.Index;
			if (Capture.Index >= UE_ARRAY_COUNT(ArtToyClasses))
			{
				UE_LOG(LogTemp, Display, TEXT("TBCaptureLoadingArt: 終わりました"));
				Capture.Target->RemoveFromRoot();
				ArtRun.Reset();
				return false;
			}
			Capture.ShootAt = FPlatformTime::Seconds() + 1.0;
			return true;
		}
		if (FPlatformTime::Seconds() < Capture.ShootAt) return true;
		ShootArt(Capture, *World);
		ClearArt(Capture);
		++Capture.Index;
		return true;
	}

	FAutoConsoleCommandWithWorld CaptureCommand(
	    TEXT("TBCaptureLoadingArt"), TEXT("ロード画面用に、おもちゃを1体ずつ背景透明の PNG で Saved/LoadingArt に撮る"),
	    FConsoleCommandWithWorldDelegate::CreateLambda(
	        [](UWorld* World)
	        {
		        if (!World || ArtRun) return;
		        ArtRun = MakeShared<FArtCapture>();
		        ArtRun->World = World;
		        ArtRun->Target = NewObject<UTextureRenderTarget2D>();
		        ArtRun->Target->AddToRoot();
		        ArtRun->Target->ClearColor = FLinearColor::Transparent;
		        ArtRun->Target->InitCustomFormat(ArtSize, ArtSize, PF_FloatRGBA, true);
		        ArtRun->Target->UpdateResourceImmediate(true);
		        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickArt));
	        }));
}
#endif
