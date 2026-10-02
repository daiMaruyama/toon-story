#include "TBBox.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "GamePlay/TBRuleMath.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ATBBox::ATBBox()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(10);
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	auto* Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorLight"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(400, 0, 250));
	Light->SetIntensity(5000.f);
	Light->SetAttenuationRadius(900.f);
	InteractionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("Interaction"));
	InteractionPoint->SetupAttachment(Root);
	InteractionPoint->SetRelativeLocation(FVector(-120, 220, 60));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto* Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InteractMarker"));
	Marker->SetupAttachment(Root);
	if (Cube.Succeeded())
	{
		Marker->SetStaticMesh(Cube.Object);
	}
	Marker->SetRelativeLocation(FVector(-120, 220, 20));
	Marker->SetRelativeScale3D(FVector(.4, .4, .4));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const FVector Pos[] = {{400, 0, -10}, {0, -210, 160},   {0, 210, 160},   {0, 0, 280},
	                       {800, 0, 160}, {400, -300, 160}, {400, 300, 160}, {400, 0, 330}};
	const FVector Scale[] = {{8, 6, .2},   {.2, 1.8, 3.2}, {.2, 1.8, 3.2}, {.2, 2.4, .8},
	                         {.2, 6, 3.2}, {8, .2, 3.2},   {8, .2, 3.2},   {8, 6, .2}};
	for (int32 N = 0; N < 8; ++N)
	{
		auto* M = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wall%d"), N));
		M->SetupAttachment(Root);
		if (Cube.Succeeded())
		{
			M->SetStaticMesh(Cube.Object);
		}
		M->SetRelativeLocation(Pos[N]);
		M->SetRelativeScale3D(Scale[N]);
		M->SetCollisionProfileName(TEXT("BlockAll"));
	}
	// 前面は固定壁。救助は開閉を行わず、箱外への移動で成立する。
	auto* FrontWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontWall"));
	FrontWall->SetupAttachment(Root);
	if (Cube.Succeeded())
	{
		FrontWall->SetStaticMesh(Cube.Object);
	}
	FrontWall->SetRelativeLocation(FVector(0, 0, 120));
	FrontWall->SetRelativeScale3D(FVector(.2, 2.4, 2.4));
	FrontWall->SetCollisionProfileName(TEXT("BlockAll"));
}

void ATBBox::BeginPlay()
{
	Super::BeginPlay();
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
	DOREPLIFETIME(ATBBox, AlarmUntil);
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
		const FVector Candidate =
		    GetActorTransform().TransformPosition(FVector(200 + (Slot % 3) * 180, -170 + (Slot / 3) * 170, 60));
		if (!GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_GameTraceChannel1,
		                                              FCollisionShape::MakeCapsule(20.f, 35.f), QueryParams))
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
	if (Rescuers == 0)
	{
		AlarmUntil = GetWorld()->GetTimeSeconds() + 3;
		MulticastAlarm();
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
		    GetActorTransform().TransformPosition(FVector(-240 - (Slot / 5) * 100, -240 + (Slot % 5) * 100, 60));
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

void ATBBox::MulticastAlarm_Implementation()
{
	// 全員に警報を届けるため、距離減衰のない2D音声を使う。
	if (GetNetMode() != NM_DedicatedServer && AlarmSound)
	{
		UGameplayStatics::PlaySound2D(this, AlarmSound);
	}
	AlarmVisual();
}
