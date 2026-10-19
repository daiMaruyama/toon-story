// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TBTypes.h"
#include "TBBox.generated.h"

class ATBCharacter;
class ATBDummyToy;
class ATBBox;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE(FTBBoxOpened);

/** 固定された箱への収納と、箱外への救助をサーバーで管理する。 */
UCLASS(Config = Game)
class TOONSTORY_API ATBBox : public AActor
{
	GENERATED_BODY()
public:
	ATBBox();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent> InteractionPoint;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<UStaticMesh> BoxMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent> Visual;
	// 見える木箱とは独立した、従来の収納先の部屋。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent> StorageRoom;
	void ReleasePrisoners();
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseRescueSeconds = 5.f;
	// 追加の救助者1人あたりの基準速度に対する加算割合。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rescue", meta = (ClampMin = "0.0"))
	float AdditionalRescuerBonus = .5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float StoreSeconds = 3.f;
	UPROPERTY(Replicated, BlueprintReadOnly)
	float RescueProgress = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 Rescuers = 0;
	bool InRange(const ATBCharacter* C) const;
	bool CanStoreFrom(const ATBCharacter* Character) const;
	bool Store(ATBCharacter* Toy);
	bool StoreDummy(ATBDummyToy* Toy);
	bool HasPrisoners() const;
	// 収納先の候補位置（StorageRoom のローカル座標）。
	static constexpr int32 StorageSlotCount = 9;
	static FVector StorageSlot(int32 Slot);
	// 収納・救助でフタが開く時に各PCで呼ばれる。
	FTBBoxOpened OnOpened;

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOpened();
	void RefreshAppearance();
	bool HasDummyPrisoners() const;
	int32 ReleaseDummyPrisoners(int32 FirstSlot);
};
