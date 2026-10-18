// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/TBTypes.h"
#include "Audio/TBAudioTypes.h"
#include "TBController.generated.h"

/** 入力とコンソールコマンドを、試合管理や接続処理へ渡す。 */
UCLASS()
class TOONSTORY_API ATBController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
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
	UPROPERTY(Replicated)
	double RecoveryEnds = 0;

	virtual void PostSeamlessTravel() override;
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
	UFUNCTION(Client, Reliable)
	void ClientRoomClosed();
	virtual void PlayerTick(float DeltaTime) override;
	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	virtual void SetupInputComponent() override;
	bool IsLeaveMenuOpen() const
	{
		return bLeaveMenuOpen;
	}
	void ToggleLeaveMenu();
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

private:
	void TickRecovery(float DeltaTime);
	TWeakObjectPtr<APawn> RecoveryPawn;
	TWeakObjectPtr<UWorld> RecoveryWorld;
	FVector RecoveryOrigin = FVector::ZeroVector;
	FVector SampleOrigin = FVector::ZeroVector;
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
