#include "Core/ToyBoxGameMode.h"

#include "Character/ToyCharacter.h"
#include "Combat/ToyBoxActor.h"
#include "Core/ToyBoxGameState.h"
#include "Core/ToyBoxPlayerController.h"
#include "Core/ToyBoxPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	/** 陣営抽選の重み。希望が最優先、前マッチの陣営はその中の並べ替えに効く。 */
	constexpr float PreferenceWeight = 10.f;
	constexpr float RepeatHumanPenalty = 5.f;

	/**
	 * 「人間に向いている度」。大きいほど人間になりやすい。
	 * 乱数を 0..1 で混ぜてあるので、同条件の人が並んだときは抽選になる。
	 */
	float ScoreForHuman(const AToyBoxPlayerState& PS)
	{
		float Score = FMath::FRand();

		if (PS.PreferredTeam == ETeamId::Human)
		{
			Score += PreferenceWeight;
		}
		else if (PS.PreferredTeam == ETeamId::Toy)
		{
			Score -= PreferenceWeight;
		}

		if (PS.bWasHumanLastMatch)
		{
			Score -= RepeatHumanPenalty;
		}

		return Score;
	}
}

AToyBoxGameMode::AToyBoxGameMode()
{
	GameStateClass = AToyBoxGameState::StaticClass();
	PlayerStateClass = AToyBoxPlayerState::StaticClass();
	PlayerControllerClass = AToyBoxPlayerController::StaticClass();

	bStartPlayersAsSpectators = false;

	// ホストが「開始」を押すまで待つ。押さない限り WaitingToStart のまま。
	bDelayedStart = true;

	// ロビーから本編へ移るときに接続を張り直さない。
	// PlayerState は CopyProperties 経由で引き継がれる。
	bUseSeamlessTravel = true;
}

AToyBoxGameState* AToyBoxGameMode::GetToyBoxGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AToyBoxGameState>() : nullptr;
}

void AToyBoxGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HostController.IsValid())
	{
		HostController = NewPlayer;
		UE_LOG(LogToyBox, Log, TEXT("ホストを %s に設定した"), *GetNameSafe(NewPlayer));
	}

	// ロビーの既定値を全員に配る。ホストが変更したら TryApplySettings で上書きされる。
	if (AToyBoxGameState* GS = GetToyBoxGameState())
	{
		if (GS->Phase == EMatchPhase::Lobby)
		{
			GS->SetSettings(DefaultSettings);
		}
	}
}

UClass* AToyBoxGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const AToyBoxPlayerState* PS = InController ? InController->GetPlayerState<AToyBoxPlayerState>() : nullptr;

	if (PS)
	{
		if (PS->TeamId == ETeamId::Human && HumanPawnClass)
		{
			return HumanPawnClass;
		}
		if (PS->TeamId == ETeamId::Toy && ToyPawnClass)
		{
			return ToyPawnClass;
		}
	}

	// ロビー中など陣営が決まる前は既定の Pawn で待たせる。
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AToyBoxGameMode::Logout(AController* Exiting)
{
	if (HostController.Get() == Exiting)
	{
		// リッスンサーバーである以上、ホストが落ちたら全員解散は避けられない
		// （仕様書「7. 解散の扱い」）。せめて理由は伝えられるように旗を立てる。
		UE_LOG(LogToyBox, Warning, TEXT("ホスト %s が抜けた"), *GetNameSafe(Exiting));
		HostController.Reset();

		if (AToyBoxGameState* GS = GetToyBoxGameState())
		{
			GS->SetHostLeft(true);
		}
	}

	// 進行中に抜けたおもちゃは、その場に残して無防備にする。
	// 一時的に退場させるより実装が単純で、待つ側にも状況が分かる（仕様書「7.」）。
	LeaveDisconnectedToyInWorld(Exiting);

	// AGameMode::Logout が PlayerState を InactivePlayerArray へ退避する。
	// 復帰時は FindInactivePlayer が拾い、OverrideWith で陣営と状態が戻る。
	Super::Logout(Exiting);

	// Super の後に見る。PlayerArray から抜けたあとでないと本人を数えてしまう。
	EvaluateWinConditions();
}

bool AToyBoxGameMode::IsHost(const AController* Controller) const
{
	return Controller != nullptr && HostController.Get() == Controller;
}

