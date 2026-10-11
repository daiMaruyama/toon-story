#pragma once

#include "CoreMinimal.h"

class UTBToyDefinition;

#if WITH_EDITOR
namespace TBToyGaitAuthoring
{
	// Creates derived clips from the preserved source animations, so re-running never accumulates corrections.
	bool Tune(UTBToyDefinition* Definition, const FString& CollectionName);
}
#endif
