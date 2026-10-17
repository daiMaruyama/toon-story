#include "Audio/TBCharacterAudioComponent.h"
#include "Audio/TBAudioSubsystem.h"
#include "Audio/TBAudioTypes.h"
#include "Character/TBCharacter.h"
#include "GamePlay/TBPickup.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBPlayerState.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UTBCharacterAudioComponent::UTBCharacterAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UTBCharacterAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<ATBCharacter>(GetOwner());
	if (GetNetMode() == NM_DedicatedServer || !Character.IsValid())
	{
		SetComponentTickEnabled(false);
		return;
	}
	AddTickPrerequisiteActor(Character.Get());
	AddTickPrerequisiteComponent(Character->GetCharacterMovement());
}

void UTBCharacterAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const auto* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	StopStoreLoop(GameInstance ? GameInstance->GetSubsystem<UTBAudioSubsystem>() : nullptr);
	StopCollectLoop(GameInstance ? GameInstance->GetSubsystem<UTBAudioSubsystem>() : nullptr);
	Super::EndPlay(EndPlayReason);
}

void UTBCharacterAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ATBCharacter* OwnerCharacter = Character.Get();
	const auto* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	auto* Audio = GameInstance ? GameInstance->GetSubsystem<UTBAudioSubsystem>() : nullptr;
	if (!OwnerCharacter || !Audio)
	{
		ResetMovement();
		StopStoreLoop(Audio);
		StopCollectLoop(Audio);
		return;
	}
	UpdateStoreLoop(OwnerCharacter, Audio);
	UpdateCollectLoop(OwnerCharacter, Audio);
	UpdateMovement(OwnerCharacter, Audio, DeltaTime);
}

void UTBCharacterAudioComponent::ResetMovement()
{
	bHasMovementSample = false;
	bObservedAirborneDeparture = false;
	StepDistance = 0.f;
}

