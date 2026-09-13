#pragma once

#include "CoreMinimal.h"
#include "ToonStoryPlayerController.h"
#include "Core/ToyBoxTypes.h"
#include "ToyBoxPlayerController.generated.h"

/**
 * 入力と UI の窓口。
 *
 * ここから出るのは「〜したい」という要求だけで、成立させるのは常にサーバー
 * （仕様書「0. 設計上いちばん大事な原則」）。
 */
UCLASS()
class AToyBoxPlayerController : public AToonStoryPlayerController
{
	GENERATED_BODY()

public:
	/** ロビーで希望陣営を選んだとき。定員を超えたら抽選になる。 */
	UFUNCTION(BlueprintCallable, Category = "ToyBox")
	void RequestPreferredTeam(ETeamId Team);

	/** ホストが「開始」を押したとき。ホスト以外が呼んでもサーバー側で弾かれる。 */
	UFUNCTION(BlueprintCallable, Category = "ToyBox")
	void RequestStartMatch();

	/** ホストがロビーで人数比などを変えたとき。 */
	UFUNCTION(BlueprintCallable, Category = "ToyBox")
	void RequestApplySettings(const FMatchSettings& NewSettings);

	/** 自分の陣営。UI から読む。 */
	UFUNCTION(BlueprintPure, Category = "ToyBox")
	ETeamId GetTeamId() const;

	/**
	 * このコントローラーがリッスンサーバーを立てた本人か。
	 * ロビーで「開始」ボタンを出すかどうかの判定に使う。
	 */
	UFUNCTION(BlueprintPure, Category = "ToyBox")
	bool IsLocalHost() const;

	// --- デバッグ用のコンソールコマンド ---
	// UI がまだ無い段階でも、~ キーのコンソールから一通り動かせるようにしておく。

	/** ToyBoxStart : ホストがマッチを開始する。 */
	UFUNCTION(Exec)
	void ToyBoxStart();

	/** ToyBoxTeam <0=おまかせ / 1=人間 / 2=おもちゃ> : 希望陣営を送る。 */
	UFUNCTION(Exec)
	void ToyBoxTeam(int32 Team);

	/** ToyBoxStatus : 自分と全体の状態をログに出す。 */
	UFUNCTION(Exec)
	void ToyBoxStatus();

	/**
	 * アイテムの取り合いに負けたときにサーバーから届く。
	 * 先着 1 人だけが成立するので、負けた側には理由を返してやる。
	 */
	UFUNCTION(Client, Reliable)
	void ClientItemPickupFailed();

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetPreferredTeam(ETeamId Team);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartMatch();

	UFUNCTION(Server, Reliable)
	void ServerApplySettings(FMatchSettings NewSettings);
};
