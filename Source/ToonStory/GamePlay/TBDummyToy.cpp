#include "TBDummyToy.h"
#include "Audio/TBAudioEvents.h"
#include "Core/TBPlayerState.h"
#include "TimerManager.h"
#include "Character/TBCharacter.h"
#include "Character/TBToyPresentation.h"
#include "Character/TBToyCharacter.h"
#include "GameFramework/GameModeBase.h"
#include "Components/ChildActorComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "GamePlay/TBBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Core/TBGameHelpers.h"

ATBDummyToy::ATBDummyToy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	Root = CreateDefaultSubobject<USphereComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->InitSphereRadius(12);
	Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("ToyCollision"));
	Collision->SetupAttachment(Root);
	Collision->InitCapsuleSize(10.f * TBToyPresentation::SizeScale, 13.f * TBToyPresentation::SizeScale);
	Collision->SetRelativeLocation(FVector(0, 0, 13.f * TBToyPresentation::SizeScale - 13.f));
	Collision->SetCollisionObjectType(ECC_GameTraceChannel1);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	Collision->SetCanEverAffectNavigation(false);
	Visual = CreateDefaultSubobject<UChildActorComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Visual->SetRelativeLocation(FVector(0, 0, -13));
}
void ATBDummyToy::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	ChooseRandomToy();
	RefreshVisual();
}
void ATBDummyToy::ChooseRandomToy()
{
	if (!HasAuthority()) return;
	const auto* Mode = GetWorld()->GetAuthGameMode();
	const auto* Defaults = Mode && Mode->DefaultPawnClass
	    ? Cast<ATBToyCharacter>(Mode->DefaultPawnClass->GetDefaultObject()) : nullptr;
	if (!Defaults) return;
	const int32 Index = TBToyPresentation::SelectToy(Defaults->ToyCatalogue);
	if (Index == INDEX_NONE) return;
	VisualClass = Defaults->ToyCatalogue[Index];
}

void ATBDummyToy::RefreshVisual()
{
	Visual->SetRelativeScale3D(FVector(TBToyPresentation::SizeScale));
	Visual->SetChildActorClass(VisualClass);
	if (auto* Child = Visual->GetChildActor())
	{
		Child->SetActorTickEnabled(false);
	}
	TBToyPresentation::Initialize(Visual->GetChildActor());
	TBToyPresentation::Animate(Visual->GetChildActor(), 0, true);
	SettleFrames = 12;
	SetActorTickEnabled(true);
}
void ATBDummyToy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && bFalling && !Holder)
	{
		Fall(DeltaSeconds);
	}
	if (HasAuthority() && Holder && !IsValid(Holder))
	{
		ResetPlacement();
	}
	if (!Holder)
	{
		TBToyPresentation::Ground(Visual, GetActorLocation().Z - 13.f);
	}
	else
	{
		TBToyPresentation::Grip(Visual, Holder->GetCarryGripLocation(), GripCache);
	}
	if (SettleFrames > 0 && --SettleFrames == 0)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual->GetChildActor());
		for (auto* Part : Parts)
		{
			Part->bPauseAnims = true;
			Part->SetComponentTickEnabled(false);
		}
		if (!Holder && !bFalling)
		{
			SetActorTickEnabled(false);
		}
	}
}
void ATBDummyToy::OnRep_Holder()
{
	Collision->SetCollisionEnabled(Holder ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	SettleFrames = 12;
	SetActorTickEnabled(true);
	if (Holder)
	{
		AttachToComponent(Holder->CarryAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(FVector(0, 0, -10));
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}
bool ATBDummyToy::PickUp(ATBCharacter* Human)
{
	if (!HasAuthority() || Holder || StoredBox || !Human || !Human->IsHuman() || !Human->CanAct() ||
	    Human->CarriedToy || Human->CarriedDummy)
	{
		return false;
	}
	Holder = Human;
	StoredBox = nullptr;
	bFalling = false;
	Human->GrabEnds = GetWorld()->GetTimeSeconds() + .25;
	Human->CarriedDummy = this;
	FTimerHandle LiftTimer;
	const double LiftAt = Human->GrabEnds;
	GetWorldTimerManager().SetTimer(LiftTimer, FTimerDelegate::CreateWeakLambda(this, [this, LiftAt]()
	{
		if (IsValid(Holder) && Holder->GrabEnds == LiftAt)
		{
			UTBAudioEvents::Emit(this, ETBSoundEvent::CarryLift, Holder->TBPS(), FVector::ZeroVector);
		}
	}), .25f, false);
	OnRep_Holder();
	Human->ApplyState();
	UTBAudioEvents::Emit(this, ETBSoundEvent::Capture, nullptr, GetActorLocation());
	Human->ForceNetUpdate();
	ForceNetUpdate();
	return true;
}
void ATBDummyToy::PutDown()
{
	if (!HasAuthority() || !Holder)
	{
		return;
	}
	ATBCharacter* Human = Holder;
	UTBAudioEvents::Emit(this, ETBSoundEvent::CarryDrop, Human->TBPS(), FVector::ZeroVector);
	FVector Position = Human->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DummyDrop), false, Human);
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	const FVector Desired = Position + FRotator(0, Human->GetControlRotation().Yaw, 0).Vector() * 100.f;
	if (!GetWorld()->SweepSingleByChannel(Hit, Position, Desired, FQuat::Identity, ECC_GameTraceChannel1,
	                                      FCollisionShape::MakeCapsule(Collision->GetUnscaledCapsuleRadius(),
	                                                                   Collision->GetUnscaledCapsuleHalfHeight()),
	                                      Params))
	{
		Position = Desired;
	}
	Place(Position);
	SetActorRotation(FRotator(0, Human->GetActorRotation().Yaw, 0));
}
void ATBDummyToy::ResetPlacement()
{
	if (!HasAuthority())
	{
		return;
	}
	if (IsValid(Holder))
	{
		Holder->CarriedDummy = nullptr;
	}
	Holder = nullptr;
	StoredBox = nullptr;
	bFalling = false;
	FallSpeed = 0;
	OnRep_Holder();
	SetActorTransform(InitialTransform, false, nullptr, ETeleportType::TeleportPhysics);
	ChooseRandomToy();
	RefreshVisual();
	ForceNetUpdate();
}
void ATBDummyToy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBDummyToy, Holder);
	DOREPLIFETIME(ATBDummyToy, VisualClass);
	DOREPLIFETIME(ATBDummyToy, StoredBox);
	DOREPLIFETIME(ATBDummyToy, bFalling);
}

bool ATBDummyToy::TryPickUpNearest(ATBCharacter* Human, float ClosestDistance)
{
	ATBDummyToy* ClosestDummy = nullptr;
	for (TActorIterator<ATBDummyToy> It(Human->GetWorld()); It; ++It)
	{
		const FVector Target = It->Collision->GetComponentLocation();
		const FVector Delta = Target - Human->GetActorLocation();
		if (!It->Holder && !It->StoredBox && Delta.Size() < ClosestDistance &&
		    FVector::DotProduct(Human->GetControlRotation().Vector(), Delta.GetSafeNormal()) >= .35f &&
		    TB::ClearPath(Human, *It, Human->GetPawnViewLocation(), Target))
		{
			ClosestDummy = *It;
			ClosestDistance = Delta.Size();
		}
	}
	if (!ClosestDummy)
	{
		return false;
	}
	ClosestDummy->PickUp(Human);
	return true;
}
