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
	// 光が外れてから踊りに戻り始めるまで。
	constexpr float ReleaseSeconds = .4f;
	// 固まった姿から今の踊りへ溶け込ませる長さ。
	constexpr float BlendTime = .3f;
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
	return Frozen.ContainsByPredicate([Toy](const FTBFrozenToy& F) { return F.Toy == Toy && F.BlendSeconds < 0.f; });
}

void ATBTitleGaze::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// メニューを出している間だけ。寸劇の最中は照らさない。
	const bool bActive = Menu.IsValid() && Menu->GetState() == UTBTitleMenu::EState::Menu;
	if (!bActive)
	{
		// 寸劇に切り替わったら、固めた姿を待たずに片付ける。
		ClearGaze();
		Light->SetVisibility(false);
		ReleaseAll();
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	float X, Y;
	FVector Origin, Direction;
	if (PC && PC->GetMousePosition(X, Y) && PC->DeprojectScreenPositionToWorld(X, Y, Origin, Direction))
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
			    !IsBlocked(Hit))
			{
				Nearest = Time;
				Lit = *It;
			}
		}
	}
	if (Lit && !Frozen.ContainsByPredicate([Lit](const FTBFrozenToy& F) { return F.Toy == Lit; })) Freeze(Lit);
	for (int32 i = Frozen.Num() - 1; i >= 0; --i)
	{
		FTBFrozenToy& F = Frozen[i];
		if (!F.Toy)
		{
			Release(i);
			continue;
		}
		if (F.Toy == Lit)
		{
			// 戻りかけていたら、その姿のまま固め直す。
			F.UnlitSeconds = 0.f;
			F.BlendSeconds = -1.f;
			continue;
		}
		F.UnlitSeconds += DeltaTime;
		if (F.BlendSeconds < 0.f)
		{
			if (F.UnlitSeconds < ReleaseSeconds) continue;
			StartBlend(F);
		}
		F.BlendSeconds += DeltaTime;
		Blend(F, FMath::SmoothStep(0.f, BlendTime, F.BlendSeconds));
		if (F.BlendSeconds >= BlendTime) Release(i);
	}
}

void ATBTitleGaze::StartBlend(FTBFrozenToy& F)
{
	F.BlendSeconds = 0.f;
	for (FTBFrozenMesh& M : F.Meshes)
	{
		if (!M.Pose) continue;
		M.StartWorld = M.Pose->GetComponentTransform();
		M.StartBones = M.Pose->GetComponentSpaceTransforms();
	}
}

// 人形の姿を、隠れて踊り続けている本物の今の姿へ寄せる。1 で本物と同じ。
void ATBTitleGaze::Blend(FTBFrozenToy& F, float Alpha)
{
	for (const FTBFrozenMesh& M : F.Meshes)
	{
		if (!M.Pose || !M.Source || !M.PoseSource) continue;
		FTransform World;
		World.Blend(M.StartWorld, M.Source->GetComponentTransform(), Alpha);
		M.Pose->SetWorldTransform(World);
		const TArray<FTransform>& Target = M.PoseSource->GetComponentSpaceTransforms();
		const int32 Num = FMath::Min3(Target.Num(), M.StartBones.Num(), M.Pose->GetNumBones());
		for (int32 b = 0; b < Num; ++b)
		{
			FTransform Bone;
			Bone.Blend(M.StartBones[b], Target[b], Alpha);
			M.Pose->SetBoneTransformByName(M.Pose->GetBoneName(b), Bone, EBoneSpaces::ComponentSpace);
		}
	}
}

// カーソルが指した位置までに壁や家具があれば照らさない。おもちゃ同士と、見えない物（字幕の板など）は素通しにする。
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

void ATBTitleGaze::MakeProxy(FTBFrozenToy& F)
{
	AActor* Toy = F.Toy;
	AActor* Proxy = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Toy->GetActorTransform());
	F.Proxy = Proxy;
	if (!Proxy) return;
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
		FTBFrozenMesh& M = F.Meshes.AddDefaulted_GetRef();
		M.Source = Src;
		M.PoseSource = Leader ? Leader : Src;
		M.Pose = Pose;
	}
}

void ATBTitleGaze::Freeze(AActor* Toy)
{
	FTBFrozenToy& F = Frozen.AddDefaulted_GetRef();
	F.Toy = Toy;
	MakeProxy(F);
	// 隠れている間は骨の姿勢が止まり、表示し直した瞬間に古い姿勢が映るので、隠している間も動かす。
	TArray<USkeletalMeshComponent*> Meshes;
	Toy->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		F.Ticked.Add(Mesh);
		F.OldTicks.Add(Mesh->VisibilityBasedAnimTickOption);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	Toy->SetActorHiddenInGame(true);
}

void ATBTitleGaze::Release(int32 Index)
{
	FTBFrozenToy F = Frozen[Index];
	Frozen.RemoveAt(Index);
	for (int32 m = 0; m < F.Ticked.Num(); ++m)
	{
		if (F.Ticked[m]) F.Ticked[m]->VisibilityBasedAnimTickOption = F.OldTicks[m];
	}
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
