#include "TBBoxEscapeCourse.h"
#include "TBBox.h"

#include "Character/TBCharacter.h"
#include "Character/TBCharacterGeometry.h"
#include "Core/TBGameMode.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameHelpers.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/TriggerBox.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/Package.h"


ATBBoxEscapeCourse::ATBBoxEscapeCourse()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(5);
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	CourseBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CourseBounds"));
	CourseBounds->SetupAttachment(Root);
	CourseBounds->SetBoxExtent(FVector(500, 500, 300));
	CourseBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

void ATBBoxEscapeCourse::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Mode = GetWorld()->GetAuthGameMode<ATBGameMode>()) AddTickPrerequisiteActor(Mode);
}

ATBBoxEscapeCourse* ATBBoxEscapeCourse::Find(const UWorld* World, const ATBBox* ForBox)
{
	for (TActorIterator<ATBBoxEscapeCourse> It(World); It; ++It) if (It->Box == ForBox) return *It;
	return nullptr;
}

bool ATBBoxEscapeCourse::Validate(FString& Error, bool CheckSafety) const
{
	TArray<UPrimitiveComponent*> Supports;
	return ValidateWithSupports(Error, CheckSafety, Supports);
}

bool ATBBoxEscapeCourse::ValidateWithSupports(FString& Error, bool CheckSafety, TArray<UPrimitiveComponent*>& Supports) const
{
	Error.Reset();
	Supports.Reset();
	if (!IsValid(Box) || Box->GetWorld() != GetWorld() || !IsValid(Goal) || Goal->GetWorld() != GetWorld() ||
	    Goal->GetActorLocation().ContainsNaN() || ExitPoints.IsEmpty() || ExitPoints.Num() > ATBBox::StorageSlotCount)
	{ Error = TEXT("箱・最終到達領域・出口を設定してください。"); return false; }
	const FBox RoomBounds = Box->StorageRoomBounds();
	const FVector LocalGoal = Box->StorageRoom->GetComponentTransform().InverseTransformPosition(Goal->GetActorLocation());
	if (!RoomBounds.IsValid || LocalGoal.Z <= RoomBounds.Min.Z || LocalGoal.Z >= RoomBounds.Max.Z)
	{ Error = TEXT("最終到達領域は箱内室の床と天井の間に置いてください。"); return false; }
	if (CourseBounds->GetScaledBoxExtent().GetMin() <= 0 ||
	    !CourseBounds->GetComponentRotation().Equals(FRotator::ZeroRotator, .001f))
	{ Error = TEXT("コース範囲の大きさまたは回転が無効です。"); return false; }
	if (!ResolveFinalSupports(Supports, Error)) return false;
	for (const auto& Exit : ExitPoints)
	{
		if (!IsValid(Exit) || Exit->GetWorld() != GetWorld() || Exit->GetActorLocation().ContainsNaN())
		{ Error = TEXT("脱出先が無効です。"); return false; }
		if (CheckSafety && !SafeExitPoint(Exit->GetActorLocation()))
		{ Error = TEXT("出口をコースと箱内室の外の安全な床へ置いてください。"); return false; }
	}
	return true;
}

