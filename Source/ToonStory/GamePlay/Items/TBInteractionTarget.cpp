#include "GamePlay/Items/TBInteractionTarget.h"
#include "GamePlay/Items/TBInteractable.h"
#include "GamePlay/TBDummyToy.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBPlayerState.h"
#include "Data/TBRuleData.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"

FTBInteractionTarget TB::FindInteractionTarget(ATBCharacter* Character)
{
	FTBInteractionTarget Result;
	if (!IsValid(Character) || !Character->CanAct() || Character->HasCarriedToy()) return Result;
	const FVector Eye = Character->GetPawnViewLocation();
	const FVector Aim = Character->GetControlRotation().Vector();
	const auto& AimRules = TB::Rules().Aim;
	float BestAlignment = FMath::Cos(FMath::DegreesToRadians(AimRules.AimAngle)); // Aim cone shared by items and toys.
	double BestDistanceSquared = TNumericLimits<double>::Max();
	auto Consider = [&](AActor* Actor, const FVector& Location, FName ActionId)
	{
		const FVector Delta = Location - Eye;
		if (Delta.ContainsNaN()) return;
		const float Alignment = FVector::DotProduct(Aim, Delta.GetSafeNormal());
		const double DistanceSquared = Delta.SizeSquared();
		if (Alignment < BestAlignment ||
		    (FMath::IsNearlyEqual(Alignment, BestAlignment) && DistanceSquared >= BestDistanceSquared)) return;
		BestAlignment = Alignment;
		BestDistanceSquared = DistanceSquared;
		Result = {Actor, ActionId};
	};
	for (TActorIterator<AActor> It(Character->GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == Character || Actor->IsActorBeingDestroyed()) continue;
		if (auto* Item = Cast<ITBInteractable>(Actor))
		{
			FTBInteractionContext Context;
			Context.InstigatorCharacter = Character;
			for (const auto& Action : Item->GetInteractionActions())
			{
				Context.ActionId = Action.Id;
				// CanInteract owns the action's reach and optional obstruction policy; no second range cap.
				if (Item->CanInteract(Context)) Consider(Actor, Item->GetInteractionLocation(), Action.Id);
			}
		}
		else if (Character->IsHuman())
		{
			FVector Location;
			if (auto* Toy = Cast<ATBCharacter>(Actor))
			{
				const auto* Player = Toy->TBPS();
				if (!Player || Player->Team != ETBTeam::Toy || Player->ToyState != ETBToyState::Free || Toy->Carrier) continue;
				Location = Toy->GetActorLocation();
			}
			else if (auto* Dummy = Cast<ATBDummyToy>(Actor))
			{
				if (Dummy->Holder || Dummy->StoredBox) continue;
				Location = Dummy->Collision->GetComponentLocation();
			}
			else continue;
			if (FVector::DistSquared(Character->GetActorLocation(), Location) <= FMath::Square(AimRules.CaptureDistance) &&
			    TB::ClearPath(Character, Actor, Eye, Location)) Consider(Actor, Location, NAME_None);
		}
	}
	return Result;
}
