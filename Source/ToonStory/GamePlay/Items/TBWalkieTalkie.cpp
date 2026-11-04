#include "GamePlay/Items/TBWalkieTalkie.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Data/TBWalkieTalkieData.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATBWalkieTalkie::ATBWalkieTalkie()
{
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;
	DisplayName = NSLOCTEXT("TBItems", "Walkie", "トランシーバー");
	Actions[0].Label = NSLOCTEXT("TBItems", "WalkieTalk", "長押しで送信");
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Model(TEXT("/Game/Items/WalkieTalkie/SM_WalkieTalkie.SM_WalkieTalkie"));
	Mesh->SetStaticMesh(Model.Object);
	InteractionPoint->SetRelativeLocation(FVector(0, 0, 11));
	// Retain the legacy subobject name for serialized instances; the imported model includes its antenna.
	auto* Antenna = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Antenna"));
	Antenna->SetupAttachment(Root);
	Antenna->SetVisibility(false);
	Antenna->SetHiddenInGame(true);
	Antenna->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WalkieData = TSoftObjectPtr<UTBWalkieTalkieData>(FSoftObjectPath(TEXT("/Game/Data/DA_WalkieTalkie.DA_WalkieTalkie")));
}

void ATBWalkieTalkie::BeginPlay()
{
	Super::BeginPlay();
	LoadedData = WalkieData.LoadSynchronous();
	Actions[0].MaxDistance = FMath::Max(1.f, Data().InteractionDistance);
}
const UTBWalkieTalkieData& ATBWalkieTalkie::Data() const { return LoadedData ? *LoadedData : *GetDefault<UTBWalkieTalkieData>(); }
bool ATBWalkieTalkie::PairWith(ATBWalkieTalkie* Other)
{
	if (!HasAuthority() || !IsValid(Other) || Other == this || Other->GetWorld() != GetWorld() || Partner || Other->Partner) return false;
	Partner = Other; Other->Partner = this;
	ForceNetUpdate(); Other->ForceNetUpdate();
	return true;
}
bool ATBWalkieTalkie::CanPerformAction_Implementation(const FTBInteractionContext& Context) const
{
	return TB::Playing(GetWorld()) && IsValid(Partner) && !Partner->IsActorBeingDestroyed() && !Transmitter && Context.InstigatorCharacter;
}
bool ATBWalkieTalkie::PerformAction_Implementation(const FTBInteractionContext& Context)
{
	Transmitter = Context.InstigatorCharacter;
	ForceNetUpdate();
	return true;
}
void ATBWalkieTalkie::StopTransmitting(ATBCharacter* Character)
{
	if (HasAuthority() && Transmitter == Character) { Transmitter = nullptr; ForceNetUpdate(); }
}
void ATBWalkieTalkie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && Transmitter && (!IsValid(Partner) || !IsValid(Transmitter) || !Transmitter->CanAct() || !TB::Playing(GetWorld()) ||
	    FVector::DistSquared(Transmitter->GetPawnViewLocation(), GetInteractionLocation()) > FMath::Square(Actions[0].MaxDistance)))
	{
		Transmitter = nullptr; ForceNetUpdate();
	}
}
void ATBWalkieTalkie::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority() && IsValid(Partner)) { Partner->Partner = nullptr; Partner->Transmitter = nullptr; Partner->ForceNetUpdate(); }
	Super::EndPlay(Reason);
}
void ATBWalkieTalkie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBWalkieTalkie, Partner);
	DOREPLIFETIME(ATBWalkieTalkie, Transmitter);
}
