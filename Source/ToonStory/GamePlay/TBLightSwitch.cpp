#include "TBLightSwitch.h"
#include "TBRoomLighting.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ATBLightSwitch::ATBLightSwitch()
{
	DisplayName = NSLOCTEXT("TBItems", "LightSwitch", "Light Switch");
	Actions.Reset();
	FTBItemAction Toggle;
	Toggle.Id = TEXT("Toggle");
	Toggle.Label = NSLOCTEXT("TBItems", "ToggleLights", "Toggle lights");
	Toggle.AllowedTeams = ETBInteractionTeams::Both;
	Toggle.MaxDistance = 180.f;
	Actions.Add(Toggle);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetRelativeScale3D(FVector(.12f, .3f, .4f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
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
