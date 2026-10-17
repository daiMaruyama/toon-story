#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBBoxAudioComponent.generated.h"

class ATBBox;
class UAudioComponent;
class UTBAudioSubsystem;

/** Rescue progress audio is private to listeners on the Toys team. */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TOONSTORY_API UTBBoxAudioComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTBBoxAudioComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<ATBBox> Box;
	TWeakObjectPtr<UAudioComponent> RescueAudio;
	bool bRescueLoopDesired = false;
	uint32 LastSettingsRevision = 0;
	void StopRescueLoop(UTBAudioSubsystem* Audio);
};
