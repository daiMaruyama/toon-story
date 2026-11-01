// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBTVRemote.generated.h"

class ATBBoxTV;
class ATBCharacter;
class ATBGameState;
class ATBPlayerState;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTBTVButton;

/** 箱内テレビのリモコン。ボタンはBPで、アクタのスケールで全体の大きさを変えられる。 */
UCLASS(Blueprintable)
class TOONSTORY_API ATBTVRemote : public AActor
{
	GENERATED_BODY()
public:
	ATBTVRemote();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Remote")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditInstanceOnly, Category = "Remote")
	TObjectPtr<ATBBoxTV> TV;
	// 続けて押せるまでの時間はマスターデータ（TB::PresentationData().Remote）にある。

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTBTVTest;
	friend class FTBTVRemoteTest;
#endif
	struct FContact
	{
		TWeakObjectPtr<ATBPlayerState> Player;
		TWeakObjectPtr<UTBTVButton> Candidate;
		TWeakObjectPtr<UTBTVButton> Latched;
		FVector Anchor = FVector::ZeroVector;
		FTransform ButtonTransform;
		uint8 Key = 0;
		int32 Digit = 0;
		double Since = -1;
		double LeftSince = -1;
	};
	TMap<TWeakObjectPtr<ATBCharacter>, FContact> Contacts;
	TWeakObjectPtr<ATBGameState> ContactState;
	TWeakObjectPtr<ATBBoxTV> ContactTV;
	uint32 ContactRound = 0;
	double LastEvaluation = -1;
	FTransform ContactTransform;
	double LastPressTime = -1;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTBTVButton>> Buttons;
	// Buttons と同じ並びのマテリアルのパラメータ名。
	TArray<FName> GlowParameters;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterial;
	UFUNCTION()
	void OnButtonOverlap(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	                     int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnButtonEndOverlap(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	                        int32 OtherBodyIndex);
	// 押せる・踏まれている・押せない をボタンごとにマテリアルへ反映する。
	void UpdateGlow();
	void UpdateButtonGlow(int32 Index, const UTBTVButton* Preview, bool bConfirmed);
	UTBTVButton* PreviewButton(ATBCharacter*& Viewer) const;
	void EvaluateContacts(double Now);
	bool IsEligible(const ATBCharacter* Viewer) const;
	bool SupportPoint(const ATBCharacter* Viewer, FVector& Point) const;
	bool InButton(const UTBTVButton* Button, const FVector& Point, float Margin) const;
	UTBTVButton* SelectButton(const ATBCharacter* Viewer, const FVector& Point) const;
	bool Dispatch(ATBCharacter* Viewer, const UTBTVButton* Button, double Now);
};
