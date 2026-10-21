// Copyright (c) 2026 Mamodou. All Rights Reserved.

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
class UTBVoiceComponent;
class ATBDummyToy;

/** 人間とおもちゃ共通の本体。操作要求をサーバーで検証する。 */
UCLASS(Config = Game)
class TOONSTORY_API ATBCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ATBCharacter(const FObjectInitializer& Init);
	virtual void BeginPlay() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
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
	// 速さ・ジャンプ・段差・担がれカメラの値はマスターデータ（TB::CharacterData()）にある。
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftObjectPtr<USkeletalMesh> ChildMesh;
	UPROPERTY(Config, EditDefaultsOnly, Category = "Appearance")
	TSoftClassPtr<UAnimInstance> ChildAnimation;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> CarryAnchor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UTBVoiceComponent> Voice;
	// Carrierは自分を運ぶ相手、CarriedToyは自分が運ぶおもちゃ。
	UPROPERTY(ReplicatedUsing = OnRep_Carrier, BlueprintReadOnly)
	TObjectPtr<ATBCharacter> Carrier;
	UPROPERTY(Replicated, BlueprintReadOnly)
	TObjectPtr<ATBCharacter> CarriedToy;
	UPROPERTY(Replicated, BlueprintReadOnly)
	TObjectPtr<ATBDummyToy> CarriedDummy;
	UFUNCTION(BlueprintCallable, Category = "Movement")
	void SetSprintRequested(bool bRequested);
	UFUNCTION()
	void OnRep_Carrier();
	ATBPlayerState* TBPS() const;
	bool IsHuman() const;
	FVector GetCarryGripLocation() const;
	bool CanAct() const;
	bool IsMovementLocked() const;
	bool IsPhysicsLocked() const;
	bool IsGazeFrozen() const;
	// 本人のPCで、運搬カメラに切り替わっているか。降ろされた後は、視点を水平に戻してから false になる。
	bool IsCarryViewActive() const { return bCarryView; }
	void UpdateItemContact(float DeltaSeconds);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void ApplyInterruption();
	// 取得対象と進捗は所有プレイヤーだけへ同期する（登録は.cpp側）。
	UPROPERTY(Replicated, BlueprintReadOnly)
	TObjectPtr<ATBPickup> ContactItem;
	UPROPERTY(Replicated, BlueprintReadOnly)
	float ItemProgress = 0.f;
	void ApplyState();
	bool HasCarriedToy() const { return CarriedToy || CarriedDummy; }
	void SetFrozen(bool Value);
	bool CanBeFrozenBy(const ATBCharacter* Observer) const;
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
	bool UpdateCarriedDummy();
	void BeginDummyStorage();
	void InitializeComfortMovement();
	void ApplyComfortBody(bool bToy);
	void ApplyComfortMovement(bool bToy);
	void Forward(float V);
	void Right(float V);
	void Yaw(float V);
	void Pitch(float V);
	static FVector2D LookScale();
	void JumpPressed();
	void JumpReleased();
	void UpdateCarryCamera();
	void CrouchPressed();
	void CrouchReleased();
	void SprintPressed();
	void SprintReleased();
	double LastRequest = -1000;
	bool bWasPhysicsLocked = false;
	// 表示用。運ばれているかの正解はCarrier。
	bool bCarryView = false;
	// 運搬カメラを今付けている相手。途中のnullptrが届かず運ぶ人が入れ替わる場合に付け直す。
	TWeakObjectPtr<ATBCharacter> CarryViewCarrier;
	TWeakObjectPtr<ATBCharacter> GripTickCarrier;
	// 直前のフレームに画面へ出した視点。切替時にここからブレンドを始める。
	FMinimalViewInfo LastView;
	FVector LastActorLocation = FVector::ZeroVector;
	bool bHasLastView = false;
	FMinimalViewInfo CarryBlendFrom;
	double CarryBlendStarted = -1;
	float CarryBlendDuration = 0;
	// 段差で遅らせている目線の高さ。0 に向かって戻っていく。
	float StepOffset = 0;
	// 前フレームの視点を出した時点で運ばれていたか・しゃがんでいたか。切り替わった分の移動は段差に数えない。
	bool bLastViewCarried = false;
	bool bLastViewCrouched = false;

protected:
	// 親子で表示の切り替え状態を共有する。
	FVector StandingMeshOffset = FVector::ZeroVector;
	bool bAppearanceInitialized = false;
	bool bShowingToy = false;
	virtual void UpdateAppearance(bool bToy);
};
