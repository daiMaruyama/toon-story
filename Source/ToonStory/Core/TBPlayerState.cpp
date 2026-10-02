#include "TBPlayerState.h"
#include "Character/TBCharacter.h"
#include "Net/UnrealNetwork.h"

void ATBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBPlayerState, Team);
	DOREPLIFETIME(ATBPlayerState, ToyState);
	DOREPLIFETIME(ATBPlayerState, bFrozen);
	DOREPLIFETIME(ATBPlayerState, bReady);
	DOREPLIFETIME(ATBPlayerState, Preference);
	DOREPLIFETIME(ATBPlayerState, Items);
}

void ATBPlayerState::OnRep_State()
{
	if (auto* ToyCharacter = Cast<ATBCharacter>(GetPawn()))
	{
		ToyCharacter->ApplyState();
		ToyCharacter->StateVisualChanged();
	}
}

void ATBPlayerState::CopyProperties(APlayerState* NewState)
{
	Super::CopyProperties(NewState);
	if (auto* CopiedState = Cast<ATBPlayerState>(NewState))
	{
		CopiedState->Team = Team;
		CopiedState->Preference = Preference;
	}
}