void UTBCharacterAudioComponent::UpdateMovement(ATBCharacter* OwnerCharacter, UTBAudioSubsystem* Audio,
	                                           float DeltaTime)
{
	const auto* Player = OwnerCharacter->TBPS();
	const auto* Movement = OwnerCharacter->GetCharacterMovement();
	if (!TB::Playing(GetWorld()) || !Player || Player->Team == ETBTeam::None || !Movement ||
	    OwnerCharacter->IsMovementLocked() || DeltaTime <= 0.f || DeltaTime > .25f)
	{
		ResetMovement();
		return;
	}

	const FVector Location = OwnerCharacter->GetActorLocation();
	const bool bGrounded = Movement->IsMovingOnGround();
	const bool bFalling = Movement->IsFalling();
	const float Distance = FVector::Dist2D(Location, PreviousLocation);
	// Ignore teleports, spawn placement, possession and round/team changes. Corrections may move
	// simulated proxies in larger steps than their local velocity, so allow a small network margin.
	const float MaxContinuousDistance = FMath::Max(100.f, Movement->GetMaxSpeed() * DeltaTime * 3.f);
	const bool bContinuousSample = bHasMovementSample && Player->Team == PreviousTeam &&
	                               Player->ToyState == PreviousToyState &&
	                               FVector::Dist(Location, PreviousLocation) <= MaxContinuousDistance;
	if (bContinuousSample)
	{
		const FVector Feet = Location - FVector(0.f, 0.f, OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (bWasGrounded && bFalling)
		{
			bObservedAirborneDeparture = true;
			StepDistance = 0.f;
			// Walking off an edge should have a landing sound, but no jump sound.
			if (Movement->Velocity.Z > 50.f && Player->Team == ETBTeam::Toy)
			{
				Audio->PlaySoundEvent(ETBSoundEvent::Jump, Feet);
			}
		}
		if (bWasFalling && bGrounded && bObservedAirborneDeparture)
		{
			Audio->PlaySoundEvent(Player->Team == ETBTeam::Human ? ETBSoundEvent::HumanLand : ETBSoundEvent::Land, Feet);
			bObservedAirborneDeparture = false;
			StepDistance = 0.f;
		}
		if (bAutomaticFootsteps && bWasGrounded && bGrounded && Movement->Velocity.SizeSquared2D() > 100.f)
		{
			StepDistance += Distance;
			const float Interval = FMath::Max(1.f, Player->Team == ETBTeam::Toy ? ToyStepDistance : HumanStepDistance);
			if (StepDistance >= Interval)
			{
				// At most one sound per frame, even after a delayed movement update.
				StepDistance = FMath::Fmod(StepDistance, Interval);
				Audio->PlaySoundEvent(Player->Team == ETBTeam::Human ? ETBSoundEvent::HumanFootstep : ETBSoundEvent::Footstep, Feet);
			}
		}
		else
		{
			StepDistance = 0.f;
		}
	}
	else
	{
		StepDistance = 0.f;
		bObservedAirborneDeparture = false;
	}
	PreviousLocation = Location;
	PreviousTeam = Player->Team;
	PreviousToyState = Player->ToyState;
	bWasGrounded = bGrounded;
	bWasFalling = bFalling;
	bHasMovementSample = true;
}

void UTBCharacterAudioComponent::UpdateStoreLoop(ATBCharacter* OwnerCharacter, UTBAudioSubsystem* Audio)
{
	// StoreStarted is owner-only replicated; keep the listen host and remote clients consistent.
	const auto* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const auto* PrimaryController = GameInstance ? GameInstance->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	const bool bDesired = TB::Playing(GetWorld()) && PrimaryController &&
	                      OwnerCharacter->GetController() == PrimaryController &&
	                      OwnerCharacter->IsLocallyControlled() &&
	                      OwnerCharacter->CanAct() && OwnerCharacter->StoreStarted >= 0.0 && OwnerCharacter->HasCarriedToy();
	const uint32 Revision = Audio->GetSettingsRevision();
	if (bDesired == bStoreLoopDesired && Revision == LastSettingsRevision)
	{
		return;
	}
	StopStoreLoop(Audio);
	bStoreLoopDesired = bDesired;
	LastSettingsRevision = Revision;
	if (bDesired)
	{
		StoreAudio = Audio->StartLoop(ETBSoundEvent::StoreLoop, OwnerCharacter->GetRootComponent());
	}
}

void UTBCharacterAudioComponent::StopStoreLoop(UTBAudioSubsystem* Audio)
{
	if (UAudioComponent* PlayingAudio = StoreAudio.Get())
	{
		if (Audio)
		{
			Audio->StopLoop(PlayingAudio);
		}
		else
		{
			PlayingAudio->Stop();
		}
	}
	StoreAudio.Reset();
	bStoreLoopDesired = false;
}

void UTBCharacterAudioComponent::UpdateCollectLoop(ATBCharacter* OwnerCharacter, UTBAudioSubsystem* Audio)
{
    const auto* Primary = GetWorld()->GetGameInstance()->GetFirstLocalPlayerController(GetWorld());
    const bool bDesired = OwnerCharacter->GetController() == Primary && OwnerCharacter->IsLocallyControlled() &&
        OwnerCharacter->CanAct() && IsValid(OwnerCharacter->ContactItem) && OwnerCharacter->ItemProgress > 0.f;
    const uint32 Revision = Audio->GetSettingsRevision();
    if (bDesired == bCollectLoopDesired && Revision == CollectSettingsRevision) return;
    StopCollectLoop(Audio);
    bCollectLoopDesired = bDesired;
    CollectSettingsRevision = Revision;
    if (bDesired) CollectAudio = Audio->StartLoop(ETBSoundEvent::CollectLoop, OwnerCharacter->GetRootComponent());
}

void UTBCharacterAudioComponent::StopCollectLoop(UTBAudioSubsystem* Audio)
{
    if (auto* Component = CollectAudio.Get())
    {
        if (Audio) Audio->StopLoop(Component);
        else Component->Stop();
    }
    CollectAudio.Reset();
    bCollectLoopDesired = false;
}
