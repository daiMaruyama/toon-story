#pragma once

#include "CoreMinimal.h"
#include "Audio/TBAudioTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TBAudioEvents.generated.h"

class APlayerState;
class ATBPlayerState;

/** Confirmed gameplay sounds are delivered through one server-owned route. */
UCLASS()
class TOONSTORY_API UTBAudioEvents : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Call only after the gameplay action succeeds. Empty sound slots are safe. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ToonStory|Audio",
	          meta = (WorldContext = "WorldContext"))
	static void Emit(UObject* WorldContext, ETBSoundEvent Event, APlayerState* Subject, FVector Location);

	/** Audience rules are shared by every emitter; rescue cues never reach Children. */
	static bool ShouldDeliver(ETBSoundEvent Event, const ATBPlayerState* Recipient, const APlayerState* Subject);
};
