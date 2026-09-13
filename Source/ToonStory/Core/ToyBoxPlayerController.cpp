#include "Core/ToyBoxPlayerController.h"

#include "Core/ToyBoxGameMode.h"
#include "Core/ToyBoxGameState.h"
#include "Core/ToyBoxPlayerState.h"
#include "Engine/World.h"

ETeamId AToyBoxPlayerController::GetTeamId() const
{
	const AToyBoxPlayerState* PS = GetPlayerState<AToyBoxPlayerState>();
	return PS ? PS->TeamId : ETeamId::Unassigned;
}

bool AToyBoxPlayerController::IsLocalHost() const
{
	// リッスンサーバーのホストだけが「権限を持つローカルコントローラー」になる。
	return HasAuthority() && IsLocalController();
}

void AToyBoxPlayerController::RequestPreferredTeam(ETeamId Team)
{
	ServerSetPreferredTeam(Team);
}

void AToyBoxPlayerController::RequestStartMatch()
{
	ServerRequestStartMatch();
}

void AToyBoxPlayerController::RequestApplySettings(const FMatchSettings& NewSettings)
{
	ServerApplySettings(NewSettings);
}

void AToyBoxPlayerController::ServerSetPreferredTeam_Implementation(ETeamId Team)
{
	// 希望として受け付けるのは 人間 / おもちゃ / おまかせ の3通りだけ。
	if (AToyBoxPlayerState* PS = GetPlayerState<AToyBoxPlayerState>())
	{
		PS->SetPreferredTeam(Team);
	}
}

void AToyBoxPlayerController::ServerRequestStartMatch_Implementation()
{
	if (AToyBoxGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AToyBoxGameMode>() : nullptr)
	{
		GM->TryStartMatch(this);
	}
}

void AToyBoxPlayerController::ServerApplySettings_Implementation(FMatchSettings NewSettings)
{
	if (AToyBoxGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AToyBoxGameMode>() : nullptr)
	{
		GM->TryApplySettings(this, NewSettings);
	}
}

void AToyBoxPlayerController::ClientItemPickupFailed_Implementation()
{
	// 演出は BP 側で。ここでは通知が届いたことだけを示す。
	UE_LOG(LogToyBox, Verbose, TEXT("アイテムの取得に失敗（先を越された）"));
}

void AToyBoxPlayerController::ToyBoxStart()
{
	RequestStartMatch();
}

void AToyBoxPlayerController::ToyBoxTeam(int32 Team)
{
	const ETeamId Desired =
		(Team == 1) ? ETeamId::Human :
		(Team == 2) ? ETeamId::Toy :
		ETeamId::Unassigned;

	RequestPreferredTeam(Desired);
}

void AToyBoxPlayerController::ToyBoxStatus()
{
	const AToyBoxPlayerState* PS = GetPlayerState<AToyBoxPlayerState>();
	const UWorld* World = GetWorld();
	const AToyBoxGameState* GS = World ? World->GetGameState<AToyBoxGameState>() : nullptr;

	UE_LOG(LogToyBox, Log, TEXT("--- ToyBox 状態 ---"));
	UE_LOG(LogToyBox, Log, TEXT("ホストか: %s / 権限: %s"),
		IsLocalHost() ? TEXT("はい") : TEXT("いいえ"),
		HasAuthority() ? TEXT("サーバー") : TEXT("クライアント"));

	if (PS)
	{
		UE_LOG(LogToyBox, Log, TEXT("陣営: %s / 捕獲状態: %s / 凍結: %s"),
			*UEnum::GetValueAsString(PS->TeamId),
			*UEnum::GetValueAsString(PS->ToyState),
			PS->bFrozen ? TEXT("はい") : TEXT("いいえ"));
	}

	if (GS)
	{
		UE_LOG(LogToyBox, Log, TEXT("フェーズ: %s / 残り %.1f 秒 / アイテム %d / 人間 %d / おもちゃ %d（うち箱の中 %d）"),
			*UEnum::GetValueAsString(GS->Phase),
			GS->GetRemainingSeconds(),
			GS->CollectedItems,
			GS->CountPlayersOnTeam(ETeamId::Human),
			GS->CountPlayersOnTeam(ETeamId::Toy),
			GS->CountBoxedToys());
	}
}
