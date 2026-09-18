#include "Core/BatteryTagGameState.h"
#include "Net/UnrealNetwork.h"

void ABatteryTagGameState::SetProgress(const FBatteryTagProgress& NewProgress, bool bNotify)
{
	if (!HasAuthority()) return;
	Progress = NewProgress;
	ForceNetUpdate();
	if (bNotify) OnRep_Progress();
}

void ABatteryTagGameState::OnRep_Progress() { OnProgressChanged.Broadcast(); }

void ABatteryTagGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABatteryTagGameState, Progress);
}
