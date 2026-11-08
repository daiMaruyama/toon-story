#include "GamePlay/Items/TBEscapeExit.h"
#include "GamePlay/Items/TBEscapeVehicle.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

ATBEscapeExit::ATBEscapeExit()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	ExitArea = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitArea"));
	SetRootComponent(ExitArea);
	ExitArea->SetBoxExtent(FVector(100, 100, 100));
	ExitArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExitArea->SetCollisionResponseToAllChannels(ECR_Overlap);
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(ExitArea);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Marker->SetStaticMesh(Cube.Object);
	Marker->SetRelativeScale3D(FVector(.2f, .2f, 1.f));
}

void ATBEscapeExit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	auto* Match = TB::GS(GetWorld());
	if (!Match || !Match->bEscapePhase || Match->Phase != ETBPhase::Playing || Match->Remaining() <= 0.f) return;
	for (TActorIterator<ATBEscapeVehicle> It(GetWorld()); It; ++It)
	{
		if (It->Passengers.IsEmpty() || !ExitArea->IsOverlappingActor(*It)) continue;
		for (ATBCharacter* Toy : It->Passengers)
			if (IsValid(Toy) && Toy->TBPS()) Match->EscapedPlayers.AddUnique(Toy->TBPS());
		Match->bVehicleEscapeResult = true;
		if (auto* Mode = GetWorld()->GetAuthGameMode<ATBGameMode>()) Mode->Finish(ETBWinner::Toys, TEXT("乗り物で脱出！"));
		return;
	}
}
