#pragma once
#include "CoreMinimal.h"

class UChildActorComponent;
class USkeletalMeshComponent;
class ATBCharacter;

struct FTBGripCache
{
	TWeakObjectPtr<USkeletalMeshComponent> Part;
	FName BodyBone = NAME_None;
	FVector ContactInBody = FVector::ZeroVector;
};

// Players and decoys deliberately share the same mesh, idle pose and floor correction.
namespace TBToyPresentation
{
	inline constexpr float SizeScale = .75f;
	void Initialize(AActor* Visual);
	void Animate(AActor* Visual, float Speed, bool Frozen);
	void Ground(UChildActorComponent* Visual, float FloorZ);
	void Grip(UChildActorComponent* Visual, const FVector& HandPosition, FTBGripCache& Cache);
	bool CarryContacts(const ATBCharacter* Human, FVector& Left, FVector& Right);
} // namespace TBToyPresentation
