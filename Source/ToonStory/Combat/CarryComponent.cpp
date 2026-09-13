#include "Combat/CarryComponent.h"

#include "Character/HumanCharacter.h"
#include "Character/ToyCharacter.h"
#include "Combat/ToyBoxActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/ToyBoxPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UCarryComponent::UCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCarryComponent, CarriedToy);
	DOREPLIFETIME(UCarryComponent, StoreProgress);
	DOREPLIFETIME(UCarryComponent, bStoring);
}

AHumanCharacter* UCarryComponent::GetHumanOwner() const
{
	return Cast<AHumanCharacter>(GetOwner());
}

void UCarryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 運んでいた相手が消えた（切断など）場合の後始末。
	if (bStoring && !CarriedToy)
	{
		ReleaseCarried();
		return;
	}

	if (bStoring)
	{
		TickStore(DeltaTime);
	}
}

bool UCarryComponent::BeginGrab(AToyCharacter* Target)
{
	AHumanCharacter* Human = GetHumanOwner();
	if (!Human || !Human->HasAuthority() || !Target)
	{
		return false;
	}

	// すでに運搬中なら受け付けない。
	if (CarriedToy)
	{
		return false;
	}

	AToyBoxPlayerState* PS = Target->GetPlayerState<AToyBoxPlayerState>();
	if (!PS || PS->ToyState != EToyState::Free)
	{
		return false;
	}

	if (!Human->CanReachToy(Target))
	{
		return false;
	}

	PS->SetToyState(EToyState::Grabbed);

	// アタッチ前に移動を止める。止めずに付けると、相手側で位置が暴れる。
	if (UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	USceneComponent* AttachTo = Human->GetMesh() ? static_cast<USceneComponent*>(Human->GetMesh()) : Human->GetRootComponent();
	Target->AttachToComponent(
		AttachTo,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		CarrySocketName);

	CarriedToy = Target;
	PS->SetToyState(EToyState::Carried);

	OnRep_CarriedToy();
	RefreshCarrierSpeed();

	return true;
}

bool UCarryComponent::BeginStore()
{
	const AHumanCharacter* Human = GetHumanOwner();
	if (!Human || !Human->HasAuthority() || !CarriedToy || bStoring)
	{
		return false;
	}

	const AToyBoxActor* Box = AToyBoxActor::GetPrimaryBox(this);
	if (!Box)
	{
		return false;
	}

	if (FVector::Dist(Human->GetActorLocation(), Box->GetActorLocation()) > StoreRange)
	{
		return false;
	}

	AToyBoxPlayerState* PS = CarriedToy->GetPlayerState<AToyBoxPlayerState>();
	if (!PS || PS->ToyState != EToyState::Carried)
	{
		return false;
	}

	bStoring = true;
	StoreProgress = 0.f;
	PS->SetToyState(EToyState::Storing);
	OnRep_StoreProgress();

	return true;
}

void UCarryComponent::TickStore(float DeltaTime)
{
	StoreProgress = FMath::Min(StoreProgress + DeltaTime / StoreDuration, 1.f);
	OnRep_StoreProgress();

	if (StoreProgress < 1.f)
	{
		return;
	}

	AToyCharacter* Toy = CarriedToy;
	AToyBoxPlayerState* PS = Toy ? Toy->GetPlayerState<AToyBoxPlayerState>() : nullptr;

	// 先に運搬状態を解いてから Boxed にする。
	// Boxed の通知で GameMode が箱へ移すので、アタッチが残っていると付いて回る。
	bStoring = false;
	StoreProgress = 0.f;
	CarriedToy = nullptr;
	OnRep_CarriedToy();
	OnRep_StoreProgress();
	RefreshCarrierSpeed();

	if (Toy)
	{
		Toy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		if (UCharacterMovementComponent* Movement = Toy->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	if (PS)
	{
		PS->SetToyState(EToyState::Boxed);
	}
}

void UCarryComponent::ReleaseCarried()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	AToyCharacter* Toy = CarriedToy;

	bStoring = false;
	StoreProgress = 0.f;
	CarriedToy = nullptr;
	OnRep_CarriedToy();
	OnRep_StoreProgress();
	RefreshCarrierSpeed();

	if (!Toy)
	{
		return;
	}

	Toy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// デタッチのあと、サーバー側で明示的に位置を決めてから移動を戻す。
	const FVector DropLocation = Toy->GetActorLocation();
	Toy->SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* Movement = Toy->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	// 中断されたおもちゃはその場に落ちて自由に戻る。
	if (AToyBoxPlayerState* PS = Toy->GetPlayerState<AToyBoxPlayerState>())
	{
		if (PS->ToyState == EToyState::Grabbed
			|| PS->ToyState == EToyState::Carried
			|| PS->ToyState == EToyState::Storing)
		{
			PS->SetToyState(EToyState::Free);
		}
	}
}

void UCarryComponent::RefreshCarrierSpeed()
{
	AHumanCharacter* Human = GetHumanOwner();
	UCharacterMovementComponent* Movement = Human ? Human->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	if (!bCachedDefaultSpeed)
	{
		DefaultMaxWalkSpeed = Movement->MaxWalkSpeed;
		bCachedDefaultSpeed = true;
	}

	Movement->MaxWalkSpeed = IsCarrying()
		? DefaultMaxWalkSpeed * CarryingSpeedScale
		: DefaultMaxWalkSpeed;
}

void UCarryComponent::OnRep_CarriedToy()
{
	// クライアントでも速度表示などが揃うように反映する。
	RefreshCarrierSpeed();
}

void UCarryComponent::OnRep_StoreProgress()
{
	OnStoreProgressChanged.Broadcast(StoreProgress);
}
