#include "TBPickup.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATBPickup::ATBPickup()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Contact = CreateDefaultSubobject<USphereComponent>(TEXT("Contact"));
	SetRootComponent(Contact);
	Contact->SetSphereRadius(30.f);
	Contact->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Contact->SetCollisionResponseToAllChannels(ECR_Ignore);
	Contact->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Contact->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	Contact->SetGenerateOverlapEvents(true);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Contact);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
	Mesh->SetRelativeScale3D(FVector(.35));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATBPickup::BeginPlay()
{
	Super::BeginPlay();
	if (auto* Visual = PickupMesh.LoadSynchronous())
	{
		Mesh->SetStaticMesh(Visual);
		const FBoxSphereBounds Bounds = Visual->GetBounds();
		const float Scale = 12.f / FMath::Max(1.f, float(Bounds.BoxExtent.Z * 2));
		Mesh->SetRelativeScale3D(FVector(Scale));
		Mesh->SetRelativeLocation(FVector(0, 0, -15.f - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale));
	}
}

bool ATBPickup::Touches(const ATBCharacter* ToyCharacter) const
{
	return !bTaken && ToyCharacter && Contact->IsOverlappingActor(ToyCharacter) &&
	       TB::ClearPath(ToyCharacter, this, ToyCharacter->GetPawnViewLocation(), GetActorLocation());
}

void ATBPickup::OnRep_Taken()
{
	SetActorHiddenInGame(bTaken);
	Contact->SetCollisionEnabled(bTaken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

// 接触と進捗を再確認する。取得済みフラグを先に立てて二重取得を防ぐ。
bool ATBPickup::Take(ATBCharacter* ToyCharacter)
{
	if (!HasAuthority() || bTaken || !ToyCharacter || !ToyCharacter->CanAct() || ToyCharacter->IsHuman())
	{
		return false;
	}
	if (ToyCharacter->ContactItem != this || ToyCharacter->ItemProgress < 1.f || !Touches(ToyCharacter))
	{
		return false;
	}
	auto* MatchInfo = TB::GS(GetWorld());
	if (!MatchInfo || !ToyCharacter->TBPS())
	{
		return false;
	}
	bTaken = true;
	OnRep_Taken();
	ForceNetUpdate();
	++MatchInfo->Collected;
	++ToyCharacter->TBPS()->Items;
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->CheckWin();
	}
	return true;
}

void ATBPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBPickup, bTaken);
}
