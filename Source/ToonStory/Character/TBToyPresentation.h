#pragma once
#include "CoreMinimal.h"

class UChildActorComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UStaticMeshComponent;
class ATBCharacter;
class AActor;
struct FPoseContext;

struct FTBEmoteBone
{
	FName Name;
	FQuat Rotation = FQuat::Identity;
};
struct FTBEmotePose
{
	TArray<FTBEmoteBone> Bones;
};

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
	int32 SelectionWeight(const UClass* VisualClass);
	int32 SelectToy(const TArray<TSubclassOf<AActor>>& Catalogue);
	void Initialize(AActor* Visual);
	void Animate(AActor* Visual, float Speed, bool Frozen);
	void Ground(UChildActorComponent* Visual, float FloorZ);
	void Grip(UChildActorComponent* Visual, const FVector& HandPosition, FTBGripCache& Cache);
	bool CarryContacts(const ATBCharacter* Human, FVector& Left, FVector& Right);
	bool SupportsEmoteMesh(const USkeletalMesh* Mesh);
	bool SupportsEmoteClass(UClass* VisualClass);
	void BuildEmotePose(const USkeletalMeshComponent* Mesh, const ATBCharacter* Pawn, FTBEmotePose& Out);
	void ApplyEmotePose(FPoseContext& Output, const FTBEmotePose& Pose);
	void ApplyCarEmote(UStaticMeshComponent* Car, ATBCharacter* Pawn);
	void RestoreEmoteVisual(AActor* Visual);
	void RestoreEmoteOwner(ATBCharacter* Pawn);
} // namespace TBToyPresentation
