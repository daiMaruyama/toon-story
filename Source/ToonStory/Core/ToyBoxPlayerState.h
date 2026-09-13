#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Core/ToyBoxTypes.h"
#include "ToyBoxPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamIdChanged, ETeamId, NewTeamId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToyStateChanged, EToyState, NewToyState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFrozenChanged, bool, bNewFrozen);

/**
 * 陣営と捕獲状態の置き場所。
 *
 * Pawn ではなく PlayerState に持たせる（仕様書「1. 全体構成とクラス設計」）。
 * Pawn に持たせると、捕獲で Pawn を差し替えたときやリスポーン時に情報が消える。
 * また Seamless Travel では CopyProperties() を override しない限りリセットされる。
 *
 * 値を変えてよいのはサーバーだけ。クライアントは OnRep_ 経由で追従する。
 */
UCLASS()
class AToyBoxPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AToyBoxPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seamless Travel で陣営と抽選の重みを引き継ぐ。ToyState は新マッチで初期化するので写さない。 */
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	/**
	 * 切断からの復帰で、元の PlayerState の内容を引き継ぐ。
	 *
	 * CopyProperties（Seamless Travel 用）とは別経路で、
	 * AGameMode::FindInactivePlayer から呼ばれる。こちらは進行中のマッチへ
	 * 戻ってくる場面なので、ToyState も含めて丸ごと戻す。
	 */
	virtual void OverrideWith(APlayerState* OldPlayerState) override;

	/** 陣営。 */
	UPROPERTY(ReplicatedUsing = OnRep_TeamId, BlueprintReadOnly, Category = "ToyBox")
	ETeamId TeamId = ETeamId::Unassigned;

	/** 捕獲状態。 */
	UPROPERTY(ReplicatedUsing = OnRep_ToyState, BlueprintReadOnly, Category = "ToyBox")
	EToyState ToyState = EToyState::Free;

	/**
	 * 視線凍結中か。
	 *
	 * 他人のおもちゃが止まって見える必要があるので、所有者以外にもレプリケートする。
	 * 書き込むのは UGazeFreezeSubsystem（M2 で実装）。それまでは常に false のまま。
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Frozen, BlueprintReadOnly, Category = "ToyBox")
	bool bFrozen = false;

	/** ロビーで受け付けた希望陣営。定員を超えた分は抽選になる。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "ToyBox")
	ETeamId PreferredTeam = ETeamId::Unassigned;

	/**
	 * 前のマッチで人間だったか。連続して同じ陣営にならないよう抽選の重みに使う。
	 * サーバーだけが読む値なのでレプリケートしない。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ToyBox")
	bool bWasHumanLastMatch = false;

	/** UI から bind する。サーバー（リッスンサーバーのホスト分も含む）でも発火する。 */
	UPROPERTY(BlueprintAssignable, Category = "ToyBox")
	FOnTeamIdChanged OnTeamIdChanged;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox")
	FOnToyStateChanged OnToyStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "ToyBox")
	FOnFrozenChanged OnFrozenChanged;

	UFUNCTION(BlueprintPure, Category = "ToyBox")
	bool IsHuman() const { return TeamId == ETeamId::Human; }

	UFUNCTION(BlueprintPure, Category = "ToyBox")
	bool IsToy() const { return TeamId == ETeamId::Toy; }

	/** 箱の中にいるか。人間の勝利判定はこれだけを数える（仕様書「6.」）。 */
	UFUNCTION(BlueprintPure, Category = "ToyBox")
	bool IsBoxed() const { return ToyState == EToyState::Boxed; }

	/** 以下 3 つはサーバー専用。クライアントから呼ばれても何もしない。 */
	void SetTeamId(ETeamId NewTeamId);
	void SetToyState(EToyState NewToyState);
	void SetFrozen(bool bNewFrozen);

	/** サーバー専用。ロビーでの希望を記録する。 */
	void SetPreferredTeam(ETeamId NewPreferredTeam);

protected:
	UFUNCTION()
	void OnRep_TeamId();

	UFUNCTION()
	void OnRep_ToyState();

	UFUNCTION()
	void OnRep_Frozen();
};
