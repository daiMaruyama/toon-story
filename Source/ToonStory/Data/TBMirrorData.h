#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBMirrorData.generated.h"

/** Shared tuning for passive, single-bounce mirrors. Dimensions are in centimeters. */
UCLASS(BlueprintType)
class TOONSTORY_API UTBMirrorData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror", meta=(ClampMin="10", Units="cm"))
	float Width = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror", meta=(ClampMin="10", Units="cm"))
	float Height = 160.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror", meta=(ClampMin="1", Units="cm"))
	float FrameWidth = 4.f;
	/** Same limit for rendering and gaze: observer distance from the mirror center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror", meta=(ClampMin="100", Units="cm"))
	float ViewDistance = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering", meta=(ClampMin="128", ClampMax="2048"))
	int32 CaptureWidth = 512;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering", meta=(ClampMin="1", ClampMax="60"))
	float FramesPerSecond = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering")
	TSoftObjectPtr<class UMaterialInterface> MirrorMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rendering")
	TSoftObjectPtr<class UMaterialInterface> FrameMaterial;
};
