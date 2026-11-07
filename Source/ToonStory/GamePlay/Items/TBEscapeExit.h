#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBEscapeExit.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** レベル担当者が車両の出口に設置する領域。 */
UCLASS(Blueprintable)
class TOONSTORY_API ATBEscapeExit : public AActor
{
	GENERATED_BODY()
public:
	ATBEscapeExit();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="脱出")
	TObjectPtr<UBoxComponent> ExitArea;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="脱出")
	TObjectPtr<UStaticMeshComponent> Marker;
};
