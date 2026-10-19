#include "TBRoomLighting.h"
#include "TBRuleMath.h"
#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ATBRoomLighting::ATBRoomLighting()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("RoomBounds"));
	SetRootComponent(Bounds);
	Bounds->SetBoxExtent(FVector(400, 400, 200));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);
}

void ATBRoomLighting::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) bLightsOn = bInitiallyLightsOn;
	OnRep_LightsOn();
}

void ATBRoomLighting::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBRoomLighting, bLightsOn);
}

void ATBRoomLighting::SetLightsOn(bool bOn)
{
	if (!HasAuthority()) return;
	bLightsOn = bOn;
	OnRep_LightsOn();
	ForceNetUpdate();
}

void ATBRoomLighting::ResetLighting()
{
	SetLightsOn(bInitiallyLightsOn);
}

void ATBRoomLighting::OnRep_LightsOn()
{
	for (ALight* Light : Lights)
	{
		if (IsValid(Light) && Light->GetLightComponent())
		{
			Light->GetLightComponent()->SetVisibility(bLightsOn);
		}
	}
}

bool ATBRoomLighting::Contains(const FVector& Position) const
{
	const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(Position);
	const FVector Extent = Bounds->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

bool ATBRoomLighting::ProtectsFromGaze(const UWorld* World, const FVector& ToyPosition, const FVector& HumanPosition)
{
	if (!World) return false;
	bool bInDarkRoom = false;
	const double DistanceSquared = FVector::DistSquared(ToyPosition, HumanPosition);
	for (TActorIterator<ATBRoomLighting> It(World); It; ++It)
	{
		if (!It->Contains(ToyPosition)) continue;
		if (!TBRuleMath::DarknessProtects(It->bLightsOn, DistanceSquared, It->DarkProtectionDistanceMeters)) return false;
		bInDarkRoom = true;
	}
	return bInDarkRoom;
}
