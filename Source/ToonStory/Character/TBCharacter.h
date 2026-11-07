// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/TBTypes.h"
#include "TBCharacter.generated.h"

struct FInputActionValue;
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

// Presentation of the server's gaze direction, independent of camera effects and root turning lag.
USTRUCT()
struct FTBHumanLookSnapshot
{
	GENERATED_BODY()
	UPROPERTY()
	bool bValid = false;
	UPROPERTY()
	FRotator View = FRotator::ZeroRotator;
	UPROPERTY()
	uint32 Round = 0;
	UPROPERTY()
	uint32 Possession = 0;
	UPROPERTY()
	TObjectPtr<ATBPlayerState> Player;
};

UENUM()
enum class ETBEmoteKind : uint8 { None, Wave, Cheer, Dance, WildDance, Surprise, HairDisappear };

USTRUCT()
struct FTBEmoteSnapshot
{
	GENERATED_BODY()
	UPROPERTY() bool bActive = false;
	UPROPERTY() ETBEmoteKind Kind = ETBEmoteKind::None;
	UPROPERTY() uint32 Revision = 0;
	UPROPERTY() uint32 Round = 0;
	UPROPERTY() uint32 Possession = 0;
	UPROPERTY() TObjectPtr<ATBPlayerState> Player;
	UPROPERTY() ETBTeam Team = ETBTeam::None;
	UPROPERTY() ETBPhase Phase = ETBPhase::Lobby;
	UPROPERTY() int32 Visual = INDEX_NONE;
	UPROPERTY() double StartsAt = 0;
	UPROPERTY() double EndsAt = 0;
	UPROPERTY() double NextAllowedAt = 0;
};

// Game-thread snapshot for presentation; no UObject references cross into animation evaluation.
struct FTBEmoteSample
{
	ETBEmoteKind Kind = ETBEmoteKind::None;
	float Progress = 0;
	float Weight = 0;
};

/** 人間とおもちゃ共通の本体。操作要求をサーバーで検証する。 */
UCLASS(Config = Game)
class TOONSTORY_API ATBCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	ATBCharacter(const FObjectInitializer& Init);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void OnRep_PlayerState() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
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
	UPROPERTY(VisibleAnywhere, Category = "Appearance")
	TObjectPtr<class UChildActorComponent> HumanVisual;
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
	void SetGazeRequested(bool bRequested);
	// 人間がボタンを押して凝視しているか。サーバーには移動のフラグで届く。
	bool IsGazing() const;
	// 凝視の視野の寄り具合（0〜1）。画面の演出に使う。
	float GetGazeZoom() const { return GazeZoom; }
	UFUNCTION()
	void OnRep_Carrier();
	ATBPlayerState* TBPS() const;
	bool IsHuman() const;
	bool IsCelebrating() const;
	// Game-thread read for animation. Simulated peers must have a matching resolved snapshot.
	bool GetHumanLook(FTBHumanLookSnapshot& Out) const;
	bool CanRequestEmote() const;
	bool SupportsEmotes() const;
	bool RequestEmote(ETBEmoteKind Kind);
	bool TryEmote(ETBEmoteKind Kind);
	bool GetEmoteSample(FTBEmoteSample& Out) const;
	// エモート1回分の秒数。タイトルのおもちゃも同じ速さで回す。
	static double EmoteDuration(ETBEmoteKind Kind);
	UFUNCTION(Server, Reliable)
	void ServerEmote(ETBEmoteKind Kind, uint32 ExpectedRound, uint32 ExpectedPossession);
	UFUNCTION()
	void OnEmoteVisualEndPlay(AActor* Visual, EEndPlayReason::Type Reason);
	FVector GetCarryGripLocation() const;
	bool CanAct() const;
	bool IsMovementLocked() const;
	bool IsPhysicsLocked() const;
	// 見られ方。GazeStateは本人にだけ届くので、IsSeenとIsGazedはサーバーとおもちゃ本人のPCでだけ正しい。
	// 視界に入っているが、凝視はされていない。
	UFUNCTION(BlueprintPure, Category = "Gaze")
	bool IsSeen() const;
	// 凝視されていて、止まる前。
	UFUNCTION(BlueprintPure, Category = "Gaze")
	bool IsGazed() const;
	// 止まっている。bFrozenは全員に届く。
	UFUNCTION(BlueprintPure, Category = "Gaze")
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
	void RequestTaunt();
	void RequestPush();
	bool TryPush();
	UFUNCTION(Server, Reliable)
	void ServerPush();
	/** Validates team, state and cooldown on the server before emitting spatial audio. */
	bool TryTaunt();
	UFUNCTION(Server, Reliable)
	void ServerTaunt();
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
	// Soft references select assets but do not keep them alive before presentation or while showing a toy.
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> LoadedChildMesh;
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> LoadedChildAnimation;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTBHumanLookTest;
	friend class FTBEmoteTest;
	friend class FTBLobbyExplorationTest;
#endif
	UPROPERTY(ReplicatedUsing = OnRep_Emote)
	FTBEmoteSnapshot Emote;
	uint32 ObservedEmoteRevision = 0;
	mutable uint32 BlockedEmoteRevision = 0;
	mutable uint32 PresentedEmoteRevision = 0;
	bool bEmoteRejected = false;
	uint64 LastEmoteCancelFrame = MAX_uint64;
	UFUNCTION()
	void OnRep_Emote();
	bool EmoteEligible() const;
	bool EmoteGameplayEligible() const;
	int32 EmoteVisualIndex() const;
	void UpdateEmote();
	void UpdateHairEmote();
	void RestoreEmoteHair();
	TMap<TWeakObjectPtr<UPrimitiveComponent>, bool> EmoteHairVisibility;
	void CancelEmote();
	UPROPERTY(ReplicatedUsing = OnRep_HumanLook)
	FTBHumanLookSnapshot HumanLook;
	uint32 HumanLookPossession = 0;
	uint32 ObservedHumanLookPossession = 0;
	bool bHumanLookRejected = false;
	UFUNCTION()
	void OnRep_HumanLook();
	void UpdateHumanLook();
	void InvalidateHumanLook();
	bool UpdateCarriedDummy();
	void BeginDummyStorage();
	void InitializeComfortMovement();
	void ApplyComfortBody(bool bToy);
	void ApplyComfortMovement(bool bToy);
	void Forward(float V);
	void Right(float V);
	void Move(const FInputActionValue& Value);
	void EmotePressed(ETBEmoteKind Kind);
	void Look(const FInputActionValue& Value);
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
	void GazePressed();
	void GazeReleased();
	double LastRequest = -1000;
	double LastTauntTime = -1000;
	double LastPushTime = -1000;
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
	// 凝視の視野の寄り具合。0で通常、1で寄りきった状態。
	float GazeZoom = 0;
	// 凝視で止まった画面のセピアの濃さ。0で通常、1でセピア。
	float FrozenSepia = 0;

protected:
	// 親子で表示の切り替え状態を共有する。
	FVector StandingMeshOffset = FVector::ZeroVector;
	bool bAppearanceInitialized = false;
	bool bShowingToy = false;
	virtual void UpdateAppearance(bool bToy);
};