bool AToyBoxGameMode::TryApplySettings(AController* Requester, const FMatchSettings& NewSettings)
{
	AToyBoxGameState* GS = GetToyBoxGameState();
	if (!GS || !IsHost(Requester) || GS->Phase != EMatchPhase::Lobby)
	{
		return false;
	}

	GS->SetSettings(NewSettings);
	return true;
}

bool AToyBoxGameMode::TryStartMatch(AController* Requester)
{
	AToyBoxGameState* GS = GetToyBoxGameState();
	if (!GS)
	{
		return false;
	}

	if (!IsHost(Requester))
	{
		UE_LOG(LogToyBox, Warning, TEXT("ホスト以外 %s が開始を要求した。無視する"), *GetNameSafe(Requester));
		return false;
	}

	if (GS->Phase != EMatchPhase::Lobby)
	{
		return false;
	}

	if (GS->PlayerArray.Num() == 0)
	{
		return false;
	}

	AssignTeams();

	// エンジン側の MatchState も進めておく。
	// 表示用のフェーズは EMatchPhase 側を正とし、こちらは土台としてだけ使う。
	if (GetMatchState() == MatchState::WaitingToStart)
	{
		StartMatch();
	}

	GS->SetMatchEndServerTime(static_cast<float>(GS->GetServerWorldTimeSeconds()) + GS->Settings.MatchDuration);
	GS->SetResult(EMatchResult::None);
	GS->SetPhase(EMatchPhase::InProgress);

	// 残り時間の表示は GameState のサーバー時刻から引くが、
	// 実際に切る判断はサーバーのタイマー1本に任せる。
	GetWorldTimerManager().SetTimer(
		MatchTimerHandle,
		FTimerDelegate::CreateUObject(this, &AToyBoxGameMode::OnMatchTimeExpired),
		GS->Settings.MatchDuration,
		false);

	UE_LOG(LogToyBox, Log, TEXT("マッチ開始。人間 %d / おもちゃ %d、制限時間 %.0f 秒"),
		GS->CountPlayersOnTeam(ETeamId::Human),
		GS->CountPlayersOnTeam(ETeamId::Toy),
		GS->Settings.MatchDuration);

	return true;
}

void AToyBoxGameMode::LeaveDisconnectedToyInWorld(AController* Exiting)
{
	const AToyBoxGameState* GS = GetToyBoxGameState();
	if (!GS || GS->Phase != EMatchPhase::InProgress || !Exiting)
	{
		return;
	}

	const AToyBoxPlayerState* PS = Exiting->GetPlayerState<AToyBoxPlayerState>();
	if (!PS || PS->TeamId != ETeamId::Toy)
	{
		return;
	}

	APawn* Pawn = Exiting->GetPawn();
	if (!Pawn)
	{
		return;
	}

	// UnPossess しておかないと、コントローラーの破棄に巻き込まれて Pawn ごと消える。
	Exiting->UnPossess();
	Pawn->SetOwner(nullptr);
}

void AToyBoxGameMode::NotifyToyStateChanged(AToyBoxPlayerState* ToyPlayerState)
{
	// 収容されたら箱の中へ移す。箱が置かれていないレベルでも動くようにしておく。
	if (ToyPlayerState && ToyPlayerState->ToyState == EToyState::Boxed)
	{
		if (AToyBoxActor* Box = AToyBoxActor::GetPrimaryBox(this))
		{
			Box->StoreToy(Cast<AToyCharacter>(ToyPlayerState->GetPawn()));
		}
	}

	EvaluateWinConditions();
}

void AToyBoxGameMode::EvaluateWinConditions()
{
	AToyBoxGameState* GS = GetToyBoxGameState();
	if (!GS || GS->Phase != EMatchPhase::InProgress)
	{
		return;
	}

	// 人間の勝利は Boxed だけを数える。
	// 運搬中（Carried / Storing）を数に入れると、最後の1人を担いだ瞬間に
	// 試合が終わり、救助の見せ場が消える（仕様書「6.」）。
	const int32 NumToys = GS->CountPlayersOnTeam(ETeamId::Toy);
	if (NumToys > 0 && GS->CountFreeToys() == 0)
	{
		FinishMatch(EMatchResult::HumanWin_AllToysBoxed);
		return;
	}

	// おもちゃ側: アイテムが必要数に到達。
	if (GS->Settings.RequiredItems > 0 && GS->CollectedItems >= GS->Settings.RequiredItems)
	{
		FinishMatch(EMatchResult::ToyWin_ItemsCollected);
		return;
	}

	// おもちゃ側: 人間が全員離脱。
	if (GS->CountPlayersOnTeam(ETeamId::Human) == 0)
	{
		FinishMatch(EMatchResult::ToyWin_HumansLeft);
	}
}

