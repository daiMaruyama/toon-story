#include "GamePlay/Items/TBEscapeVehicle.h"
#include "GamePlay/Items/TBEscapeVehicleMovement.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Data/TBEscapeVehicleData.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATBEscapeVehicle::ATBEscapeVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("VehicleCollision"));
	Collision->SetBoxExtent(FVector(55.f, 40.f, 20.f));
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BoardingArea = CreateDefaultSubobject<UBoxComponent>(TEXT("BoardingArea"));
	BoardingArea->SetupAttachment(Collision);
	BoardingArea->SetBoxExtent(FVector(65.f, 50.f, 35.f));
	BoardingArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardingArea->SetCollisionResponseToAllChannels(ECR_Overlap);
	SetRootComponent(Collision);
	VehicleMovement = CreateDefaultSubobject<UTBEscapeVehicleMovement>(TEXT("VehicleMovement"));
	VehicleMovement->SetUpdatedComponent(Collision);
	Root->SetupAttachment(Collision);
	Mesh->SetupAttachment(Collision);
	InteractionPoint->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetRelativeScale3D(FVector(1.1f, .8f, .4f));
	DisplayName = NSLOCTEXT("TBItems", "EscapeVehicle", "脱出用の車");
	Actions.Reset();
	FTBItemAction Action;
	Action.Id = TEXT("Board");
	Action.Label = NSLOCTEXT("TBItems", "BoardEscapeVehicle", "乗り込む");
	Action.AllowedTeams = ETBInteractionTeams::ToyOnly;
	Actions.Add(Action);
	VehicleData = TSoftObjectPtr<UTBEscapeVehicleData>(FSoftObjectPath(TEXT("/Game/Data/DA_EscapeVehicle.DA_EscapeVehicle")));
}

const UTBEscapeVehicleData& ATBEscapeVehicle::Data() const
{
	return LoadedData ? *LoadedData : *GetDefault<UTBEscapeVehicleData>();
}

void ATBEscapeVehicle::BeginPlay()
{
	Super::BeginPlay();
	LoadedData = VehicleData.LoadSynchronous();
	StartTransform = GetActorTransform();
	BoardingArea->OnComponentBeginOverlap.AddDynamic(this, &ATBEscapeVehicle::OnBoardingOverlap);
	for (FTBItemAction& Action : Actions) Action.MaxDistance = FMath::Max(1.f, Data().InteractionDistance);
}

void ATBEscapeVehicle::OnBoardingOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (HasAuthority()) Board(Cast<ATBCharacter>(Other));
}

bool ATBEscapeVehicle::CanPerformAction_Implementation(const FTBInteractionContext& Context) const
{
	const auto* Match = TB::GS(GetWorld());
	return Match && Match->bEscapePhase && Context.InstigatorCharacter && !Context.InstigatorCharacter->EscapeVehicle;
}

bool ATBEscapeVehicle::PerformAction_Implementation(const FTBInteractionContext& Context)
{
	return Board(Context.InstigatorCharacter);
}

bool ATBEscapeVehicle::IsDriver(const ATBCharacter* Toy) const
{
	return Passengers.Num() > 0 && Passengers[0] == Toy;
}

bool ATBEscapeVehicle::Board(ATBCharacter* Toy)
{
	const auto* Match = TB::GS(GetWorld());
	if (!HasAuthority() || !IsValid(Toy) || !Match || Match->Phase != ETBPhase::Playing ||
	    Match->IsPreparing() || !Match->bEscapePhase || !Toy->TBPS() || Toy->TBPS()->Team != ETBTeam::Toy ||
	    Toy->TBPS()->ToyState != ETBToyState::Free || Toy->Carrier || Toy->EscapeVehicle) return false;
	// NPC は運転も降車もしない。最初に乗ると運転手の席がふさがり、誰も動かせなくなる。
	if (!Toy->IsPlayerControlled()) return false;
	Toy->ClearHeldInteraction();
	Toy->SetFrozen(false);
	Toy->TBPS()->SetGaze(ETBGazeState::None);
	Toy->EscapeVehicle = this;
	Passengers.Add(Toy);
	Toy->GetCharacterMovement()->StopMovementImmediately();
	Toy->GetCharacterMovement()->DisableMovement();
	Toy->SetActorEnableCollision(false);
	Toy->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	Toy->SetActorRelativeLocation(FVector(0, 0, 25));
	Toy->SetActorHiddenInGame(true);
	Toy->ForceNetUpdate();
	ForceNetUpdate();
	return true;
}

