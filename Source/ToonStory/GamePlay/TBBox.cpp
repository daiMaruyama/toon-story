#include "TBBox.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "GamePlay/TBRuleMath.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATBBox::ATBBox()
{
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
	auto* Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("StorageRoomLight"));
	Light->SetupAttachment(StorageRoom);
	Light->SetRelativeLocation(FVector(400, 0, 250));
	Light->SetIntensity(5000.f);
	Light->SetAttenuationRadius(900.f);
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
	}
}

void ATBBox::BeginPlay()
{
	Super::BeginPlay();
	Visual->SetStaticMesh(BoxMesh.LoadSynchronous());
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
	return false;
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

// 収納の最終確認を行い、箱内へ移してから勝敗を確認する。
bool ATBBox::Store(ATBCharacter* Toy)
{
	if (!HasAuthority() || !Toy || !Toy->TBPS() || !TB::Playing(GetWorld()))
	{
		return false;
	}
	// 収納済みのおもちゃと重ならない空き位置を選ぶ。
	FVector StoragePosition;
	bool bFoundPosition = false;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TBStorage), false, Toy);
	for (int32 Slot = 0; Slot < 9; ++Slot)
	{
		const FVector Candidate = StorageRoom->GetComponentTransform().TransformPosition(
		    FVector(200 + (Slot % 3) * 180, -170 + (Slot / 3) * 170, 60));
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
		}
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
