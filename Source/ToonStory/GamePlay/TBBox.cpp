// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBBox.h"
#include "Audio/TBBoxAudioComponent.h"
#include "Audio/TBAudioEvents.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "GamePlay/TBDummyToy.h"
#include "GamePlay/TBRuleMath.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATBBox::ATBBox()
{
	CreateDefaultSubobject<UTBBoxAudioComponent>(TEXT("GameplayAudio"));
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(10);
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestVisual"));
	Visual->SetupAttachment(Root);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("Interaction"));
	InteractionPoint->SetupAttachment(Root);
	InteractionPoint->SetRelativeLocation(FVector(-40, 0, 25));
	// 木箱の底・四方の板・閉じた蓋に合わせた単純衝突。前方は-X。
	const FVector Centers[] = {{43, 0, 3}, {1, 0, 50}, {86, 0, 50}, {43, -78, 50}, {43, 78, 50}, {43, 0, 99}};
	const FVector Extents[] = {{43, 80, 3}, {3, 80, 49}, {3, 80, 49}, {43, 3, 49}, {43, 3, 49}, {43, 80, 4}};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Centers); ++Index)
	{
		auto* Collision = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("ChestCollision%d"), Index));
		Collision->SetupAttachment(Root);
		Collision->SetRelativeLocation(Centers[Index]);
		Collision->SetBoxExtent(Extents[Index]);
		Collision->SetCollisionProfileName(TEXT("BlockAll"));
	}
	StorageRoom = CreateDefaultSubobject<USceneComponent>(TEXT("StorageRoom"));
	StorageRoom->SetupAttachment(Root);
	StorageRoom->SetRelativeLocation(FVector(3000, 0, 0));
	// 収納後は従来の別室へ移動する。木箱の見た目や大きさから独立させる。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	const FVector RoomCenters[] = {{400, 0, -10},    {0, -210, 160},  {0, 210, 160}, {0, 0, 280}, {800, 0, 160},
	                               {400, -300, 160}, {400, 300, 160}, {400, 0, 330}, {0, 0, 120}};
	const FVector RoomScales[] = {{8, 6, .2},   {.2, 1.8, 3.2}, {.2, 1.8, 3.2}, {.2, 2.4, .8}, {.2, 6, 3.2},
	                              {8, .2, 3.2}, {8, .2, 3.2},   {8, 6, .2},     {.2, 2.4, 2.4}};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(RoomCenters); ++Index)
	{
		auto* Wall = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("StorageRoomWall%d"), Index));
		Wall->SetupAttachment(StorageRoom);
		Wall->SetStaticMesh(Cube.Object);
		Wall->SetRelativeLocation(RoomCenters[Index]);
		Wall->SetRelativeScale3D(RoomScales[Index]);
		Wall->SetCollisionProfileName(TEXT("BlockAll"));
		// 見た目は TB_ToyBoxRoom 側が持つので、ここは当たり判定だけ残す。蓋が開いた時に見えないようにする。
		Wall->SetHiddenInGame(true);
	}
}

void ATBBox::BeginPlay()
{
	Super::BeginPlay();
	RefreshAppearance();
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		AddTickPrerequisiteActor(GameMode);
	}
}

void ATBBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBBox, RescueProgress);
	DOREPLIFETIME(ATBBox, Rescuers);
}

bool ATBBox::InRange(const ATBCharacter* ToyCharacter) const
{
	if (!ToyCharacter ||
	    FVector::Dist(ToyCharacter->GetActorLocation(), InteractionPoint->GetComponentLocation()) > 180.f)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TBBoxInteraction), false, ToyCharacter);
	// 箱の壁も遮蔽判定に含め、内側からの救助操作を防ぐ。
	return !GetWorld()->LineTraceTestByChannel(ToyCharacter->GetPawnViewLocation(),
	                                           InteractionPoint->GetComponentLocation(), ECC_Visibility, Params);
}

bool ATBBox::HasPrisoners() const
{
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		if (It->TBPS() && It->TBPS()->ToyState == ETBToyState::Boxed)
		{
			return true;
		}
	}
	return HasDummyPrisoners();
}