void AToyBoxGameMode::OnMatchTimeExpired()
{
	// 制限時間切れはおもちゃ側の勝利。
	FinishMatch(EMatchResult::ToyWin_TimeUp);
}

void AToyBoxGameMode::NotifyItemCollected(int32 NewTotal)
{
	if (AToyBoxGameState* GS = GetToyBoxGameState())
	{
		GS->SetCollectedItems(NewTotal);
	}

	EvaluateWinConditions();
}

void AToyBoxGameMode::FinishMatch(EMatchResult NewResult)
{
	AToyBoxGameState* GS = GetToyBoxGameState();
	if (!GS || GS->Phase != EMatchPhase::InProgress)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(MatchTimerHandle);

	GS->SetResult(NewResult);
	GS->SetPhase(EMatchPhase::PostMatch);

	// エンジン側の MatchState も合わせて進める。
	if (GetMatchState() == MatchState::InProgress)
	{
		EndMatch();
	}

	UE_LOG(LogToyBox, Log, TEXT("決着: %s"), *UEnum::GetValueAsString(NewResult));
}

void AToyBoxGameMode::AssignTeams()
{
	AToyBoxGameState* GS = GetToyBoxGameState();
	if (!GS)
	{
		return;
	}

	TArray<AToyBoxPlayerState*> Players;
	Players.Reserve(GS->PlayerArray.Num());
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (AToyBoxPlayerState* ToyPS = Cast<AToyBoxPlayerState>(PS))
		{
			// 観戦者は陣営を持たない。
			if (!ToyPS->IsOnlyASpectator())
			{
				Players.Add(ToyPS);
			}
		}
	}

	if (Players.Num() == 0)
	{
		return;
	}

	// 設定どおりの人数比を取れるとは限らない。全員が人間になる事態だけは避ける。
	// 1 人しかいない場合はソロ検証用に人間 1 人とする。
	const int32 MaxHumans = (Players.Num() == 1) ? 1 : Players.Num() - 1;
	const int32 NumHumans = FMath::Clamp(GS->Settings.NumHumans, 1, MaxHumans);

	if (NumHumans != GS->Settings.NumHumans)
	{
		UE_LOG(LogToyBox, Warning,
			TEXT("人数が足りないので人間を %d 人に丸めた（設定は %d 人、参加者 %d 人）"),
			NumHumans, GS->Settings.NumHumans, Players.Num());
	}

	// ScoreForHuman は乱数を含むので、比較のたびに呼ぶと並び順が壊れる。
	// 先に一度だけ評価して、その値で並べる。
	struct FCandidate
	{
		AToyBoxPlayerState* PlayerState;
		float Score;
	};

	TArray<FCandidate> Candidates;
	Candidates.Reserve(Players.Num());
	for (AToyBoxPlayerState* PS : Players)
	{
		Candidates.Add(FCandidate{ PS, ScoreForHuman(*PS) });
	}

	// 「人間に向いている度」の降順に並べ、上から NumHumans 人を人間にする。
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		return A.Score > B.Score;
	});

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		AToyBoxPlayerState* PS = Candidates[Index].PlayerState;
		const ETeamId Assigned = (Index < NumHumans) ? ETeamId::Human : ETeamId::Toy;

		PS->SetTeamId(Assigned);
		PS->SetToyState(EToyState::Free);
		PS->SetFrozen(false);

		// 次のマッチの抽選で「連続して人間」を避けるための記録。
		PS->bWasHumanLastMatch = (Assigned == ETeamId::Human);

		UE_LOG(LogToyBox, Verbose, TEXT("%s -> %s"),
			*PS->GetPlayerName(),
			Assigned == ETeamId::Human ? TEXT("人間") : TEXT("おもちゃ"));
	}
}
