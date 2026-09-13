#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ToyBoxSessionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionHostComplete, bool, bSucceeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionSearchComplete, int32, NumResults);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionJoinComplete, bool, bSucceeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionDisconnected, const FString&, Reason);

/**
 * セッションの作成・検索・参加。
 *
 * まず Null サブシステムで LAN、あとから EOS か Steam に差し替える前提（仕様書 M8）。
 * 新しい Online Services (OSSv2) は公式ドキュメント上ベータ扱いなので、
 * 実績のある OSSv1 の上に組んでいる。
 */
UCLASS()
class UToyBoxSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** リッスンサーバーを立てて、指定のマップへ ServerTravel する。 */
	UFUNCTION(BlueprintCallable, Category = "ToyBox|Session")
	void HostSession(int32 MaxPlayers, bool bUseLAN, const FString& MapPath);

	/** 参加できるセッションを探す。結果は OnSearchComplete で返る。 */
	UFUNCTION(BlueprintCallable, Category = "ToyBox|Session")
	void FindSessions(bool bUseLAN, int32 MaxResults = 20);

	/** FindSessions の結果の何番目に参加するか。 */
	UFUNCTION(BlueprintCallable, Category = "ToyBox|Session")
	void JoinFoundSession(int32 ResultIndex);

	UFUNCTION(BlueprintCallable, Category = "ToyBox|Session")
	void DestroyCurrentSession();

	/** 直近の検索で見つかったセッションの表示名。UI の一覧用。 */
	UFUNCTION(BlueprintPure, Category = "ToyBox|Session")
	TArray<FString> GetSearchResultLabels() const;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox|Session")
	FOnSessionHostComplete OnHostComplete;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox|Session")
	FOnSessionSearchComplete OnSearchComplete;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox|Session")
	FOnSessionJoinComplete OnJoinComplete;

	/**
	 * 接続が切れた理由。
	 * 黒画面で放り出さず、これを出してロビー画面へ戻すために使う（仕様書「7.」）。
	 */
	UPROPERTY(BlueprintAssignable, Category = "ToyBox|Session")
	FOnSessionDisconnected OnDisconnected;

protected:
	IOnlineSessionPtr GetSessionInterface() const;

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	void ClearDelegates();

	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	FDelegateHandle NetworkFailureHandle;

	/** 作成成功後に飛ぶ先。HostSession で受け取って覚えておく。 */
	FString PendingMapPath;

	TSharedPtr<FOnlineSessionSearch> SearchSettings;

	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
};