bool ATBEscapeVehicle::Leave(ATBCharacter* Toy)
{
	if (!HasAuthority() || !IsValid(Toy) || !Passengers.Contains(Toy)) return false;
	const auto* Capsule = Toy->GetCapsuleComponent();
	if (!Capsule) return false;
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Directions[] = {GetActorRightVector(), -GetActorRightVector(), -GetActorForwardVector(), GetActorForwardVector()};
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TBEscapeVehicleLeave), false, Toy);
	Query.AddIgnoredActor(this);
	FVector Destination;
	bool bFoundDestination = false;
	for (const FVector& Direction : Directions)
	{
		const FVector Candidate = GetActorLocation() + Direction * (90.f + Radius) + FVector(0, 0, HalfHeight + 5.f);
		if (!GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn,
		    FCollisionShape::MakeCapsule(Radius, HalfHeight), Query))
		{
			Destination = Candidate;
			bFoundDestination = true;
			break;
		}
	}
	if (!bFoundDestination) return false;
	const bool bWasDriver = IsDriver(Toy);
	Passengers.Remove(Toy);
	Toy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Toy->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
	Toy->EscapeVehicle = nullptr;
	Toy->VehicleExitStarted = -1;
	Toy->SetActorHiddenInGame(false);
	Toy->SetActorEnableCollision(true);
	Toy->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (bWasDriver) DriveInput = FVector2D::ZeroVector;
	Toy->ForceNetUpdate();
	ForceNetUpdate();
	return true;
}

void ATBEscapeVehicle::SetDrivingInput(ATBCharacter* Toy, FVector2D Input)
{
	if (!HasAuthority() || !IsDriver(Toy) || Input.ContainsNaN()) return;
	DriveInput = Input.GetClampedToMaxSize(1.f);
	LastInputAt = GetWorld()->GetTimeSeconds();
}

void ATBEscapeVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
	const auto* Match = TB::GS(GetWorld());
	if (Match && Match->bEscapePhase && Match->Phase == ETBPhase::Playing)
	{
		TArray<AActor*> Overlapping;
		BoardingArea->GetOverlappingActors(Overlapping, ATBCharacter::StaticClass());
		for (AActor* Actor : Overlapping) Board(Cast<ATBCharacter>(Actor));
	}
	for (int32 I = Passengers.Num() - 1; I >= 0; --I)
		if (!IsValid(Passengers[I])) Passengers.RemoveAt(I);
	if (!TB::Playing(GetWorld()) || Passengers.IsEmpty() || GetWorld()->GetTimeSeconds() - LastInputAt > .3)
		DriveInput = FVector2D::ZeroVector;
	const float Dt = FMath::Min(DeltaSeconds, .1f);
	const FRotator Rotation = GetActorRotation() + FRotator(0, DriveInput.X * FMath::Max(0.f, Data().TurnSpeed) * Dt, 0);
	const FVector Delta = Rotation.Vector() * DriveInput.Y * FMath::Max(0.f, Data().DriveSpeed) * Dt;
	VehicleMovement->Advance(Delta, Rotation, Dt);
}

void ATBEscapeVehicle::ResetRound()
{
	if (!HasAuthority()) return;
	ReleasePassengers();
	SetActorTransform(StartTransform, false, nullptr, ETeleportType::TeleportPhysics);
	DriveInput = FVector2D::ZeroVector;
	LastInputAt = 0;
	VehicleMovement->ResetMotion();
	ForceNetUpdate();
}

void ATBEscapeVehicle::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority()) ReleasePassengers();
	Super::EndPlay(Reason);
}

void ATBEscapeVehicle::ReleasePassengers()
{
	while (!Passengers.IsEmpty())
	{
		if (ATBCharacter* Toy = Passengers.Last(); IsValid(Toy))
		{
			if (!Leave(Toy))
			{
				Passengers.Pop();
				Toy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				Toy->EscapeVehicle = nullptr;
				Toy->VehicleExitStarted = -1;
				Toy->SetActorHiddenInGame(false);
				Toy->SetActorEnableCollision(true);
				Toy->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
				Toy->ForceNetUpdate();
			}
		}
		else Passengers.Pop();
	}
}

void ATBEscapeVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBEscapeVehicle, Passengers);
}
