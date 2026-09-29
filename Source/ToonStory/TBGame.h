#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "TBGame.generated.h"

class UCameraComponent;
class USphereComponent;
class ATBPickup;
class UStaticMeshComponent;
class USoundBase;
class ATBCharacter;
class ATBBox;

UENUM(BlueprintType)
enum class ETBTeam : uint8
{
	None,
	Human,
	Toy
};
UENUM(BlueprintType)
enum class ETBToyState : uint8
{
	Free,
	Grabbed,
	Carried,
	Storing,
	Boxed
};
UENUM(BlueprintType)
enum class ETBPhase : uint8
{
	Lobby,
	Playing,
	Results,
	Aborted
};
UENUM(BlueprintType)
enum class ETBWinner : uint8
{
	None,
	Humans,
	Toys
};

USTRUCT(BlueprintType)
struct FTBSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Humans = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Toys = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Duration = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RequiredItems = 5;
};

/** プレイヤーごとの陣営・Ready・捕獲状態を全員へ同期する。 */
UCLASS()
class TOONSTORY_API ATBPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly)
	ETBTeam Team = ETBTeam::None;
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly)
	ETBToyState ToyState = ETBToyState::Free;
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly)
	bool bFrozen = false;
	UPROPERTY(Replicated, BlueprintReadOnly)
	bool bReady = false;
	UPROPERTY(Replicated, BlueprintReadOnly)
	ETBTeam Preference = ETBTeam::None;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 Items = 0;
	UFUNCTION()
	void OnRep_State();
	virtual void CopyProperties(APlayerState* NewState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

/** 試合設定・残り時間・勝敗など、全員が参照する情報を持つ。 */
UCLASS()
class TOONSTORY_API ATBGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	UPROPERTY(Replicated, BlueprintReadOnly)
	FTBSettings Settings;
	UPROPERTY(Replicated, BlueprintReadOnly)
	ETBPhase Phase = ETBPhase::Lobby;
	UPROPERTY(Replicated, BlueprintReadOnly)
	ETBWinner Winner = ETBWinner::None;
	UPROPERTY(Replicated, BlueprintReadOnly)
	FString Reason;
	UPROPERTY(Replicated, BlueprintReadOnly)
	double EndTime = 0;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 Collected = 0;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 BoxedCount = 0;
	UPROPERTY(Replicated, BlueprintReadOnly)
	TObjectPtr<ATBBox> Box;
	UFUNCTION(BlueprintPure)
	float Remaining() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

// クライアントから届く移動にも、同じ凍結・運搬制限を適用する。
/** 凍結中は横移動だけを止め、重力による落下を継続する。 */
UCLASS()
class TOONSTORY_API UTBMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	virtual void PerformMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags,
	                            const FVector& NewAccel) override;
};

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

/** 固定された箱への収納と、箱外への救助をサーバーで管理する。 */
UCLASS()
class TOONSTORY_API ATBBox : public AActor
{
	GENERATED_BODY()
public:
	ATBBox();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent> InteractionPoint;
	void ReleasePrisoners();
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> AlarmSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BaseRescueSeconds = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float StoreSeconds = 3.f;
	UPROPERTY(Replicated, BlueprintReadOnly)
	float RescueProgress = 0.f;
	UPROPERTY(Replicated, BlueprintReadOnly)
	int32 Rescuers = 0;
	UPROPERTY(Replicated, BlueprintReadOnly)
	double AlarmUntil = 0;
	bool InRange(const ATBCharacter* C) const;
	bool Store(ATBCharacter* Toy);
	bool HasPrisoners() const;
	UFUNCTION(NetMulticast, Reliable)
	void MulticastAlarm();
	UFUNCTION(BlueprintImplementableEvent)
	void AlarmVisual();
};

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

/** サーバー専用の試合管理。開始条件・視線凍結・勝敗を判断する。 */
UCLASS()
class TOONSTORY_API ATBGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ATBGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
	                      FString& Error) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	UPROPERTY(EditDefaultsOnly)
	bool bBuildTestArena = true;
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<ATBBox> BoxClass;
	UPROPERTY(EditDefaultsOnly)
	float GazeHalfAngle = 40.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bAllowInterference = false;
	bool IsHost(const APlayerController* PlayerController) const;
	void SetRules(APlayerController* PlayerController, const FTBSettings& Rules);
	void StartRound(APlayerController* PlayerController);
	void CheckWin();
	void Finish(ETBWinner Winner, const FString& Why, bool bAbort = false);
	bool Watched(ATBCharacter* Toy, ATBCharacter* Human) const;
	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnPoints;

private:
	void BuildArena();
	void EvaluateGaze();
	int32 SpawnCursor = 0;
};

/** 入力とコンソールコマンドを、試合管理や接続処理へ渡す。 */
UCLASS()
class TOONSTORY_API ATBController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	UFUNCTION(Exec)
	void TBHost();
	UFUNCTION(Exec)
	void TBFind();
	UFUNCTION(Exec)
	void TBJoin(int32 Index);
	UFUNCTION(Exec)
	void TBLeave();
	UFUNCTION(Exec)
	void TBReady();
	UFUNCTION(Exec)
	void TBStart();
	UFUNCTION(Exec)
	void TBRules(int32 Humans, int32 Toys, float Seconds = 600.f, int32 Items = 5);
	UFUNCTION(Exec)
	void TBPrefer(int32 Team);
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerReady();
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerStart();
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerRules(FTBSettings Rules);
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerPreference(ETBTeam Team);
	UFUNCTION(Client, Reliable)
	void ClientNotice(const FString& Message);
	UFUNCTION(Client, Reliable)
	void ClientDisband(const FString& Message);
	UPROPERTY(BlueprintReadOnly)
	FString Notice;
};

/** 同期された状態を読み取り、操作説明と進捗を画面に表示する。 */
UCLASS()
class TOONSTORY_API ATBHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
