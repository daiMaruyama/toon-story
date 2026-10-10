#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBCharacterAppearanceComponent.generated.h"

class UTBToyDefinition;
class ATBCharacter;
class UTBToyAnimationComponent;

/** Applies character presentation; game rules and replicated selection remain on the character. */
UCLASS(ClassGroup = (ToonStory))
class TOONSTORY_API UTBCharacterAppearanceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	void SetDefinition(UTBToyDefinition* Definition);
	UTBToyDefinition* GetDefinition() const { return LoadedToyDefinition; }
	void Apply(ATBCharacter& Character, UTBToyAnimationComponent& Animation, bool bToy);

private:
	UPROPERTY(Transient)
	TObjectPtr<UTBToyDefinition> LoadedToyDefinition;
	bool bAppearanceInitialized = false;
	bool bShowingToy = false;
};
