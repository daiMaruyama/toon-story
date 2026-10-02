#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TBTypes.h"
#include "TBBox.generated.h"

class USoundBase;
class ATBCharacter;
class ATBBox;

/** 固定された箱への収納と、箱外への救助をサーバーで管理する。 */
UCLASS()
class TOONSTORY_API ATBBox : public AActor
{
	GENERATED_BODY()
public:
	ATBBox();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent> InteractionPoint;
	void ReleasePrisoners();
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> AlarmSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseRescueSeconds = 10.f;
	// 追加の救助者1人あたりの基準速度に対する加算割合。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rescue", meta = (ClampMin = "0.0"))
	float AdditionalRescuerBonus = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float StoreSeconds = 3.f;
	UPROPERTY(Replicated, BlueprintReadOnly)
	float RescueProgress = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 Rescuers = 0;
	UPROPERTY(Replicated, BlueprintReadOnly)
	double AlarmUntil = 0;
	bool InRange(const ATBCharacter* C) const;
	bool Store(ATBCharacter* Toy);
	bool HasPrisoners() const;
	UFUNCTION(NetMulticast, Reliable)
	void MulticastAlarm();
	UFUNCTION(BlueprintImplementableEvent)
	void AlarmVisual();
};
