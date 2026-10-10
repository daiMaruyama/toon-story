#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBToyDefinition.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UMaterialInterface;
class UAnimSequence;
class UAnimInstance;
class UBlendSpace;

/** Model-specific presentation. Gameplay, carrying and replication stay in TBCharacter. */
UCLASS(BlueprintType)
class TOONSTORY_API UTBToyDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	// Import meshes/clips with UE's Content Browser, assign them below, then use these editor actions.
	UFUNCTION(CallInEditor, Category = "Toy Setup")
	void CreateLocomotion();
	UFUNCTION(CallInEditor, Category = "Toy Setup")
	void PrepareMaterials();
	UFUNCTION(CallInEditor, Category = "Toy Setup")
	void ValidateToy();
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Animation")
	void RebuildLocomotion();
	bool ValidateDefinition(FString& OutMessage) const;
	UPROPERTY(VisibleAnywhere, Transient, Category = "Toy Setup")
	FString SetupStatus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TObjectPtr<USkeletalMesh> SkeletalMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TObjectPtr<UStaticMesh> StaticMesh;
	// Empty slots use the mesh's original materials; never force one toy's color on another.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model", meta = (ClampMin = "1.0"))
	float Height = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Model")
	float FacingYaw = 0.f;
	// Zero retains the character's gameplay defaults. Values are cm/s.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> Idle;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> Walk;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> Run;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimSequence> Jump;
	// Speed in cm/s on X. A shared locomotion path for any retargeted toy skeleton.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UBlendSpace> Locomotion;
	// Optional alternative for blend spaces, quadrupeds, wheels or other custom rigs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> AnimationBlueprint;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "1.0"))
	float WalkReferenceSpeed = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "1.0"))
	float RunReferenceSpeed = 320.f;
};
