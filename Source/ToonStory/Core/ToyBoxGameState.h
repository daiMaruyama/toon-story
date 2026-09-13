#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Core/ToyBoxTypes.h"
#include "ToyBoxGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchPhaseChanged, EMatchPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchSettingsChanged, FMatchSettings, NewSettings);

/**
 * 全員が見てよいマッチ情報。
 *
 * 残り時間は「各クライアントが独立にカウントダウンする」と必ずずれるので、
 * 終了予定のサーバー時刻を 1 つだけ置き、クライアントは差分を表示する
 * （仕様書「6. タイマーと勝敗判定」）。
 */
UCLASS()
class AToyBoxGameState : public AGameState
{
	GENERATED_BODY()

public:
	AToyBoxGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Settings, BlueprintReadOnly, Category = "ToyBox")
	FMatchSettings Settings;

	UPROPERTY(ReplicatedUsing = OnRep_Phase, BlueprintReadOnly, Category = "ToyBox")
	EMatchPhase Phase = EMatchPhase::Lobby;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ToyBox")
	EMatchResult Result = EMatchResult::None;

	/** 終了予定のサーバー時刻。GetRemainingSeconds() 越しに読むこと。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ToyBox")
	float MatchEndServerTime = 0.f;

	/** おもちゃ側が集めたアイテム数。全員が進捗を見られるべき情報なので GameState に置く。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ToyBox")
	int32 CollectedItems = 0;

	/**
	 * ホストが抜けたか。
	 *
	 * リッスンサーバーである以上、ホストが落ちたら全員解散は避けられない。
	 * できるのは黒画面で放り出さないことだけなので、理由を出すための旗を立てる
	 * （仕様書「7. 解散の扱い」）。
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ToyBox")
	bool bHostLeft = false;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox")
	FOnMatchPhaseChanged OnMatchPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox")
	FOnMatchSettingsChanged OnMatchSettingsChanged;

	/** 残り秒数。0 未満にはならない。 */
	UFUNCTION(BlueprintPure, Category = "ToyBox")
	float GetRemainingSeconds() const;

	/** 箱に入っているおもちゃの数。運搬中（Carried / Storing）は数えない。 */
	UFUNCTION(BlueprintPure, Category = "ToyBox")
	int32 CountBoxedToys() const;

	/** 生存しているおもちゃ（= まだ Boxed でない）の数。 */
	UFUNCTION(BlueprintPure, Category = "ToyBox")
	int32 CountFreeToys() const;

	UFUNCTION(BlueprintPure, Category = "ToyBox")
	int32 CountPlayersOnTeam(ETeamId Team) const;

	/** 以下はサーバー専用。 */
	void SetSettings(const FMatchSettings& NewSettings);
	void SetPhase(EMatchPhase NewPhase);
	void SetResult(EMatchResult NewResult);
	void SetMatchEndServerTime(float NewEndTime);
	void SetCollectedItems(int32 NewCount);
	void SetHostLeft(bool bLeft);

protected:
	UFUNCTION()
	void OnRep_Settings();

	UFUNCTION()
	void OnRep_Phase();
};
