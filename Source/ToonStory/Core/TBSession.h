#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Engine/EngineBaseTypes.h"
#include "TimerManager.h"
#include "TBSession.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTBSessionChanged);
class UNetDriver;

/** 部屋の作成・検索・参加・退出を管理する。非同期操作は同時に1つだけ実行する。 */
UCLASS()
class TOONSTORY_API UTBSession : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable)
	void Activate();
	UFUNCTION(BlueprintCallable)
	void Host();
	UFUNCTION(BlueprintCallable)
	void Find();
	UFUNCTION(BlueprintCallable)
	void Join(int32 Index);
	UFUNCTION(BlueprintCallable)
	void Leave();
	// 試合開始時に検索・招待からの新規参加を停止する（サーバー専用）。
	void CloseLobby();
	UFUNCTION(BlueprintPure)
	bool IsBusy() const
	{
		return CurrentOperation != ESessionOperation::Idle;
	}
	UPROPERTY(BlueprintReadOnly)
	FString Status = TEXT("Offline prototype. TBHost to host; TBFind to search.");
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Rooms;
	UPROPERTY(BlueprintAssignable)
	FTBSessionChanged Changed;

private:
	enum class ESessionOperation : uint8
	{
		Idle,
		Creating,
		Finding,
		Joining,
		Closing
	};
	ESessionOperation CurrentOperation = ESessionOperation::Idle;
	IOnlineSessionPtr Sessions;
	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FOnlineSessionSearchResult> Results;
	// 登録した完了通知を、終了時に解除するための識別子。
	FDelegateHandle CreateSessionHandle, FindSessionsHandle, JoinSessionHandle, DestroySessionHandle,
	    InviteAcceptedHandle, NetworkFailureHandle, TravelFailureHandle, UpdateSessionHandle;
	FTimerHandle CloseTimer;
	FTimerHandle LobbyUpdateTimer;
	void LobbyUpdated(FName Name, bool bSucceeded);
	bool Acquire();
	void Message(const FString& Text);
	void Created(FName Name, bool bSucceeded);
	void Found(bool bSucceeded);
	void Joined(FName Name, EOnJoinSessionCompleteResult::Type Result);
	void Destroyed(FName Name, bool bSucceeded);
	void Accepted(bool bSucceeded, int32 LocalUser, TSharedPtr<const FUniqueNetId> Id,
	              const FOnlineSessionSearchResult& Result);
	void BeginJoin(const FOnlineSessionSearchResult& Result);
	void NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void TravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void ReturnOffline();
};
