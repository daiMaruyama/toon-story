#include "Combat/ToyBoxActor.h"

#include "Character/HumanCharacter.h"
#include "Character/ToyCharacter.h"
#include "Components/SceneComponent.h"
#include "Core/GazeFreezeSubsystem.h"
#include "Core/ToyBoxPlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AToyBoxActor::AToyBoxActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteriorAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("InteriorAnchor"));
	InteriorAnchor->SetupAttachment(SceneRoot);

	ExitAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ExitAnchor"));
	ExitAnchor->SetupAttachment(SceneRoot);

	bReplicates = true;
	// 進行バーがカクつかない程度には送る。
	SetNetUpdateFrequency(20.f);
}

void AToyBoxActor::BeginPlay()
{
	Super::BeginPlay();
}

void AToyBoxActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AToyBoxActor, RescueProgress);
	DOREPLIFETIME(AToyBoxActor, NumRescuers);
}

AToyBoxActor* AToyBoxActor::GetPrimaryBox(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AToyBoxActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

void AToyBoxActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	TickRescue(DeltaSeconds);
	TickHumanCamping(DeltaSeconds);
}

int32 AToyBoxActor::CountValidRescuers() const
{
	const UGazeFreezeSubsystem* Gaze = UGazeFreezeSubsystem::Get(this);
	if (!Gaze)
	{
		return 0;
	}

	const float RadiusSq = RescueRadius * RescueRadius;
	int32 Count = 0;

	for (const TWeakObjectPtr<AToyCharacter>& Entry : Gaze->GetToys())
	{
		const AToyCharacter* Toy = Entry.Get();
		if (!Toy || !Toy->bWantsToRescue)
		{
			continue;
		}

		// 箱の中にいる本人は自分では開けられない。
		const AToyBoxPlayerState* PS = Toy->GetPlayerState<AToyBoxPlayerState>();
		if (!PS || PS->ToyState != EToyState::Free)
		{
			continue;
		}

		// 凍結中に救助を続けられるかは EFreezeBlock::Rescue 次第。
		if (Toy->IsEffectivelyFrozen() && Toy->IsFreezeBlocking(EFreezeBlock::Rescue))
		{
			continue;
		}

		if (FVector::DistSquared(Toy->GetActorLocation(), GetActorLocation()) <= RadiusSq)
		{
			++Count;
		}
	}

	return Count;
}

void AToyBoxActor::TickRescue(float DeltaSeconds)
{
	const int32 N = CountValidRescuers();

	if (N == 0)
	{
		if (RescueProgress > 0.f)
		{
			// 中断したら 0 から。
			RescueProgress = 0.f;
			OnRep_RescueProgress();
			MulticastRescueAborted();
		}

		NumRescuers = 0;
		return;
	}

	if (NumRescuers == 0)
	{
		// 開始時に大きな音。人間に位置が伝わるのが救助側のリスク。
		MulticastRescueStarted();
	}

	NumRescuers = N;

	const float RequiredTime = BaseRescueTime / (1.f + RescueSpeedupPerExtra * static_cast<float>(N - 1));
	const float Rate = (RequiredTime > KINDA_SMALL_NUMBER) ? (1.f / RequiredTime) : 1.f;

	RescueProgress = FMath::Min(RescueProgress + Rate * DeltaSeconds, 1.f);
	OnRep_RescueProgress();

	if (RescueProgress >= 1.f)
	{
		ReleaseAllBoxedToys();
		RescueProgress = 0.f;
		NumRescuers = 0;
		OnRep_RescueProgress();
		MulticastRescueCompleted();
	}
}

void AToyBoxActor::TickHumanCamping(float DeltaSeconds)
{
	const UGazeFreezeSubsystem* Gaze = UGazeFreezeSubsystem::Get(this);
	if (!Gaze)
	{
		return;
	}

	const float RadiusSq = HumanCampingRadius * HumanCampingRadius;

	for (const TWeakObjectPtr<AHumanCharacter>& Entry : Gaze->GetHumans())
	{
		const AHumanCharacter* Human = Entry.Get();
		if (Human && FVector::DistSquared(Human->GetActorLocation(), GetActorLocation()) <= RadiusSq)
		{
			// 1人でも張り付いていれば加算する。何人いても倍にはしない。
			HumanCampingSeconds += DeltaSeconds;
			return;
		}
	}
}

void AToyBoxActor::StoreToy(AToyCharacter* Toy)
{
	if (!HasAuthority() || !Toy)
	{
		return;
	}

	const FVector Destination = InteriorAnchor
		? InteriorAnchor->GetComponentLocation()
		: GetActorLocation();

	Toy->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);

	// 箱の中は部屋として成立する広さなので、収容後も動けるままにしておく。
	Toy->bWantsToRescue = false;
}

void AToyBoxActor::ReleaseAllBoxedToys()
{
	if (!HasAuthority())
	{
		return;
	}

	const UGazeFreezeSubsystem* Gaze = UGazeFreezeSubsystem::Get(this);
	if (!Gaze)
	{
		return;
	}

	const FVector Destination = ExitAnchor
		? ExitAnchor->GetComponentLocation()
		: GetActorLocation();

	for (const TWeakObjectPtr<AToyCharacter>& Entry : Gaze->GetToys())
	{
		AToyCharacter* Toy = Entry.Get();
		if (!Toy)
		{
			continue;
		}

		AToyBoxPlayerState* PS = Toy->GetPlayerState<AToyBoxPlayerState>();
		if (!PS || PS->ToyState != EToyState::Boxed)
		{
			continue;
		}

		Toy->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
		PS->SetToyState(EToyState::Free);
	}
}

void AToyBoxActor::MulticastRescueStarted_Implementation()
{
	// 演出は BP 側で。ここでは通知だけを配る。
}

void AToyBoxActor::MulticastRescueAborted_Implementation()
{
}

void AToyBoxActor::MulticastRescueCompleted_Implementation()
{
}

void AToyBoxActor::OnRep_RescueProgress()
{
	OnRescueProgressChanged.Broadcast(RescueProgress);
}