bool ATBBox::CanStoreFrom(const ATBCharacter* Character) const
{
	if (!Character)
	{
		return false;
	}
	// 収納は小さな救助マーカーではなく、箱の正面全体で受け付ける。
	const FVector Local = GetActorTransform().InverseTransformPosition(Character->GetActorLocation());
	if (Local.X >= 0.f || Local.X < -180.f || FMath::Abs(Local.Y) > 140.f || Local.Z < 0.f || Local.Z > 220.f)
	{
		return false;
	}
	const FVector Target = GetActorTransform().TransformPosition(FVector(-10, FMath::Clamp(Local.Y, -75.f, 75.f), 60));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TBBoxStorage), false, Character);
	Params.AddIgnoredActor(this);
	return !GetWorld()->LineTraceTestByChannel(Character->GetPawnViewLocation(), Target, ECC_Visibility, Params);
}

// 蓋は -Y 側が持ち上がる（TBBoxLid）ので、その真下の天井近くから落とす。リモコンの上には落とさない。
FVector ATBBox::StorageSlot(int32 Slot)
{
	// 部屋は X 0〜800・Y ±300・天井 Z 320。3 列ずつ -Y の壁際へ並べる。
	constexpr int32 Columns = 3;
	constexpr float FirstX = 200.f, StepX = 180.f;
	constexpr float FirstY = -185.f, StepY = -30.f;
	constexpr float DropHeight = 220.f;
	return FVector(FirstX + (Slot % Columns) * StepX, FirstY + (Slot / Columns) * StepY, DropHeight);
}

const FName ATBBox::DropAreaTag(TEXT("TBBoxDropArea"));

const UBoxComponent* ATBBox::FindDropArea(const UWorld* World)
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (const auto* Area = It->ActorHasTag(DropAreaTag) ? It->FindComponentByClass<UBoxComponent>() : nullptr)
		{
			return Area;
		}
	}
	return nullptr;
}

// 落下エリアの上面から落とす位置を探す。収納済みの人数に応じて左右交互にずらし、
// 真下に誰かいる・上面が物に埋まっている位置は飛ばして次を試す。どこも使えなければ false。
bool ATBBox::FindDropSpot(const ATBCharacter* Toy, FVector& OutSpot) const
{
	const UBoxComponent* Area = FindDropArea(GetWorld());
	if (!Area)
	{
		return false;
	}
	const auto* State = TB::GS(GetWorld());
	const int32 First = State ? State->BoxedCount : 0;
	const FVector Extent = Area->GetScaledBoxExtent();
	const auto* Capsule = Toy->GetCapsuleComponent();
	const FCollisionShape Shape =
	    FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(TBStorageDrop), false, Toy);
	for (int32 Index = First; Index < First + StorageSlotCount; ++Index)
	{
		// 0人目は真ん中、以降は右・左・右…と間隔ずつ外へ。はみ出したら、その先も全部はみ出す。
		const float Side = (Index % 2 ? 1.f : -1.f) * ((Index + 1) / 2) * DropSpacing;
		if (FMath::Abs(Side) > Extent.Y)
		{
			break;
		}
		const FVector Top = Area->GetComponentLocation() + Area->GetRightVector() * Side + Area->GetUpVector() * Extent.Z;
		// 上面から真下へたどり、最初に当たるのが床（おもちゃ以外）なら空き。リモコンは傾いているので床の高さは決め打ちしない。
		FHitResult Hit;
		const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Top, Top - Area->GetUpVector() * Extent.Z * 4.f,
		                                                   FQuat::Identity, ECC_GameTraceChannel1, Shape, Params);
		const AActor* Below = bHit ? Hit.GetActor() : nullptr;
		if (!bHit || (!Hit.bStartPenetrating && !Cast<ATBCharacter>(Below) && !Cast<ATBDummyToy>(Below)))
		{
			OutSpot = Top;
			return true;
		}
	}
	return false;
}

