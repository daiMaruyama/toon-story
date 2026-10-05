#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/TBTypes.h"
#include "TBCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UStaticMesh;
class ATBPlayerState;
class ATBPickup;
class USkeletalMesh;
class UAnimInstance;
class UAnimSequence;

/** 人間とおもちゃ共通の本体。操作要求をサーバーで検証する。 */
UCLASS(Config = Game)
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
	// 運ばれている間だけ使う、運ぶ人間を中心にした三人称カメラ。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USpringArmComponent> CarryCameraArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UCameraComponent> CarryCamera;
	// 運ばれている間の見下ろし角。左右には回り込めるが、上下は固定。
	UPROPERTY(EditDefaultsOnly, Category = "Carry Camera")
	float CarryViewPitch = -40.f;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<UStaticMesh> ToyMesh;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<USkeletalMesh> ToySkeletalMesh;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<UAnimSequence> ToyIdleAnimation;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<UAnimSequence> ToyWalkAnimation;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<UAnimSequence> ToyJumpAnimation;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<USkeletalMesh> ChildMesh;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftClassPtr<UAnimInstance> ChildAnimation;
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
	float ToySpeed = 180.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float ToySprintSpeed = 320.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float ToyJumpVelocity = 400.f;
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetSprintRequested(bool bRequested);
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
	UPROPERTY(Replicated)
	double StoreStarted = -1;
	bool bHoldingInteract = false;
	bool bRescuing = false;

private:
	void Forward(float V);
	void Right(float V);
	void Yaw(float V);
	void Pitch(float V);
	void JumpPressed();
	void JumpReleased();
	void UpdateCarryCamera();
	void SprintPressed();
	void SprintReleased();
	double LastRequest = -1000;
	bool bWasPhysicsLocked = false;
	// 表示用。運ばれているかの正解はCarrier。
	bool bCarryView = false;
	// 運搬カメラを今付けている相手。途中のnullptrが届かず運ぶ人が入れ替わる場合に付け直す。
	TWeakObjectPtr<ATBCharacter> CarryViewCarrier;
	bool bAppearanceInitialized = false;
	bool bShowingToy = false;
	void UpdateAppearance(bool bToy);
	void UpdateToyAnimation();
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ActiveToyAnimation;
};
