#include "TBHumanEntrance.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TBGameState.h"
#include "Core/TBGameHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Character/TBCharacter.h"
#include "Character/TBCharacterGeometry.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

ATBHumanEntrance::ATBHumanEntrance()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	ApproachStart = CreateDefaultSubobject<USceneComponent>(TEXT("ApproachStart"));
	ApproachStart->SetupAttachment(Root);
	ApproachStart->SetRelativeLocation(FVector(-600, 0, 150));
	ApproachEnd = CreateDefaultSubobject<USceneComponent>(TEXT("ApproachEnd"));
	ApproachEnd->SetupAttachment(Root);
	ApproachEnd->SetRelativeLocation(FVector(-100, 0, 150));
	DoorLeaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeaf"));
	DoorLeaf->SetupAttachment(Root);
	DoorLeaf->SetMobility(EComponentMobility::Movable);
	DoorLeaf->SetCollisionProfileName(TEXT("BlockAll"));
	DoorLeaf->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OpenDoorTransform = FTransform(FRotator(0, 90, 0));
}

void ATBHumanEntrance::SetPreparing(bool Closed)
{
	if (bDoorApplied && bDoorClosed == Closed) return;
	bDoorApplied = true; bDoorClosed = Closed;
	DoorLeaf->SetRelativeTransform(Closed ? ClosedDoorTransform : OpenDoorTransform);
	DoorLeaf->SetCollisionEnabled(Closed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ATBHumanEntrance::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const auto* State = TB::GS(GetWorld());
	SetPreparing(State && State->IsPreparing());
}

ATBHumanEntrance* ATBHumanEntrance::Find(const UWorld* World)
{
	for (TActorIterator<ATBHumanEntrance> It(World); It; ++It) return *It;
	return nullptr;
}

bool ATBHumanEntrance::Validate(int32 Capacity, FString& Error) const
{
	if (!FMath::IsFinite(FootstepIntervalSeconds) || FootstepIntervalSeconds < .1f)
	{ Error = TEXT("入口の足音間隔は0.1秒以上にしてください。"); return false; }
	if (Capacity < 1 || DoorSlots.Num() < Capacity)
	{ Error = TEXT("入口の人数分の開始位置が不足しています。"); return false; }
	TSet<const AActor*> Seen;
	for (const auto& Slot : DoorSlots)
	{
		if (!IsValid(Slot) || Slot->GetWorld() != GetWorld() || Seen.Contains(Slot.Get()) ||
		    Slot->GetActorLocation().ContainsNaN())
		{ Error = TEXT("入口の開始位置が無効または重複しています。"); return false; }
		Seen.Add(Slot.Get());
	}
	if (ApproachStart->GetComponentLocation().ContainsNaN() || ApproachEnd->GetComponentLocation().ContainsNaN())
	{ Error = TEXT("入口カメラの位置が無効です。"); return false; }
	if (!DoorLeaf->GetStaticMesh() || ClosedDoorTransform.ContainsNaN() || OpenDoorTransform.ContainsNaN())
	{ Error = TEXT("入口の扉のmeshと閉・開transformを設定してください。"); return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TBEntranceSlots), false, this);
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It) Params.AddIgnoredActor(*It);
	for (const auto& Slot : DoorSlots) Params.AddIgnoredActor(Slot.Get());
	// Use the same standing capsule as the character, including before participants are assigned roles.
	const FVector HumanExtent(TBCharacterGeometry::HumanRadius, TBCharacterGeometry::HumanRadius,
	    TBCharacterGeometry::HumanHalfHeight);
	const FBox ClosedBounds = DoorLeaf->GetStaticMesh()->GetBoundingBox().TransformBy(ClosedDoorTransform * Root->GetComponentTransform());
	for (int32 Index = 0; Index < DoorSlots.Num(); ++Index)
	{
		const FVector Position = DoorSlots[Index]->GetActorLocation();
		if (ClosedBounds.Intersect(FBox(Position - HumanExtent, Position + HumanExtent)) ||
		    !TB::SupportsCapsule(GetWorld(), Position, TBCharacterGeometry::HumanRadius,
		        TBCharacterGeometry::HumanHalfHeight, ECC_Pawn, Params))
		{ Error = TEXT("入口の開始位置に安全な床・頭上空間がありません。"); return false; }
		for (int32 Other = 0; Other < Index; ++Other)
		{
			const FVector Delta = Position - DoorSlots[Other]->GetActorLocation();
			if (Delta.SizeSquared2D() < FMath::Square(2.f * TBCharacterGeometry::HumanRadius) && FMath::Abs(Delta.Z) < 2.f * TBCharacterGeometry::HumanHalfHeight)
			{ Error = TEXT("入口の開始位置どうしが重なっています。"); return false; }
		}
	}
	return true;
}

bool ATBHumanEntrance::CameraPose(float Alpha, FVector& Location, FRotator& Rotation) const
{
	if (!ApproachStart || !ApproachEnd) return false;
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	Location = FMath::Lerp(ApproachStart->GetComponentLocation(), ApproachEnd->GetComponentLocation(), Alpha);
	Rotation = FQuat::Slerp(ApproachStart->GetComponentQuat(), ApproachEnd->GetComponentQuat(), Alpha).Rotator();
	return !Location.ContainsNaN() && !Rotation.ContainsNaN();
}
