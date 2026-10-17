#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TBTypes.h"
#include "TBCharacterAudioComponent.generated.h"

class ATBCharacter;
class UAudioComponent;
class UTBAudioSubsystem;

/** Each listener observes replicated movement locally; no per-step network messages are needed. */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TOONSTORY_API UTBCharacterAudioComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTBCharacterAudioComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// Distance-based approximation until animation contact notifies are available.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Footsteps", meta = (ClampMin = "1.0"))
	float HumanStepDistance = 130.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Footsteps", meta = (ClampMin = "1.0"))
	float ToyStepDistance = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Footsteps")
	bool bAutomaticFootsteps = true;

private:
	TWeakObjectPtr<ATBCharacter> Character;
	TWeakObjectPtr<UAudioComponent> StoreAudio;
	FVector PreviousLocation = FVector::ZeroVector;
	ETBTeam PreviousTeam = ETBTeam::None;
	ETBToyState PreviousToyState = ETBToyState::Free;
	float StepDistance = 0.f;
	bool bHasMovementSample = false;
	bool bWasGrounded = false;
	bool bWasFalling = false;
	bool bObservedAirborneDeparture = false;
	bool bStoreLoopDesired = false;
	uint32 LastSettingsRevision = 0;

	void ResetMovement();
	void UpdateMovement(ATBCharacter* OwnerCharacter, UTBAudioSubsystem* Audio, float DeltaTime);
	void UpdateStoreLoop(ATBCharacter* OwnerCharacter, UTBAudioSubsystem* Audio);
	void StopStoreLoop(UTBAudioSubsystem* Audio);
};
