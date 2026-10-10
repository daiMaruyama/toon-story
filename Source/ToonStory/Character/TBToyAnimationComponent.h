#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBToyAnimationComponent.generated.h"

class USkeletalMeshComponent;
class UTBToyDefinition;
class UAnimSequence;

/** Toy locomotion playback, independent of character movement and game rules. */
UCLASS(ClassGroup = (ToonStory), meta = (BlueprintSpawnableComponent))
class TOONSTORY_API UTBToyAnimationComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	void Configure(USkeletalMeshComponent* InMesh, UTBToyDefinition* InDefinition,
	               float InWalkSpeed, float InRunSpeed);
	void Reset();
	void Update(float Speed, bool bFalling, bool bLocked);

private:
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(Transient)
	TObjectPtr<UTBToyDefinition> Definition;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ActiveAnimation;
	float WalkSpeed = 180.f;
	float RunSpeed = 320.f;
};
