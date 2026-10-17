#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBToyCarVisual.generated.h"

class UStaticMeshComponent;
class UChildActorComponent;
class ATBCharacter;

// A lightweight presentation actor; the owning toy character handles movement and replication.
UCLASS()
class TOONSTORY_API ATBToyCarVisual : public AActor
{
	GENERATED_BODY()
public:
	ATBToyCarVisual();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Toy Car")
	TObjectPtr<UStaticMeshComponent> Car;

	void Animate(float Speed, bool Frozen);
	void Ground(UChildActorComponent* Visual, float FloorZ);
	void Grip(UChildActorComponent* Visual, const FVector& Position);
	bool CarryContacts(const ATBCharacter* Human, FVector& Left, FVector& Right) const;
};
