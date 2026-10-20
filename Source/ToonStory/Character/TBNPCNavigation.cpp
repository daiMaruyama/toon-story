// Copyright (c) 2026 Mamodou. All Rights Reserved.
#include "TBNPCNavigation.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GamePlay/TBPickup.h"
#include "GamePlay/TBBox.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool TBNPCNavigation::Prepare(UWorld* World)
{
	auto* Nav = UNavigationSystemV1::GetCurrent(World);
	if (!Nav) return false;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
		if (It->ActorHasTag(TEXT("TBNPCBounds"))) return true;
	// Bound navigation around gameplay locations, excluding distant scenery.
	FBox Area(ForceInit);
	for (TActorIterator<APlayerStart> It(World); It; ++It) Area += It->GetActorLocation();
	for (TActorIterator<ATBPickup> It(World); It; ++It) Area += It->GetActorLocation();
	for (TActorIterator<ATBBox> It(World); It; ++It) Area += It->GetActorLocation();
	if (!Area.IsValid) return false;
	Area = Area.ExpandBy(FVector(1000, 1000, 300));
	auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
	if (!Bounds) return false;
	// A box component supplies bounds without authoring/modifying the map's brush asset.
	auto* Shape = NewObject<UBoxComponent>(Bounds);
	Bounds->AddInstanceComponent(Shape);
	Shape->SetupAttachment(Bounds->GetRootComponent());
	Shape->SetBoxExtent(Area.GetExtent());
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shape->SetCanEverAffectNavigation(false);
	Shape->RegisterComponent();
	Shape->SetWorldLocation(Area.GetCenter());
	Bounds->Tags.Add(TEXT("TBNPCBounds"));
	Nav->OnNavigationBoundsUpdated(Bounds);
	return true;
}
