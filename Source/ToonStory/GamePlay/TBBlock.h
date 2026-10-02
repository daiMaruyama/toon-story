#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/TBTypes.h"
#include "TBBlock.generated.h"

class UStaticMeshComponent;

/** テスト用の床・壁。サーバーで生成した位置とサイズを同期する。 */
UCLASS()
class TOONSTORY_API ATBBlock : public AActor
{
	GENERATED_BODY()
public:
	ATBBlock();
	UPROPERTY(ReplicatedUsing = OnRep_Size)
	FVector Size = FVector::OneVector;
	UFUNCTION()
	void OnRep_Size();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};
