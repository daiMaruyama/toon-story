#include "Core/ToonStoryPlayerState.h"
#include "Net/UnrealNetwork.h"

void AToonStoryPlayerState::SetMatchReady(bool bReady)
{
	if (!HasAuthority() || bMatchReady == bReady) return;
	bMatchReady = bReady;
	ForceNetUpdate();
	OnRep_Lobby();
}

void AToonStoryPlayerState::SetTeam(EToonTeam NewTeam)
{
	if (!HasAuthority() || Team == NewTeam) return;
	Team = NewTeam;
	ForceNetUpdate();
	OnRep_Lobby();
}

void AToonStoryPlayerState::OnRep_Lobby() { OnLobbyChanged.Broadcast(); }

void AToonStoryPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AToonStoryPlayerState, bMatchReady);
	DOREPLIFETIME(AToonStoryPlayerState, Team);
}
