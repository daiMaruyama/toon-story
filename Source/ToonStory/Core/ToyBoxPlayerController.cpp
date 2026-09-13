#include "Core/ToyBoxPlayerController.h"

#include "Core/ToyBoxGameMode.h"
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
