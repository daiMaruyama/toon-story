#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TBTypes.h"
#include "TBPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ATBCharacter;
class ATBPickup;

/** 接触による取得を確定し、取得済み状態を同期する。 */
UCLASS()
class TOONSTORY_API ATBPickup : public AActor
{
	GENERATED_BODY()
public:
	ATBPickup();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USphereComponent> Contact;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1"))
	float CollectSeconds = 3.f;
	bool Touches(const ATBCharacter* C) const;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(ReplicatedUsing = OnRep_Taken, BlueprintReadOnly)
	bool bTaken = false;
	UFUNCTION()
	void OnRep_Taken();
	bool Take(ATBCharacter* Who);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
