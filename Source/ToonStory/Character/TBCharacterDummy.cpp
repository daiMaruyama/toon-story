#include "TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBDummyToy.h"
#include "Engine/World.h"

void ATBCharacter::BeginDummyStorage()
{
	auto* State = TB::GS(GetWorld());
	auto* Box = State ? State->Box.Get() : nullptr;
	if (CarriedDummy && Box && Box->CanStoreFrom(this) && GetWorld()->GetTimeSeconds() >= GrabEnds)
	{
		StoreStarted = GetWorld()->GetTimeSeconds();
	}
}

bool ATBCharacter::UpdateCarriedDummy()
{
	if (!CarriedDummy || StoreStarted < 0)
	{
		return false;
	}
	auto* State = TB::GS(GetWorld());
	auto* Box = State ? State->Box.Get() : nullptr;
	if (!bHoldingInteract || !Box || !Box->CanStoreFrom(this))
	{
		ClearHeldInteraction();
		return true;
	}
	if (GetWorld()->GetTimeSeconds() - StoreStarted >= Box->StoreSeconds && Box->StoreDummy(CarriedDummy))
	{
		ClearHeldInteraction();
		ApplyState();
	}
	return true;
}
