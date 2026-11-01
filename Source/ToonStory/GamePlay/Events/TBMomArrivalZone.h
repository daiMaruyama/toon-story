#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBMomArrivalZone.generated.h"

class ATBCharacter;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/** 進行をまとめて複製し、参加途中のクライアントにも同じ残り時間を表示する。 */
USTRUCT()
struct FTBMomArrivalState
{
	GENERATED_BODY()
	UPROPERTY() bool bActive = false;
	UPROPERTY() bool bOccupied = false;
	UPROPERTY() float Remaining = 0;
	UPROPERTY() float Held = 0;
	UPROPERTY() float RequiredHold = 5;
	UPROPERTY() float Radius = 60;
	UPROPERTY() float Height = 120;
};

/** 原点を床に置く。範囲と秒数はDA_TBRulesのイベント設定から読む。 */
UCLASS()
class TOONSTORY_API ATBMomArrivalZone : public AActor
{
	GENERATED_BODY()
public:
	ATBMomArrivalZone();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	/** GameModeが通常の勝敗判定前に進める。クライアントでは実行しない。 */
	void Advance(float DeltaSeconds);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(EditAnywhere, Category="ママが来る！", meta=(DisplayName="この地点を候補にする"))
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, Category="ママが来る！", meta=(DisplayName="赤い点線のマテリアル"))
	TObjectPtr<UMaterialInterface> BoundaryMaterial;
	UPROPERTY(ReplicatedUsing=OnRep_State)
	FTBMomArrivalState State;
	bool StartEvent();
	void ResetEvent();
	bool ContainsHuman(const ATBCharacter& Human) const;
	static ATBMomArrivalZone* FindActive(UWorld* World);
	/** NPCだけでは対処不能になるため、人間陣営を操作中のプレイヤーが必要。 */
	static bool HasHumanPlayer(UWorld* World);
private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Marker;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Boundary;
	UFUNCTION() void OnRep_State();
	void BuildBoundary(float Radius);
	float DisplayRadius = -1;
	uint32 Round = 0;
};
