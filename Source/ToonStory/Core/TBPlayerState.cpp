// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBPlayerState.h"
#include "Character/TBCharacter.h"
#include "Net/UnrealNetwork.h"

void ATBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBPlayerState, Team);
	DOREPLIFETIME(ATBPlayerState, HumanAppearance);
	DOREPLIFETIME(ATBPlayerState, ToyState);
	DOREPLIFETIME(ATBPlayerState, bFrozen);
	DOREPLIFETIME_CONDITION(ATBPlayerState, GazeState, COND_OwnerOnly);
	DOREPLIFETIME(ATBPlayerState, bReady);
	DOREPLIFETIME(ATBPlayerState, Preference);
	DOREPLIFETIME(ATBPlayerState, Items);
	DOREPLIFETIME(ATBPlayerState, bEscapedInVehicle);
}

void ATBPlayerState::SetGaze(ETBGazeState State, float Seconds)
{
	GazedSeconds = Seconds;
	if (GazeState == State) return;
	GazeState = State;
	ForceNetUpdate();
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
		CopiedState->bReady = bReady;
	}
}
