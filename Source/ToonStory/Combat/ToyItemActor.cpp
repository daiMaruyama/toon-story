#include "Combat/ToyItemActor.h"

#include "Character/ToyCharacter.h"
#include "Combat/ToyBoxActor.h"
#include "Components/SceneComponent.h"
#include "Core/GazeFreezeSubsystem.h"
#include "Core/ToyBoxGameMode.h"
#include "Core/ToyBoxGameState.h"
#include "Core/ToyBoxPlayerController.h"
#include "Core/ToyBoxPlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AToyItemActor::AToyItemActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	bReplicates = true;

	// マップに多数置かれる想定。取得されるまでは何も変わらないので休ませておき、
	// 取得時に FlushNetDormancy() で起こす（仕様書「8. アイテムの実装メモ」）。
	NetDormancy = DORM_Initial;
}

void AToyItemActor::BeginPlay()
{
	Super::BeginPlay();
}

void AToyItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AToyItemActor, bCollected);
	DOREPLIFETIME(AToyItemActor, CollectProgress);
}

void AToyItemActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || bCollected)
	{
		return;
	}

	TickCollect(DeltaSeconds);
}

float AToyItemActor::GetEffectiveCollectDuration() const
{
	float CampingSeconds = 0.f;
	if (const AToyBoxActor* Box = AToyBoxActor::GetPrimaryBox(this))
	{
		CampingSeconds = Box->HumanCampingSeconds;
	}

	const float Duration = BaseCollectDuration / (1.f + CampingBonusPerSecond * CampingSeconds);
	return FMath::Max(Duration, MinCollectDuration);
}

AToyCharacter* AToyItemActor::FindCollector() const
{
	const UGazeFreezeSubsystem* Gaze = UGazeFreezeSubsystem::Get(this);
	if (!Gaze)
	{
		return nullptr;
	}

	const float RadiusSq = CollectRadius * CollectRadius;

	for (const TWeakObjectPtr<AToyCharacter>& Entry : Gaze->GetToys())
	{
		AToyCharacter* Toy = Entry.Get();
		if (!Toy || !Toy->bWantsToCollect)
		{
			continue;
		}

		const AToyBoxPlayerState* PS = Toy->GetPlayerState<AToyBoxPlayerState>();
		if (!PS || PS->ToyState != EToyState::Free)
		{
			continue;
		}

		if (Toy->IsEffectivelyFrozen() && Toy->IsFreezeBlocking(EFreezeBlock::Interact))
		{
			continue;
		}

		if (FVector::DistSquared(Toy->GetActorLocation(), GetActorLocation()) <= RadiusSq)
		{
			return Toy;
		}
	}

	return nullptr;
}

void AToyItemActor::TickCollect(float DeltaSeconds)
{
	AToyCharacter* Collector = FindCollector();

	if (!Collector)
	{
		if (CollectProgress > 0.f)
		{
			CollectProgress = 0.f;
			CurrentCollector.Reset();
			FlushNetDormancy();
			OnRep_CollectProgress();
		}
		return;
	}

	// 途中で取得者が変わったら、進行度は引き継がせない。
	if (CurrentCollector.Get() != Collector)
	{
		CurrentCollector = Collector;
		CollectProgress = 0.f;
	}

	CollectProgress = FMath::Min(CollectProgress + DeltaSeconds / GetEffectiveCollectDuration(), 1.f);
	FlushNetDormancy();
	OnRep_CollectProgress();

	if (CollectProgress >= 1.f)
	{
		CompleteCollect(Collector);
	}
}

void AToyItemActor::CompleteCollect(AToyCharacter* Collector)
{
	// 先着 1 人だけ。すでに誰かが取っていたら、この場で失敗を返す。
	if (bCollected)
	{
		if (Collector)
		{
			if (AToyBoxPlayerController* PC = Cast<AToyBoxPlayerController>(Collector->GetController()))
			{
				PC->ClientItemPickupFailed();
			}
		}
		return;
	}

	bCollected = true;
	CollectProgress = 0.f;
	CurrentCollector.Reset();

	// 休眠していたので、取得を確実に全員へ届けるために起こす。
	FlushNetDormancy();
	OnRep_Collected();

	SetActorTickEnabled(false);

	const UWorld* World = GetWorld();
	AToyBoxGameState* GS = World ? World->GetGameState<AToyBoxGameState>() : nullptr;
	AToyBoxGameMode* GM = World ? World->GetAuthGameMode<AToyBoxGameMode>() : nullptr;

	if (GS && GM)
	{
		GM->NotifyItemCollected(GS->CollectedItems + 1);
	}
}

void AToyItemActor::OnRep_Collected()
{
	// 取得済みのアイテムは見えても触れてもいけない。
	SetActorHiddenInGame(bCollected);
	SetActorEnableCollision(!bCollected);

	OnCollectProgressChanged.Broadcast(0.f);
}

void AToyItemActor::OnRep_CollectProgress()
{
	OnCollectProgressChanged.Broadcast(CollectProgress);
}