// 収納の最終確認を行い、箱内へ移してから勝敗を確認する。
bool ATBBox::Store(ATBCharacter* Toy)
{
	if (!HasAuthority() || !Toy || !Toy->TBPS() || !TB::Playing(GetWorld()))
	{
		return false;
	}
	// 収納済みのおもちゃと重ならない空き位置を選ぶ。落下エリアに空きがあればそこを優先し、なければ別室の壁際へ。
	FVector StoragePosition;
	bool bFoundPosition = FindDropSpot(Toy, StoragePosition);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TBStorage), false, Toy);
	for (int32 Slot = 0; !bFoundPosition && Slot < StorageSlotCount; ++Slot)
	{
		const FVector Candidate = StorageRoom->GetComponentTransform().TransformPosition(StorageSlot(Slot));
		if (!GetWorld()->OverlapBlockingTestByChannel(
		        Candidate, FQuat::Identity, ECC_GameTraceChannel1,
		        FCollisionShape::MakeCapsule(Toy->GetCapsuleComponent()->GetScaledCapsuleRadius(),
		                                     Toy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
		        QueryParams))
		{
			StoragePosition = Candidate;
			bFoundPosition = true;
			break;
		}
	}
	if (!bFoundPosition)
	{
		return false;
	}
	Toy->Carrier = nullptr;
	Toy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Toy->SetActorLocation(StoragePosition, false, nullptr, ETeleportType::TeleportPhysics);
	Toy->SetFrozen(false);
	Toy->SetToyState(ETBToyState::Boxed);
	Toy->ForceNetUpdate();
	UTBAudioEvents::Emit(this, ETBSoundEvent::Store, Toy->TBPS(), InteractionPoint->GetComponentLocation());
	UTBAudioEvents::Emit(this, ETBSoundEvent::BoxedTransition, Toy->TBPS(), FVector::ZeroVector);
	UTBAudioEvents::Emit(this, ETBSoundEvent::TeamNotice, Toy->TBPS(), FVector::ZeroVector);
	MulticastOpened();
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->CheckWin();
	}
	return true;
}

// 救助人数と進捗をサーバーで更新する。
void ATBBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (!TB::Playing(GetWorld()))
	{
		RescueProgress = 0;
		Rescuers = 0;
		return;
	}
	int32 RescueCount = 0;
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		auto* ToyCharacter = *It;
		if (!ToyCharacter->bRescuing)
		{
			continue;
		}
		if (!ToyCharacter->bHoldingInteract || !ToyCharacter->CanAct() || ToyCharacter->IsHuman() ||
		    !InRange(ToyCharacter))
		{
			ToyCharacter->bRescuing = false;
			continue;
		}
		++RescueCount;
	}
	if (!HasPrisoners())
	{
		RescueCount = 0;
	}
	if (RescueCount == 0)
	{
		if (RescueProgress > 0.f && HasPrisoners())
			UTBAudioEvents::Emit(this, ETBSoundEvent::RescueCancel, nullptr, FVector::ZeroVector);
		Rescuers = 0;
		RescueProgress = 0;
		return;
	}
	Rescuers = RescueCount;
	RescueProgress =
	    FMath::Min(1.f, RescueProgress + DeltaSeconds * TBRuleMath::RescueRate(RescueCount, BaseRescueSeconds,
	                                                                           AdditionalRescuerBonus));
	if (RescueProgress >= 1.f)
	{
		ReleasePrisoners();
	}
}

// 空いている箱外の位置へ救助する。塞がっている場合は次のTickで再試行する。
void ATBBox::ReleasePrisoners()
{
	if (!HasAuthority() || !TB::Playing(GetWorld()))
	{
		return;
	}
	int32 Slot = 0;
	int32 Released = 0;
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		auto* Toy = *It;
		if (!Toy->TBPS() || Toy->TBPS()->ToyState != ETBToyState::Boxed)
		{
			continue;
		}
		const FVector Exit =
		    GetActorTransform().TransformPosition(FVector(-65 - (Slot / 3) * 35, -50 + (Slot % 3) * 50, 60));
		++Slot;
		if (Toy->TeleportTo(Exit, GetActorRotation()))
		{
			Toy->SetFrozen(false);
			Toy->SetToyState(ETBToyState::Free);
			++Released;
		}
	}
	Released += ReleaseDummyPrisoners(Slot);
	if (Released > 0)
	{
		UTBAudioEvents::Emit(this, ETBSoundEvent::RescueComplete, nullptr, InteractionPoint->GetComponentLocation());
		MulticastOpened();
	}
	if (!HasPrisoners())
	{
		RescueProgress = 0;
		Rescuers = 0;
		for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
		{
			It->bRescuing = false;
		}
	}
	ForceNetUpdate();
}

void ATBBox::MulticastOpened_Implementation()
{
	OnOpened.Broadcast();
}
