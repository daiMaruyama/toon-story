#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/ToyBoxTypes.h"
#include "ToyBoxPlayerController.generated.h"

/**
 * 入力と UI の窓口。
 *
 * ここから出るのは「〜したい」という要求だけで、成立させるのは常にサーバー
 * （仕様書「0. 設計上いちばん大事な原則」）。
 */
UCLASS()
class AToyBoxPlayerController : public APlayerController
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

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetPreferredTeam(ETeamId Team);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartMatch();

	UFUNCTION(Server, Reliable)
	void ServerApplySettings(FMatchSettings NewSettings);
};
