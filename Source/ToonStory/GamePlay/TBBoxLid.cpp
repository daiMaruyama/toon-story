// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBBoxLid.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"

ATBBoxLid::ATBBoxLid()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeRotation(FRotator(-90, 0, 0));
	Light->SetIntensity(0.f);
	Light->SetCastShadows(false);
	// 天井全体から光が落ちてくるように、部屋全体へ広げる。
	Light->SetInnerConeAngle(0.f);
	Light->SetOuterConeAngle(80.f);
}

void ATBBoxLid::BeginPlay()
{
	Super::BeginPlay();
	if (Ceiling)
	{
		if (auto* Mesh = Ceiling->FindComponentByClass<UStaticMeshComponent>())
		{
			CeilingMaterial = Mesh->CreateDynamicMaterialInstance(0);
		}
		CeilingRest = Ceiling->GetActorTransform();
		FVector Origin, Extent;
		Ceiling->GetActorBounds(false, Origin, Extent);
		// 側面をPivotにして、反対側を持ち上げる。
		Hinge = Origin + FVector(0.f, Extent.Y, Extent.Z);
		const FVector Arm = CeilingRest.GetLocation() - Hinge;
		OpenSign = FQuat(FVector::ForwardVector, FMath::DegreesToRadians(1.f)).RotateVector(Arm).Z > Arm.Z ? 1.f : -1.f;
	}
	if (RoomVolume)
	{
		RestExposure = RoomVolume->Settings.AutoExposureBias;
	}
	SetOpen(0.f);
	SetFlash(0.f);
}

void ATBBoxLid::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const auto* State = TB::GS(GetWorld());
	if (!State)
	{
		return;
	}
	// 収納・救助のたびに箱の中の人数が変わるので、それを蓋が開く合図にする。
	const bool bPlaying = TB::Playing(GetWorld());
	const int32 Boxed = bPlaying ? State->BoxedCount : 0;
	if (bPlaying && LastBoxedCount >= 0 && Boxed != LastBoxedCount)
	{
		Elapsed = 0.f;
	}
	LastBoxedCount = Boxed;
	if (Elapsed < 0.f)
	{
		return;
	}
	Elapsed += DeltaSeconds;
	const float Closing = Elapsed - OpenSeconds - HoldSeconds;
	float Alpha = Elapsed < OpenSeconds ? Elapsed / OpenSeconds : Closing < 0.f ? 1.f : 1.f - Closing / CloseSeconds;
	const float Flash = Elapsed < OpenSeconds ? Elapsed / OpenSeconds : 1.f - (Elapsed - OpenSeconds) / FlashSeconds;
	SetFlash(FMath::Clamp(Flash, 0.f, 1.f));
	if (Alpha <= 0.f)
	{
		Alpha = 0.f;
		Elapsed = -1.f;
	}
	SetOpen(Alpha);
}

void ATBBoxLid::SetOpen(float Alpha)
{
	const float Eased = Alpha * Alpha * (3.f - 2.f * Alpha);
	Light->SetIntensity(PeakIntensity * Eased);
	if (CeilingMaterial)
	{
		CeilingMaterial->SetScalarParameterValue(TEXT("Glow"), Eased);
	}
	if (Ceiling)
	{
		const FQuat Tilt(FVector::ForwardVector, FMath::DegreesToRadians(OpenAngle * Eased) * OpenSign);
		Ceiling->SetActorLocationAndRotation(Hinge + Tilt.RotateVector(CeilingRest.GetLocation() - Hinge),
		                                     Tilt * CeilingRest.GetRotation());
	}
}

void ATBBoxLid::SetFlash(float Alpha)
{
	if (RoomVolume)
	{
		RoomVolume->Settings.AutoExposureBias = RestExposure + FlashExposure * Alpha;
	}
}
