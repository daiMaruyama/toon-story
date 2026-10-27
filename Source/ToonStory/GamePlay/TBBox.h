// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TBTypes.h"
#include "TBBox.generated.h"

class ATBCharacter;
class ATBDummyToy;
class UBoxComponent;
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
	// Collision geometry owns the room bounds; escape validation must not duplicate its dimensions.
	FBox StorageRoomBounds() const;
	void ReleasePrisoners();
	// Generation zero is reserved for the existing server-side team rescue path.
	bool ReleasePrisoner(ATBCharacter* Toy, uint32 Generation, const TArray<FTransform>& Exits, bool bNotify = true);
	uint32 CaptureGeneration(const ATBCharacter* Toy) const;
	void ResetCaptivity();
	// 救助・収納の秒数や範囲はマスターデータ（TB::Rules().Box）にある。
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
	// 箱の部屋に置いた落下エリア。このタグを付けたアクタのBoxを使う。
	static const FName DropAreaTag;
	static const UBoxComponent* FindDropArea(const UWorld* World);
	// 収納・救助でフタが開く時に各PCで呼ばれる。
	FTBBoxOpened OnOpened;

private:
	struct FCaptivity { uint32 Round = 0; uint32 Generation = 0; };
	TMap<TWeakObjectPtr<ATBCharacter>, FCaptivity> Captivity;
	uint32 NextCaptureGeneration = 0;
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOpened();
	void RefreshAppearance();
	bool HasDummyPrisoners() const;
	bool FindDropSpot(const ATBCharacter* Toy, FVector& OutSpot) const;
	// ダミーも同じ落下エリアに落とすため、形と無視するアクタで探す版。
	bool FindDropSpot(const AActor* Ignore, const FCollisionShape& Shape, FVector& OutSpot) const;
	FVector ReleasePosition(int32 Slot) const;
	int32 ReleaseDummyPrisoners(int32 FirstSlot);
};