bool ATBBoxEscapeCourse::ResolveFinalSupports(TArray<UPrimitiveComponent*>& Out, FString& Error) const
{
	Out.Reset();
	const auto* Volume = IsValid(Goal) ? Cast<UBoxComponent>(Goal->GetCollisionComponent()) : nullptr;
	if (!IsValid(Box) || !Volume || FinalSupportTag.IsNone() || FinalSupportLevelPackage.IsNone() ||
	    ExpectedFinalSupportCount < 1 || ExpectedFinalSupportCount > 2 || !FMath::IsFinite(GoalMarginCm) || GoalMarginCm < 0 || GoalMarginCm > 5 ||
	    Volume->GetComponentTransform().ContainsNaN() || Volume->GetComponentScale().GetMin() <= 0 ||
	    !Volume->GetComponentRotation().Equals(FRotator::ZeroRotator, .001f) ||
	    (Volume->IsQueryCollisionEnabled() && Volume->GetCollisionResponseToChannel(ECC_GameTraceChannel1) == ECR_Block))
	{ Error = TEXT("最終領域・余白・支持面タグ/共通level/想定数の設定が無効です。"); return false; }
	const FBox GoalBox = Volume->Bounds.GetBox();
	const FVector Extent = Volume->GetScaledBoxExtent();
	if (Extent.ContainsNaN() || Extent.X <= TBCharacterGeometry::ToyRadius + GoalMarginCm || Extent.Y <= TBCharacterGeometry::ToyRadius + GoalMarginCm ||
	    Extent.Z <= TBCharacterGeometry::ToyHalfHeight + GoalMarginCm)
	{ Error = TEXT("最終領域が玩具capsuleと安全余白に対して小さすぎます。"); return false; }
	ULevel* SharedLevel = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		TInlineComponentArray<UPrimitiveComponent*> Parts(*It);
		for (UPrimitiveComponent* Part : Parts)
		{
			if (!IsValid(Part) || !Part->ComponentHasTag(FinalSupportTag)) continue;
			auto* Level = It->GetLevel();
			if (!Level || FName(*UWorld::RemovePIEPrefix(Level->GetOutermost()->GetName())) != FinalSupportLevelPackage ||
			    (SharedLevel && SharedLevel != Level) || !Part->IsRegistered() || !Part->IsQueryCollisionEnabled() || Part->IsSimulatingPhysics() ||
			    Part->GetComponentTransform().ContainsNaN() || Part->GetComponentScale().GetMin() <= 0 ||
			    Part->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block)
			{ Error = TEXT("最終面タグは指定共通level内の有効な非物理Block支持componentだけへ付けてください。"); return false; }
			SharedLevel = Level;
			const FBox Surface = Part->Bounds.GetBox();
			const FVector InRoom = Box->StorageRoom->GetComponentTransform().InverseTransformPosition(Surface.GetCenter());
			if (!Surface.IsValid || Surface.Min.ContainsNaN() || Surface.Max.ContainsNaN() || !Box->StorageRoomBounds().IsInsideOrOn(InRoom) ||
			    Surface.Min.X > GoalBox.Min.X + GoalMarginCm || Surface.Max.X < GoalBox.Max.X - GoalMarginCm ||
			    Surface.Min.Y > GoalBox.Min.Y + GoalMarginCm || Surface.Max.Y < GoalBox.Max.Y - GoalMarginCm)
			{ Error = TEXT("タグ付き最終面は箱室内で最終領域の足元を覆う同位置の面に限定してください。"); return false; }
			Out.Add(Part);
		}
	}
	if (Out.Num() != ExpectedFinalSupportCount)
	{ Error = TEXT("最終支持面タグが欠落または想定外に重複しています。"); return false; }
	// Render bounds alone do not prove support: inspect the actual first blocking geometry at centre/corners.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TBFinalSupportContract), false);
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It) Params.AddIgnoredActor(*It);
	double FirstHeight = 0;
	for (int32 Sample = 0; Sample < 5; ++Sample)
	{
		const double X = Sample == 0 ? GoalBox.GetCenter().X : (Sample & 1) ? GoalBox.Min.X + GoalMarginCm : GoalBox.Max.X - GoalMarginCm;
		const double Y = Sample == 0 ? GoalBox.GetCenter().Y : (Sample & 2) ? GoalBox.Min.Y + GoalMarginCm : GoalBox.Max.Y - GoalMarginCm;
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, FVector(X, Y, GoalBox.Max.Z), FVector(X, Y, GoalBox.Min.Z - 4), ECC_GameTraceChannel1, Params) ||
		    Hit.bStartPenetrating || !Out.Contains(Hit.GetComponent()) || Hit.ImpactNormal.Z < .71f ||
		    (Sample > 0 && FMath::Abs(Hit.ImpactPoint.Z - FirstHeight) > .5) ||
		    GoalBox.Min.Z + GoalMarginCm > Hit.ImpactPoint.Z + UCharacterMovementComponent::MIN_FLOOR_DIST ||
		    GoalBox.Max.Z - GoalMarginCm < Hit.ImpactPoint.Z + 2 * TBCharacterGeometry::ToyHalfHeight + UCharacterMovementComponent::MAX_FLOOR_DIST)
		{ Error = TEXT("最終領域を指定された水平支持面の直上へ、立位capsule全体が収まる高さで配置してください。"); return false; }
		if (Sample == 0) FirstHeight = Hit.ImpactPoint.Z;
	}
	for (UPrimitiveComponent* Surface : Out)
		if (FMath::Abs(Surface->Bounds.GetBox().Max.Z - FirstHeight) > .5)
		{ Error = TEXT("許可する重複最終面の上端は同じ支持高さへ合わせてください。"); return false; }
	return true;
}

bool ATBBoxEscapeCourse::ReachedGoal(const ATBCharacter* Toy) const
{
	TArray<UPrimitiveComponent*> Supports; FString Error;
	return ResolveFinalSupports(Supports, Error) && IsAtGoalHeight(Toy);
}

bool ATBBoxEscapeCourse::IsAtGoalHeight(const ATBCharacter* Toy) const
{
	const auto* Volume = IsValid(Goal) ? Cast<UBoxComponent>(Goal->GetCollisionComponent()) : nullptr;
	if (!IsValid(Toy) || !Volume || !Contains(CourseBounds, Toy)) return false;
	const FVector Position = Toy->GetActorLocation();
	// The bottom of the existing goal volume defines a horizontal finish line.
	// Jumping across it counts; no precise XY landing or platform contact is needed.
	return !Position.ContainsNaN() &&
	    Position.Z - Toy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() >= Volume->Bounds.GetBox().Min.Z - GoalMarginCm;
}

