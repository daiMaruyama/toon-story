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
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"


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
	if (!IsValid(Box) || Box->GetWorld() != GetWorld() || !IsValid(Goal) || Goal->GetWorld() != GetWorld() ||
	    Goal->GetActorLocation().ContainsNaN() || ExitPoints.IsEmpty() || ExitPoints.Num() > ATBBox::StorageSlotCount)
	{ Error = TEXT("箱・脱出高さマーカー・出口を設定してください。"); return false; }
	const FBox RoomBounds = Box->StorageRoomBounds();
	const FVector LocalGoal = Box->StorageRoom->GetComponentTransform().InverseTransformPosition(Goal->GetActorLocation());
	if (!RoomBounds.IsValid || LocalGoal.Z <= RoomBounds.Min.Z || LocalGoal.Z >= RoomBounds.Max.Z)
	{ Error = TEXT("脱出高さマーカーは箱内室の床と天井の間に置いてください。"); return false; }
	if (CourseBounds->GetScaledBoxExtent().GetMin() <= 0 ||
	    !CourseBounds->GetComponentRotation().Equals(FRotator::ZeroRotator, .001f))
	{ Error = TEXT("コース範囲の大きさまたは回転が無効です。"); return false; }
	for (const auto& Exit : ExitPoints)
	{
		if (!IsValid(Exit) || Exit->GetWorld() != GetWorld() || Exit->GetActorLocation().ContainsNaN())
		{ Error = TEXT("脱出先が無効です。"); return false; }
		if (CheckSafety && !SafeExitPoint(Exit->GetActorLocation()))
		{ Error = TEXT("出口をコースと箱内室の外の安全な床へ置いてください。"); return false; }
	}
	return true;
}

bool ATBBoxEscapeCourse::AboveEscapeHeight(const ATBCharacter* Toy) const
{
	if (!IsValid(Toy) || !IsValid(Box) || !IsValid(Goal)) return false;
	const FVector Local = Box->StorageRoom->GetComponentTransform().InverseTransformPosition(Toy->GetActorLocation());
	const FBox Room = Box->StorageRoomBounds();
	return Room.IsValid && Local.X >= Room.Min.X && Local.X <= Room.Max.X &&
	    Local.Y >= Room.Min.Y && Local.Y <= Room.Max.Y && Toy->GetActorLocation().Z > Goal->GetActorLocation().Z;
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

bool ATBBoxEscapeCourse::Contains(const UBoxComponent* Volume, const ATBCharacter* Toy) const
{
	if (!Volume || !Toy) return false;
	const FVector Scale = Volume->GetComponentScale().GetAbs();
	if (Scale.GetMin() <= SMALL_NUMBER) return false;
	const FVector Local = Volume->GetComponentTransform().InverseTransformPosition(Toy->GetActorLocation());
	const FVector Extent = Volume->GetUnscaledBoxExtent();
	const auto* Capsule = Toy->GetCapsuleComponent();
	return FMath::Abs(Local.X) + Capsule->GetScaledCapsuleRadius() / Scale.X <= Extent.X &&
	       FMath::Abs(Local.Y) + Capsule->GetScaledCapsuleRadius() / Scale.Y <= Extent.Y &&
	       FMath::Abs(Local.Z) + Capsule->GetScaledCapsuleHalfHeight() / Scale.Z <= Extent.Z;
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
	if (!Validate(Error, false)) return;
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
		if (Contains(CourseBounds, Toy) && Toy->GetActorLocation().Z < Goal->GetActorLocation().Z)
			Attempt.bDescendedBelowLine = true;
		if (Contains(CourseBounds, Toy)) Toy->SetFrozen(false);
		if (!Attempt.bDescendedBelowLine || !AboveEscapeHeight(Toy)) continue;
		// Server deadlines and the current capture token still govern release.
		if (auto* Mode = GetWorld()->GetAuthGameMode<ATBGameMode>()) Mode->CheckWin();
		if (!Eligible(Toy) || State->Phase != ETBPhase::Playing) continue;
		TArray<FTransform> Exits;
		for (const auto& Exit : ExitPoints)
			if (SafeExitPoint(Exit->GetActorLocation(), Toy)) Exits.Add(Exit->GetActorTransform());
		if (Box->ReleasePrisoner(Toy, Capture, Exits)) Attempts.Remove(Toy);
	}
}
