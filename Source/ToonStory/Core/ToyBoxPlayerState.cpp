#include "Core/ToyBoxPlayerState.h"

#include "Core/ToyBoxGameMode.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AToyBoxPlayerState::AToyBoxPlayerState()
{
	// 陣営表示や凍結はこまめに更新したいので、既定より速く送る。
	// 実測で足りなければ M2 で詰める。
	SetNetUpdateFrequency(20.f);
}

void AToyBoxPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AToyBoxPlayerState, TeamId);
	DOREPLIFETIME(AToyBoxPlayerState, ToyState);
	DOREPLIFETIME(AToyBoxPlayerState, bFrozen);
	DOREPLIFETIME(AToyBoxPlayerState, PreferredTeam);
}

void AToyBoxPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	if (AToyBoxPlayerState* PS = Cast<AToyBoxPlayerState>(NewPlayerState))
	{
		PS->TeamId = TeamId;
		PS->PreferredTeam = PreferredTeam;
		PS->bWasHumanLastMatch = bWasHumanLastMatch;
		// ToyState と bFrozen は新しいマッチで初期化するので写さない。
	}
}

void AToyBoxPlayerState::SetTeamId(ETeamId NewTeamId)
{
	if (!HasAuthority() || TeamId == NewTeamId)
	{
		return;
	}

	TeamId = NewTeamId;

	// OnRep_ はサーバーでは呼ばれない。リッスンサーバーのホストにも
	// 同じ通知が要るので、ここで明示的に叩く。
	OnRep_TeamId();
}

void AToyBoxPlayerState::SetToyState(EToyState NewToyState)
{
	if (!HasAuthority() || ToyState == NewToyState)
	{
		return;
	}

	ToyState = NewToyState;
	OnRep_ToyState();

	// 勝敗の評価はここが唯一の入口。状態を変えた側が呼び忘れる余地を作らない。
	if (UWorld* World = GetWorld())
	{
		if (AToyBoxGameMode* GM = World->GetAuthGameMode<AToyBoxGameMode>())
		{
			GM->NotifyToyStateChanged(this);
		}
	}
}

void AToyBoxPlayerState::SetFrozen(bool bNewFrozen)
{
	if (!HasAuthority() || bFrozen == bNewFrozen)
	{
		return;
	}

	bFrozen = bNewFrozen;
	OnRep_Frozen();
}

void AToyBoxPlayerState::SetPreferredTeam(ETeamId NewPreferredTeam)
{
	if (!HasAuthority())
	{
		return;
	}

	PreferredTeam = NewPreferredTeam;
}

void AToyBoxPlayerState::OnRep_TeamId()
{
	OnTeamIdChanged.Broadcast(TeamId);
}

void AToyBoxPlayerState::OnRep_ToyState()
{
	OnToyStateChanged.Broadcast(ToyState);
}

void AToyBoxPlayerState::OnRep_Frozen()
{
	OnFrozenChanged.Broadcast(bFrozen);
}
