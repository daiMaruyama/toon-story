#pragma once
#include "CoreMinimal.h"
#include "GamePlay/Items/TBItemBase.h"
#include "TBNoiseRadio.generated.h"

class UAudioComponent;
class USoundWaveProcedural;
class UTextRenderComponent;
class UTBNoiseRadioData;

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
	// Sound, volume, reach and toy-action masking. Defaults to /Game/Data/DA_NoiseRadio; a placed radio may use another.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Radio")
	TSoftObjectPtr<UTBNoiseRadioData> RadioData;
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
	// Loaded in BeginPlay; until then the class defaults are used.
	UPROPERTY(Transient)
	TObjectPtr<UTBNoiseRadioData> LoadedData;
	const UTBNoiseRadioData& Data() const;
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
