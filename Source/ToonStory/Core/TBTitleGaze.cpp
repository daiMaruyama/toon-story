// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBTitleGaze.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UI/TBTitleMenu.h"

namespace
{
	const FName ToyTag(TEXT("TitleToy"));
	// 視線がここまで届く。
	constexpr float GazeLength = 5000.f;
	// 光が外れてから踊りに戻るまで。
	constexpr float ReleaseSeconds = .4f;
}

ATBTitleGaze::ATBTitleGaze()
{
	PrimaryActorTick.bCanEverTick = true;
	Light = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	RootComponent = Light;
	Light->SetMobility(EComponentMobility::Movable);
	Light->SetIntensity(6000.f);
	Light->SetLightColor(FLinearColor(1.f, .9f, .75f));
	Light->SetInnerConeAngle(5.f);
	Light->SetOuterConeAngle(9.f);
	Light->SetAttenuationRadius(1500.f);
	Light->SetVisibility(false);
}

void ATBTitleGaze::SetGazeRay(const FVector& Origin, const FVector& Direction)
{
	bHasRay = true;
	RayOrigin = Origin;
	RayDirection = Direction.GetSafeNormal();
}

void ATBTitleGaze::ClearGaze()
{
	bHasRay = false;
}

bool ATBTitleGaze::IsFrozen(const AActor* Toy) const
{
	return Frozen.ContainsByPredicate([Toy](const FTBFrozenToy& F) { return F.Toy == Toy; });
}

void ATBTitleGaze::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// メニューを出している間だけ。寸劇の最中は照らさない。
	const bool bActive = Menu.IsValid() && Menu->GetState() == UTBTitleMenu::EState::Menu;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	float X, Y;
	FVector Origin, Direction;
	if (bActive && PC && PC->GetMousePosition(X, Y) && PC->DeprojectScreenPositionToWorld(X, Y, Origin, Direction))
	{
		SetGazeRay(Origin, Direction);
	}
	else
	{
		ClearGaze();
	}
	UpdateGaze(DeltaTime);
}

void ATBTitleGaze::UpdateGaze(float DeltaTime)
{
	Light->SetVisibility(bHasRay);
	if (bHasRay) SetActorLocationAndRotation(RayOrigin, RayDirection.Rotation());
	// 視線が当たった、いちばん手前の1体だけ。固まっている間は、見えている人形の位置で判定する。
	AActor* Lit = nullptr;
	if (bHasRay)
	{
		const FVector End = RayOrigin + RayDirection * GazeLength;
		float Nearest = TNumericLimits<float>::Max();
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (!It->ActorHasTag(ToyTag)) continue;
			const FTBFrozenToy* F = Frozen.FindByPredicate([Toy = *It](const FTBFrozenToy& Each) { return Each.Toy == Toy; });
			FBox Box = F && F->Proxy ? F->Proxy->GetComponentsBoundingBox(true) : FBox(ForceInit);
			if (!Box.IsValid) Box = It->GetComponentsBoundingBox(true);
			FVector Hit, Normal;
			float Time;
			if (FMath::LineExtentBoxIntersection(Box, RayOrigin, End, FVector::ZeroVector, Hit, Normal, Time) && Time < Nearest &&
			    (!IsBlocked(Hit) || !IsBlocked(Box.GetCenter())))
			{
				Nearest = Time;
				Lit = *It;
			}
		}
	}
	if (Lit && !IsFrozen(Lit)) Freeze(Lit);
	for (int32 i = Frozen.Num() - 1; i >= 0; --i)
	{
		FTBFrozenToy& F = Frozen[i];
		F.UnlitSeconds = F.Toy == Lit ? 0.f : F.UnlitSeconds + DeltaTime;
		if (!F.Toy || F.UnlitSeconds >= ReleaseSeconds) Release(i);
	}
}

// 手前の壁や家具に隠れていれば照らさない。おもちゃ同士と、見えない物（字幕の板など）は素通しにする。
bool ATBTitleGaze::IsBlocked(const FVector& Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TitleGaze), true, this);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(ToyTag)) Params.AddIgnoredActor(*It);
	}
	for (const FTBFrozenToy& F : Frozen)
	{
		if (F.Proxy) Params.AddIgnoredActor(F.Proxy);
	}
	for (int32 Try = 0; Try < 8; ++Try)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, RayOrigin, Target, ECC_Visibility, Params)) return false;
		const UPrimitiveComponent* Comp = Hit.GetComponent();
		const AActor* Owner = Hit.GetActor();
		if (!Comp || (Comp->IsVisible() && !(Owner && Owner->IsHidden()))) return true;
		Params.AddIgnoredComponent(Comp);
	}
	return false;
}

AActor* ATBTitleGaze::MakeProxy(AActor* Toy)
{
	AActor* Proxy = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Toy->GetActorTransform());
	if (!Proxy) return nullptr;
	auto* Root = NewObject<USceneComponent>(Proxy);
	Proxy->SetRootComponent(Root);
	Root->RegisterComponent();
	Root->SetWorldTransform(Toy->GetActorTransform());
	TArray<USkeletalMeshComponent*> Meshes;
	Toy->GetComponents(Meshes);
	for (USkeletalMeshComponent* Src : Meshes)
	{
		if (!Src->IsVisible() || !Src->GetSkeletalMeshAsset()) continue;
		auto* Pose = NewObject<UPoseableMeshComponent>(Proxy);
		Pose->SetSkinnedAssetAndUpdate(Src->GetSkeletalMeshAsset());
		Pose->SetupAttachment(Root);
		Pose->RegisterComponent();
		Pose->SetWorldTransform(Src->GetComponentTransform());
		for (int32 m = 0; m < Src->GetNumMaterials(); ++m) Pose->SetMaterial(m, Src->GetMaterial(m));
		// クマの毛並みの層は本体の姿勢を借りて動き、自分の姿勢を持たないので、本体から写す。
		auto* Leader = Cast<USkeletalMeshComponent>(Src->LeaderPoseComponent.Get());
		Pose->CopyPoseFromSkeletalComponent(Leader ? Leader : Src);
	}
	return Proxy;
}

void ATBTitleGaze::Freeze(AActor* Toy)
{
	FTBFrozenToy& F = Frozen.AddDefaulted_GetRef();
	F.Toy = Toy;
	F.Proxy = MakeProxy(Toy);
	Toy->SetActorHiddenInGame(true);
}

void ATBTitleGaze::Release(int32 Index)
{
	FTBFrozenToy F = Frozen[Index];
	Frozen.RemoveAt(Index);
	if (F.Toy) F.Toy->SetActorHiddenInGame(false);
	if (F.Proxy) F.Proxy->Destroy();
}

void ATBTitleGaze::ReleaseAll()
{
	for (int32 i = Frozen.Num() - 1; i >= 0; --i) Release(i);
}

void ATBTitleGaze::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseAll();
	Super::EndPlay(Reason);
}
