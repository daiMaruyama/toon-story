// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBToyCharacter.h"
#include "TBToyPresentation.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/TBPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

ATBToyCharacter::ATBToyCharacter(const FObjectInitializer& Init) : Super(Init)
{
	ToyVisual = CreateDefaultSubobject<UChildActorComponent>(TEXT("ToyVisual"));
	ToyVisual->SetupAttachment(GetMesh());
}

void ATBToyCharacter::ChooseRandomToy()
{
	// おもちゃ側の抽選は、サーバーで一度だけ行う。
	if (!HasAuthority() || !TBPS() || TBPS()->Team != ETBTeam::Toy || SelectedToyIndex != INDEX_NONE)
	{
		return;
	}
	TArray<int32> Candidates;
	for (int32 Index = 0; Index < ToyCatalogue.Num(); ++Index)
	{
		if (ToyCatalogue[Index])
		{
			Candidates.Add(Index);
		}
	}
	if (Candidates.IsEmpty())
	{
		return;
	}
	SelectedToyIndex = Candidates[FMath::RandHelper(Candidates.Num())];
	RefreshToyPresentation();
	ForceNetUpdate();
}

void ATBToyCharacter::RefreshToyPresentation()
{
	if (TBPS())
	{
		UpdateAppearance(TBPS()->Team == ETBTeam::Toy);
	}
}

void ATBToyCharacter::UpdateAppearance(bool bToy)
{
	if (!bToy)
	{
		if (ToyVisual->GetChildActorClass())
		{
			ToyVisual->SetChildActorClass(nullptr);
		}
		AppliedVisual.Reset();
		// 人間のCarryAnchor補正は専用サーバーでも必要。
		Super::UpdateAppearance(false);
		return;
	}
	// 専用サーバーで省略するのは、おもちゃの表示処理だけ。
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UClass* VisualClass = ToyCatalogue.IsValidIndex(SelectedToyIndex) ? ToyCatalogue[SelectedToyIndex].Get() : nullptr;
	if (ToyVisual->GetChildActorClass() != VisualClass)
	{
		ToyVisual->SetChildActorClass(VisualClass);
	}
	if (!bAppearanceInitialized || !bShowingToy)
	{
		bAppearanceInitialized = true;
		bShowingToy = true;
		Body->SetVisibility(false);
		GetMesh()->SetSkeletalMesh(nullptr);
		CarryAnchor->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform);
		GetMesh()->SetRelativeTransform(FTransform(FVector(0, 0, -13 * TBToyPresentation::SizeScale)));
		ToyVisual->SetRelativeScale3D(FVector(TBToyPresentation::SizeScale));
		CacheInitialMeshOffset(GetMesh()->GetRelativeLocation(), FRotator::ZeroRotator);
		CarryAnchor->SetRelativeLocation(FVector(95, 0, 23));
		CarryAnchor->SetRelativeRotation(FRotator::ZeroRotator);
		const float Clearance =
		    (UCharacterMovementComponent::MIN_FLOOR_DIST + UCharacterMovementComponent::MAX_FLOOR_DIST) * .5f;
		ToyVisual->SetRelativeLocation(FVector(0, 0, -Clearance));
	}
	auto* Visual = ToyVisual->GetChildActor();
	if (!Visual)
	{
		AppliedVisual.Reset();
		return;
	}
	const bool bOwnerNoSee = GetMesh()->bOwnerNoSee;
	const bool bPause = IsMovementLocked();
	// 親から毎フレーム呼ばれても、各パーツへの反映は状態が変わったときだけにする。
	if (AppliedVisual.Get() == Visual && bAppliedOwnerNoSee == bOwnerNoSee && bAppliedPause == bPause)
	{
		return;
	}
	Visual->SetOwner(this);
	if (AppliedVisual.Get() != Visual)
	{
		TBToyPresentation::Initialize(Visual);
	}
	TInlineComponentArray<UPrimitiveComponent*> Parts(Visual);
	for (auto* Part : Parts)
	{
		Part->SetOwnerNoSee(bOwnerNoSee);
		if (auto* PartMesh = Cast<USkeletalMeshComponent>(Part))
		{
			PartMesh->bPauseAnims = bPause;
		}
	}
	AppliedVisual = Visual;
	bAppliedOwnerNoSee = bOwnerNoSee;
	bAppliedPause = bPause;
}

void ATBToyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!TBPS() || TBPS()->Team != ETBTeam::Toy)
	{
		return;
	}
	TBToyPresentation::Animate(ToyVisual->GetChildActor(), GetVelocity().Size2D(), IsMovementLocked());
	if (Carrier)
	{
		TBToyPresentation::Grip(ToyVisual, Carrier->GetCarryGripLocation(), GripCache);
	}
	if (!Carrier && GetCharacterMovement()->IsMovingOnGround())
	{
		TBToyPresentation::Ground(ToyVisual, GetActorLocation().Z -
		                                         GetCapsuleComponent()->GetScaledCapsuleHalfHeight() -
		                                         GetCharacterMovement()->CurrentFloor.FloorDist);
	}
}

void ATBToyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBToyCharacter, SelectedToyIndex);
}
