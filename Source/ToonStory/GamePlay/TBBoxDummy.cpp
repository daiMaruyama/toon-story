#include "TBBox.h"
#include "TBDummyToy.h"
#include "Core/TBGameHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool ATBBox::StoreDummy(ATBDummyToy* Toy)
{
	if (!HasAuthority() || !IsValid(Toy) || Toy->StoredBox || !TB::Playing(GetWorld()))
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StoreDecoy), false, Toy);
	for (int Slot = 0; Slot < StorageSlotCount; ++Slot)
	{
		const FVector Position = StorageRoom->GetComponentTransform().TransformPosition(StorageSlot(Slot));
		if (GetWorld()->OverlapBlockingTestByChannel(
		        Position, FQuat::Identity, ECC_GameTraceChannel1,
		        FCollisionShape::MakeCapsule(Toy->Collision->GetScaledCapsuleRadius(),
		                                     Toy->Collision->GetScaledCapsuleHalfHeight()),
		        Params))
		{
			continue;
		}
		Toy->Place(Position, this);
		MulticastOpened();
		return true;
	}
	return false;
}

bool ATBBox::HasDummyPrisoners() const
{
	for (TActorIterator<ATBDummyToy> It(GetWorld()); It; ++It)
	{
		if (It->StoredBox == this)
		{
			return true;
		}
	}
	return false;
}

int32 ATBBox::ReleaseDummyPrisoners(int32 FirstSlot)
{
	int Released = 0;
	for (TActorIterator<ATBDummyToy> It(GetWorld()); It; ++It)
	{
		if (It->StoredBox != this)
		{
			continue;
		}
		const int Slot = FirstSlot++;
		const FVector Exit =
		    GetActorTransform().TransformPosition(FVector(-65 - (Slot / 3) * 35, -50 + (Slot % 3) * 50, 60));
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ReleaseDecoy), false, *It);
		if (GetWorld()->OverlapBlockingTestByChannel(
		        Exit, FQuat::Identity, ECC_GameTraceChannel1,
		        FCollisionShape::MakeCapsule(It->Collision->GetScaledCapsuleRadius(),
		                                     It->Collision->GetScaledCapsuleHalfHeight()),
		        Params))
		{
			continue;
		}
		It->Place(Exit);
		++Released;
	}
	return Released;
}
