#pragma once
#include "CoreMinimal.h"
#include "GamePlay/Items/TBItemBase.h"
#include "TBNoiseRadio.generated.h"

class UAudioComponent;
class USoundBase;
class USoundWaveProcedural;
class UTextRenderComponent;

/** Place in a level. Toys start it with E; humans stop it. No external assets required. */
UCLASS(Blueprintable)
class TOONSTORY_API ATBNoiseRadio : public ATBItemBase
{
	GENERATED_BODY()
public:
	ATBNoiseRadio();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UFUNCTION(BlueprintPure, Category = "Radio")
	bool IsRadioOn() const { return bRadioOn; }
	// Optional replacement: plain waves repeat; authored loops/Cues provide gapless playback.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Audio")
	TObjectPtr<USoundBase> LoopSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Audio", meta = (ClampMin = "0", ClampMax = "4"))
	float RadioVolume = .6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Audio", meta = (ClampMin = "1"))
	float AudibleDistance = 1800.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Masking", meta = (ClampMin = "1"))
	float MaskRadius = 1200.f;
	// Keep the serialized name for already placed radios; applies to all marked toy actions now.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Masking", meta = (ClampMin = "0", ClampMax = "1", DisplayName = "Toy Action Volume Multiplier"))
	float ToyFootstepMultiplier = .25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Visual")
	bool bAnimateSpeaker = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio|Visual")
	bool bUsePrototypeColors = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Radio")
	TObjectPtr<UStaticMeshComponent> Speaker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Radio")
	TObjectPtr<UAudioComponent> RadioAudio;
	float GetMaskAtLocation(const FVector& ListenerLocation) const;
protected:
	virtual bool CanPerformAction_Implementation(const FTBInteractionContext& Context) const override;
	virtual bool PerformAction_Implementation(const FTBInteractionContext& Context) override;
private:
	UPROPERTY(ReplicatedUsing = OnRep_RadioOn)
	bool bRadioOn = false;
	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> PlaceholderSound;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> StatusLabel;
	TArray<uint8> PlaceholderPCM;
	bool bLocalPlaying = false;
	UFUNCTION()
	void OnRep_RadioOn();
	void UpdateLocalPlayback();
	void BuildPlaceholderSound();
};
