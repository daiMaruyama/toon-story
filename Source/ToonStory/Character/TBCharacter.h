#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/TBTypes.h"
#include "TBCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class ATBPlayerState;
class ATBPickup;

/** 人間とおもちゃ共通の本体。操作要求をサーバーで検証する。 */
UCLASS()
class TOONSTORY_API ATBCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ATBCharacter(const FObjectInitializer& Init);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void OnRep_PlayerState() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> CarryAnchor;
	// Carrierは自分を運ぶ相手、CarriedToyは自分が運ぶおもちゃ。
	UPROPERTY(ReplicatedUsing = OnRep_Carrier, BlueprintReadOnly)
	TObjectPtr<ATBCharacter> Carrier;
	UPROPERTY(Replicated, BlueprintReadOnly)
	TObjectPtr<ATBCharacter> CarriedToy;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float HumanSpeed = 450.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ToySpeed = 350.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float CarrySpeed = 300.f;
	UFUNCTION()
	void OnRep_Carrier();
	ATBPlayerState* TBPS() const;
	bool IsHuman() const;
	bool CanAct() const;
	bool IsMovementLocked() const;
	bool IsPhysicsLocked() const;
	bool IsGazeFrozen() const;
	void UpdateItemContact(float DeltaSeconds);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void ApplyInterruption();
	// 取得対象と進捗は所有プレイヤーだけへ同期する（登録は.cpp側）。
	UPROPERTY(Replicated, BlueprintReadOnly)
	TObjectPtr<ATBPickup> ContactItem;
	UPROPERTY(Replicated, BlueprintReadOnly)
	float ItemProgress = 0.f;
	void ApplyState();
	void SetFrozen(bool Value);
	void SetToyState(ETBToyState Value);
	void ClearHeldInteraction();
	void DropToy();
	void RequestInteract();
	void ReleaseInteract();
	void RequestDrop();
	UFUNCTION(Server, Reliable)
	void ServerInteract();
	UFUNCTION(Server, Reliable)
	void ServerReleaseInteract();
	UFUNCTION(Server, Reliable)
	void ServerDrop();
	UFUNCTION(BlueprintImplementableEvent)
	void StateVisualChanged();
	// 操作の開始・終了時刻はサーバーで管理する。
	double GrabEnds = 0;
	double StoreStarted = 0;
	bool bHoldingInteract = false;
	bool bRescuing = false;

private:
	void Forward(float V);
	void Right(float V);
	void Yaw(float V);
	void Pitch(float V);
	void JumpPressed();
	void JumpReleased();
	double LastRequest = -1000;
	bool bWasPhysicsLocked = false;
};
