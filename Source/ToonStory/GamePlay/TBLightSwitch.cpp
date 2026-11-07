#include "TBLightSwitch.h"
#include "TBRoomLighting.h"
#include "Data/TBLightSwitchData.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

void ATBLightSwitch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBLightSwitch,Room);
}

ATBLightSwitch::ATBLightSwitch()
{
	DisplayName = NSLOCTEXT("TBItems", "LightSwitch", "Light Switch");
	Actions.Reset();
	FTBItemAction Toggle;
	Toggle.Id = TEXT("Toggle");
	Toggle.Label = NSLOCTEXT("TBItems", "ToggleLights", "Toggle lights");
	Toggle.AllowedTeams = ETBInteractionTeams::Both;
	Toggle.MaxDistance = GetDefault<UTBLightSwitchData>()->InteractionDistance;
	Actions.Add(Toggle);
	SwitchData = TSoftObjectPtr<UTBLightSwitchData>(FSoftObjectPath(TEXT("/Game/Data/DA_LightSwitch.DA_LightSwitch")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plate(TEXT("/Game/Meshes/SM_Switch.SM_Switch"));
	Mesh->SetStaticMesh(Plate.Object);
	// 実物大（幅6・高さ10cm）のプレートを、おもちゃからも見つけやすいよう4倍にする。裏を壁側に、表を部屋側（+X）に向け、
	// 前の仮の箱と同じ高さ・奥行きに収める。
	Mesh->SetRelativeLocation(FVector(-6.f, 0.f, -20.f));
	Mesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Mesh->SetRelativeScale3D(FVector(4.f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

FVector ATBLightSwitch::GetInteractionLocation() const
{
	if (!Mesh || !Mesh->GetStaticMesh()) return Super::GetInteractionLocation();
	// The plate faces actor +X. Trace to its front, not the mounting pivot:
	// a wall's simple collision can extend a few cm past its rendered surface.
	// Keep clearance in world cm even for scaled switches; walls along the rest
	// of the ray (including use from the back) still block normal item interaction.
	const FBox LocalBounds = Mesh->GetStaticMesh()->GetBoundingBox();
	const FTransform& Transform = Mesh->GetComponentTransform();
	const FVector Forward = GetActorForwardVector();
	const FVector Extent = LocalBounds.GetExtent();
	const double FrontExtent =
		FMath::Abs(FVector::DotProduct(Forward, Transform.TransformVector(FVector(Extent.X, 0, 0)))) +
		FMath::Abs(FVector::DotProduct(Forward, Transform.TransformVector(FVector(0, Extent.Y, 0)))) +
		FMath::Abs(FVector::DotProduct(Forward, Transform.TransformVector(FVector(0, 0, Extent.Z))));
	constexpr double MountingClearance = 6.0;
	return Transform.TransformPosition(LocalBounds.GetCenter()) + Forward * (FrontExtent + MountingClearance);
}

void ATBLightSwitch::ApplyData()
{
	LoadedData = SwitchData.LoadSynchronous();
	const auto& Data = LoadedData ? *LoadedData : *GetDefault<UTBLightSwitchData>();
	for (auto& Action : Actions)
	{
		if (Action.Id == TEXT("Toggle")) Action.MaxDistance = Data.InteractionDistance;
	}
}

void ATBLightSwitch::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyData();
}

void ATBLightSwitch::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Placed actors and late-joining clients must also use the selected asset before interaction.
	ApplyData();
}

bool ATBLightSwitch::CanPerformAction_Implementation(const FTBInteractionContext& Context) const
{
	return Context.ActionId == TEXT("Toggle") && IsValid(Room) && !Room->IsActorBeingDestroyed() &&
	       Room->GetWorld() == GetWorld();
}

bool ATBLightSwitch::PerformAction_Implementation(const FTBInteractionContext& Context)
{
	// TryInteract has already validated the common conditions and holds its re-entry guard.
	if (!HasAuthority() || !IsValid(Context.InstigatorCharacter) || !CanPerformAction(Context)) return false;
	ATBCharacter* Character = Context.InstigatorCharacter;
	Character->ClearHeldInteraction();
	Room->SetLightsOn(!Room->bLightsOn);
	TB::Notice(Character, Room->bLightsOn ? TEXT("Lights on") : TEXT("Lights off"));
	return true;
}
