#include "GamePlay/Items/TBItemBase.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBPlayerState.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

ATBItemBase::ATBItemBase()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	InteractionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("InteractionPoint"));
	InteractionPoint->SetupAttachment(Root);
	Actions.AddDefaulted();
}

TArray<FTBItemAction> ATBItemBase::GetInteractionActions() const { return Actions; }
FVector ATBItemBase::GetInteractionLocation() const { return InteractionPoint->GetComponentLocation(); }

bool ATBItemBase::CanInteract(const FTBInteractionContext& Context) const
{
	const ATBCharacter* Character = Context.InstigatorCharacter;
	if (!bInteractionEnabled || bExecutingAction || IsActorBeingDestroyed() ||
		!IsValid(Character) || Character->IsActorBeingDestroyed() || Character->GetWorld() != GetWorld() ||
		!Character->CanAct() || Context.ActionId.IsNone()) return false;
	if (Context.SourceItem && (!IsValid(Context.SourceItem) || Context.SourceItem->IsActorBeingDestroyed() ||
		Context.SourceItem->GetWorld() != GetWorld())) return false;

	const FTBItemAction* Action = nullptr;
	for (const FTBItemAction& Candidate : Actions)
	{
		if (Candidate.Id != Context.ActionId) continue;
		if (Action) return false;
		Action = &Candidate;
	}
	if (!Action || !FMath::IsFinite(Action->MaxDistance) || Action->MaxDistance <= 0.f) return false;
	const ATBPlayerState* Player = Character->TBPS();
	if (!Player) return false;
	switch (Action->AllowedTeams)
	{
	case ETBInteractionTeams::Both:
		if (Player->Team != ETBTeam::Human && Player->Team != ETBTeam::Toy) return false;
		break;
	case ETBInteractionTeams::HumanOnly:
		if (Player->Team != ETBTeam::Human) return false;
		break;
	case ETBInteractionTeams::ToyOnly:
		if (Player->Team != ETBTeam::Toy) return false;
		break;
	default: return false;
	}
	const FVector Start = Character->GetPawnViewLocation();
	const FVector End = GetInteractionLocation();
	if (Start.ContainsNaN() || End.ContainsNaN() || FVector::Dist(Start, End) > Action->MaxDistance) return false;
	if (Action->bRequiresLineOfSight && !TB::ClearPath(Character, this, Start, End)) return false;
	return CanPerformAction(Context);
}

bool ATBItemBase::TryInteract(const FTBInteractionContext& Context)
{
	if (!HasAuthority() || !CanInteract(Context)) return false;
	TGuardValue<bool> ExecutionGuard(bExecutingAction, true);
	return PerformAction(Context);
}

bool ATBItemBase::CanPerformAction_Implementation(const FTBInteractionContext& Context) const { return true; }
bool ATBItemBase::PerformAction_Implementation(const FTBInteractionContext& Context) { return false; }

void ATBItemBase::SetInteractionEnabled(bool bEnabled)
{
	if (!HasAuthority() || bInteractionEnabled == bEnabled) return;
	bInteractionEnabled = bEnabled;
	OnRep_InteractionEnabled();
	ForceNetUpdate();
}

void ATBItemBase::OnRep_InteractionEnabled() { InteractionEnabledChanged(); }

void ATBItemBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBItemBase, bInteractionEnabled);
}
