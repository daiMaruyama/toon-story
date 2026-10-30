// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/TBTypes.h"
#include "Audio/TBAudioTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "TBController.generated.h"

class UInputMappingContext;

/** 入力とコンソールコマンドを、試合管理や接続処理へ渡す。 */
UCLASS()
class TOONSTORY_API ATBController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ClientWasKicked_Implementation(const FText& KickReason) override;
	UFUNCTION(Server, Reliable)
	void ServerKickPlayer(int32 PlayerId);
	UFUNCTION(Server, Reliable)
	void ServerUnstuck();
	bool CanRecover() const;
	bool FindRecoveryLocation(FVector& Destination) const;
	UPROPERTY(Replicated)
	bool bStuckHint = false;
	// サーバーだけで使う待機完了時刻。表示用の同期は不要。
	double RecoveryEnds = 0;

	virtual void PostSeamlessTravel() override;
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
	UFUNCTION(Client, Reliable)
	void ClientRoomClosed();
	virtual void PlayerTick(float DeltaTime) override;
	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	virtual void SetupInputComponent() override;
	virtual void ReceivedPlayer() override;
	bool IsLeaveMenuOpen() const
	{
		return bLeaveMenuOpen;
	}
	bool ShouldHidePreparationView() const;
	bool IsShowingEntranceApproach() const { return bEntranceApproach; }
	UFUNCTION(Client, Reliable)
	void ClientPreparationRole(uint32 Round, ETBTeam Team);
	UFUNCTION(Client, Reliable)
	void ClientPreparationBegin(uint32 Round);
	UFUNCTION(Client, Reliable)
	void ClientPreparationCancelled(uint32 Round);
	UFUNCTION(Client, Reliable)
	void ClientPreparationTravelCancelled();
	void ToggleLeaveMenu();
	/** 陣営に合わせてInput Mapping Contextを付け替える。設定が空なら何もしない */
	void SyncInputContexts();
	UFUNCTION(Exec)
	void TBHost();
	UFUNCTION(Exec)
	void TBFind();
	UFUNCTION(Exec)
	void TBJoin(int32 Index);
	UFUNCTION(Exec)
	void TBLeave();
	UFUNCTION(Exec)
	void TBQuit();
	UFUNCTION(Exec)
	void TBLobby();
	UFUNCTION(Server, Reliable)
	void ServerReturnToLobby();
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
	UFUNCTION(Server, Reliable)
	void ServerSelectStage(int32 Index);
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerPreference(ETBTeam Team);
	UFUNCTION(Client, Reliable)
	void ClientNotice(const FString& Message, bool bError = true);
	UFUNCTION(Client, Reliable)
	void ClientPlaySoundEvent(ETBSoundEvent Event, FVector Location);
	UPROPERTY(BlueprintReadOnly)
	FString Notice;
	// この人の画面比（横÷縦）。本人のPCが測り、基準より横長なら黒帯を出す。サーバーにも送り、画面が狭い人の
	// 視界の判定を狭めるのに使う。0 はまだ測っていない（届いていない）ことを表し、基準の画面比として扱う。
	float ViewAspect = 0.f;
	UFUNCTION(Server, Reliable)
	void ServerSetViewAspect(float Aspect);
	// チャット。開く・送るはEnter、閉じるはEsc。キー設定には出さない。
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	bool CanUseChat() const;
	bool IsChatOpen() const;
	void OpenChat();
	void CloseChat();
	/** 間隔が短すぎるときは送らずにfalseを返す。入力欄は文を残す。 */
	bool SendChat(const FString& Text);
	UFUNCTION(Server, Reliable)
	void ServerSendChat(const FString& Text);
	UFUNCTION(Client, Reliable)
	void ClientReceiveChat(const FString& Name, const FString& Text, int32 SenderId);

private:
	void UpdateChat();
	UPROPERTY(Transient)
	TObjectPtr<class UTBChatBox> ChatBox;
	// どちらもFPlatformTime::Seconds。送った側とサーバーで別々に数える。
	double LastChatSent = -1;
	double LastChatAccepted = -1;
	void ClearInputContexts();
	TArray<TWeakObjectPtr<const UInputMappingContext>> AppliedContexts;
	bool bPreparationTravelMask = false;
	bool bPreparationStartMask = false;
	bool bPreparationBlack = false;
	bool bEntranceApproach = false;
	TWeakObjectPtr<class ACameraActor> EntranceCamera;
	double NextEntranceFootstep = 0;
	bool bPreparationTravelFailed = false;
	uint32 PreparationStartRound = 0;
	FDelegateHandle PreparationTravelFailureHandle;
	void PreparationTravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	uint32 PreparationViewRound = 0;
	uint32 LastCompletedPreparationRound = 0;
	ETBTeam PreparationViewTeam = ETBTeam::None;
	void UpdateViewAspect();
	void TickRecovery();
	TWeakObjectPtr<APawn> RecoveryPawn;
	TWeakObjectPtr<UWorld> RecoveryWorld;
	FVector RecoveryOrigin = FVector::ZeroVector;
	FVector SampleOrigin = FVector::ZeroVector;
	FVector StuckOrigin = FVector::ZeroVector;
	ETBPhase RecoveryPhase = ETBPhase::Lobby;
	float BlockedSeconds = 0;
	double HintUntil = 0;
	double RecoveryCooldown = 0;
	double RecoverySampleAt = 0;
	void UpdateScreenTransition();
	// 直前のフレームの状態。変わった瞬間に演出を出す。
	ETBPhase LastPhase = ETBPhase::Lobby;
	ETBToyState LastToyState = ETBToyState::Free;
	void UpdateLobbyCamera(bool InLobby);
	void UpdateMenuInput(bool ShowMenu);
	bool bLobbyInput = false;
	double NoticeExpires = 0;
	bool bLeaveMenuOpen = false;
	bool bLobbyCameraActive = false;
	bool bLobbyCameraSearched = false;
	TWeakObjectPtr<AActor> LobbyCamera;
};