bool ATBBoxEscapeCourse::SafeExitPoint(FVector Position, const ATBCharacter* Toy) const
{
	const float Radius = Toy ? Toy->GetCapsuleComponent()->GetScaledCapsuleRadius() : TBCharacterGeometry::ToyRadius;
	const float Half = Toy ? Toy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : TBCharacterGeometry::ToyHalfHeight;
	const FBox CapsuleBox(Position - FVector(Radius, Radius, Half), Position + FVector(Radius, Radius, Half));
	const FVector InRoom = Box->StorageRoom->GetComponentTransform().InverseTransformPosition(Position);
	if (CourseBounds->Bounds.GetBox().Intersect(CapsuleBox) ||
	    Box->StorageRoomBounds().ExpandBy(FMath::Max(Radius, Half)).IsInsideOrOn(InRoom)) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TBCourseExit), false, Toy);
	if (!Toy) for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It) Params.AddIgnoredActor(*It);
	return TB::SupportsCapsule(GetWorld(), Position, Radius, Half, ECC_GameTraceChannel1, Params,
	    Toy ? Toy->GetCharacterMovement()->GetWalkableFloorZ() : .71f);
}

bool ATBBoxEscapeCourse::Contains(const UBoxComponent* Volume, const ATBCharacter* Toy, float Margin) const
{
	if (!Volume || !Toy) return false;
	const FVector Scale = Volume->GetComponentScale().GetAbs();
	if (Scale.GetMin() <= SMALL_NUMBER) return false;
	const FVector Local = Volume->GetComponentTransform().InverseTransformPosition(Toy->GetActorLocation());
	const FVector Extent = Volume->GetUnscaledBoxExtent();
	const auto* Capsule = Toy->GetCapsuleComponent();
	return FMath::Abs(Local.X) + (Capsule->GetScaledCapsuleRadius() + Margin) / Scale.X <= Extent.X &&
	       FMath::Abs(Local.Y) + (Capsule->GetScaledCapsuleRadius() + Margin) / Scale.Y <= Extent.Y &&
	       FMath::Abs(Local.Z) + (Capsule->GetScaledCapsuleHalfHeight() + Margin) / Scale.Z <= Extent.Z;
}

bool ATBBoxEscapeCourse::Eligible(const ATBCharacter* Toy) const
{
	const auto* State = TB::GS(GetWorld());
	const auto* PC = IsValid(Toy) ? Cast<ATBController>(Toy->GetController()) : nullptr;
	return IsValid(Box) && IsValid(Toy) && Toy->TBPS() && !Toy->TBPS()->IsABot() &&
	    IsValid(Toy->GetController()) && Toy->GetController()->GetPawn() == Toy &&
	    IsValid(PC) && IsValid(PC->Player) && PC->GetPlayerState<ATBPlayerState>() == Toy->TBPS() &&
	    Toy->TBPS()->Team == ETBTeam::Toy && Toy->TBPS()->ToyState == ETBToyState::Boxed &&
	    State && State->Phase == ETBPhase::Playing && !State->IsPreparing() &&
	    State->PlayerArray.Contains(Toy->TBPS()) &&
	    State->EndTime > State->GetServerWorldTimeSeconds() && Box->CaptureGeneration(Toy) != 0;
}

bool ATBBoxEscapeCourse::Protects(const ATBCharacter* Toy) const
{
	return Eligible(Toy) && Contains(CourseBounds, Toy);
}

void ATBBoxEscapeCourse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	const auto* State = TB::GS(GetWorld());
	if (!State || State->Phase != ETBPhase::Playing || State->IsPreparing())
	{ Attempts.Reset(); return; }
	FString Error;
	TArray<UPrimitiveComponent*> Supports;
	if (!ValidateWithSupports(Error, false, Supports)) return;
	for (auto It = Attempts.CreateIterator(); It; ++It)
		if (!Eligible(It.Key().Get())) It.RemoveCurrent();
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		auto* Toy = *It;
		if (!Eligible(Toy)) continue;
		const uint32 Capture = Box->CaptureGeneration(Toy);
		auto& Attempt = Attempts.FindOrAdd(Toy);
		if (Attempt.Generation != Capture)
		{
			Attempt = FCaptureAttempt();
			Attempt.Generation = Capture;
		}
		// Storage drops toys from above. A fresh capture must descend before any ascent can release it.
		const auto* GoalVolume = Cast<UBoxComponent>(Goal->GetCollisionComponent());
		if (Contains(CourseBounds, Toy) && Toy->GetActorLocation().Z + Toy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() < GoalVolume->Bounds.GetBox().Min.Z)
			Attempt.bDescendedBelowGoal = true;
		if (Contains(CourseBounds, Toy)) Toy->SetFrozen(false);
		if (!Attempt.bDescendedBelowGoal || !IsAtGoalHeight(Toy)) continue;
		// Server deadlines and the current capture token still govern release.
		if (auto* Mode = GetWorld()->GetAuthGameMode<ATBGameMode>()) Mode->CheckWin();
		if (!Eligible(Toy) || State->Phase != ETBPhase::Playing) continue;
		TArray<FTransform> Exits;
		for (const auto& Exit : ExitPoints)
			if (SafeExitPoint(Exit->GetActorLocation(), Toy)) Exits.Add(Exit->GetActorTransform());
		if (Box->ReleasePrisoner(Toy, Capture, Exits)) Attempts.Remove(Toy);
	}
}
