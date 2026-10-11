#include "TBToyCharacter.h"
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

void ATBToyCharacter::BeginPlay()
{
	if (!ToyCatalogue.IsEmpty())
	{
		ToyMesh.Reset();
		ToySkeletalMesh.Reset();
		ToyIdleAnimation.Reset();
		ToyWalkAnimation.Reset();
		ToyJumpAnimation.Reset();
	}
	Super::BeginPlay();
	RefreshToyPresentation();
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
	if (!TBPS())
	{
		return; // 選択番号がPlayerStateより先に届く場合は、次の更新を待つ。
	}
	const bool bToy =
	    TBPS()->Team == ETBTeam::Toy && ToyCatalogue.IsValidIndex(SelectedToyIndex) && ToyCatalogue[SelectedToyIndex];
	UClass* VisualClass = bToy ? ToyCatalogue[SelectedToyIndex].Get() : nullptr;
	if (ToyVisual->GetChildActorClass() != VisualClass)
	{
		ToyVisual->SetChildActorClass(VisualClass);
	}
	if (bToy && !bShowingToy)
	{
		ApplyState();
		// 元のMeshは表示せず、通信時の移動補間と持ち運びの取り付け基準として残す。
		Body->SetVisibility(false);
		GetMesh()->SetSkeletalMesh(nullptr);
		GetMesh()->SetRelativeTransform(FTransform(FVector(0, 0, -13)));
		CacheInitialMeshOffset(GetMesh()->GetRelativeLocation(), FRotator::ZeroRotator);
		CarryAnchor->SetRelativeLocation(FVector(95, 0, 23));
		CarryAnchor->SetRelativeRotation(FRotator::ZeroRotator);
	}
	bShowingToy = bToy;
	if (auto* Visual = ToyVisual->GetChildActor())
	{
		Visual->SetOwner(this); // 一人称の表示判定と、ABPからの移動速度取得に使う。
		const auto* Movement = GetCharacterMovement();
		const float Clearance =
		    Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor()
		        ? FMath::Clamp(Movement->CurrentFloor.FloorDist, 0.f, UCharacterMovementComponent::MAX_FLOOR_DIST)
		        : (UCharacterMovementComponent::MIN_FLOOR_DIST + UCharacterMovementComponent::MAX_FLOOR_DIST) * .5f;
		ToyVisual->SetRelativeLocation(FVector(0, 0, -Clearance));
		TInlineComponentArray<UPrimitiveComponent*> Parts(Visual);
		for (auto* Part : Parts)
		{
			Part->SetOwnerNoSee(GetMesh()->bOwnerNoSee);
			if (auto* PartMesh = Cast<USkeletalMeshComponent>(Part))
			{
				PartMesh->bPauseAnims = IsMovementLocked();
			}
		}
	}
}

void ATBToyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshToyPresentation();
}

void ATBToyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBToyCharacter, SelectedToyIndex);
}
