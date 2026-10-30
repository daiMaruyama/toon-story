#pragma once
#include "CoreMinimal.h"
#include "GamePlay/Items/TBItemBase.h"
#include "TBMirror.generated.h"

class UTBMirrorData;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;

/** Local X=0 is the reflective plane; +X is its front. Origin is the surface center. */
UCLASS()
class TOONSTORY_API ATBMirror : public ATBItemBase
{
	GENERATED_BODY()
public:
	ATBMirror();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TArray<FTBItemAction> GetInteractionActions() const override { return {}; }
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mirror")
	TSoftObjectPtr<UTBMirrorData> MirrorData;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mirror")
	TObjectPtr<UStaticMeshComponent> Surface;
	UPROPERTY(VisibleAnywhere, Category="Mirror")
	TArray<TObjectPtr<UStaticMeshComponent>> Frame;
	UPROPERTY(VisibleAnywhere, Category="Mirror")
	TObjectPtr<USceneCaptureComponent2D> ReflectionCamera;
	const UTBMirrorData& GetData() const;
	bool ReflectionPoint(FVector Eye, FVector Target, FVector& Hit) const;
	// Tests both unobstructed ray segments and the observer's view frustum. No renderer dependency.
	static bool FindView(const AActor* Observer, const AActor* Target, FVector Eye, FRotator View,
	    FVector Sample, float TanH, float TanV, FVector& Hit, float MaxPathLength = 0.f, float MinViewDot = -1.f);
	static FVector ReflectPoint(FVector Point, FVector Origin, FVector Normal);
	static FVector ReflectDirection(FVector Direction, FVector Normal);
protected:
	virtual void BeginPlay() override;
	virtual bool CanPerformAction_Implementation(const FTBInteractionContext&) const override { return false; }
private:
	friend class FTBMirrorCaptureTest;
	void ApplyData();
	UPROPERTY(Transient) TObjectPtr<UTBMirrorData> LoadedData;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> TargetTexture;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SurfaceMaterial;
	double NextCapture = 0;
};
