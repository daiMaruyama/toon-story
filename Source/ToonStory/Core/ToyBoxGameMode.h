#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/ToyBoxTypes.h"
#include "ToyBoxGameMode.generated.h"

class AToyBoxGameState;
class AToyBoxPlayerState;

/**
 * サーバーにしか存在しない審判役。
 *
 * 陣営の割り振り、勝敗判定、ログイン／ログアウト処理を持つ。
 * クライアントは「開始したい」と要求するだけで、成立させる権利は持たない
 * （仕様書「0. 確定した仕様 - 設計上いちばん大事な原則」）。
 */
UCLASS()
class AToyBoxGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AToyBoxGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/**
	 * ホストが「開始」を押したときに呼ぶ。要求元がホストでなければ何もしない。
	 * @return 実際に開始したか。
	 */
	bool TryStartMatch(AController* Requester);

	/** ホストがロビーで人数比などを変えたときに呼ぶ。 */
	bool TryApplySettings(AController* Requester, const FMatchSettings& NewSettings);

	/** リッスンサーバーを立てた本人か。最初にログインしたコントローラーをホストとして扱う。 */
	UFUNCTION(BlueprintPure, Category = "ToyBox")
	bool IsHost(const AController* Controller) const;

protected:
	/**
	 * ロビーの全員に陣営を配る。
	 *
	 * ログイン順に割り当てると人数が揃う前に比率が崩れるので、
	 * 開始を押した瞬間にまとめて配る（仕様書「3. 割り振りのタイミング」）。
	 */
	void AssignTeams();

	/** 既定のマッチ設定。BP 側で差し替えられる。 */
	UPROPERTY(EditDefaultsOnly, Category = "ToyBox")
	FMatchSettings DefaultSettings;

private:
	AToyBoxGameState* GetToyBoxGameState() const;

	/** 最初にログインしたコントローラー。ホスト判定にだけ使う。 */
	TWeakObjectPtr<AController> HostController;
};
