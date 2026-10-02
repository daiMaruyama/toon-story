#include "TBGameState.h"
#include "GamePlay/TBBox.h"
#include "Net/UnrealNetwork.h"

void ATBGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBGameState, Settings);
	DOREPLIFETIME(ATBGameState, Phase);
	DOREPLIFETIME(ATBGameState, Winner);
	DOREPLIFETIME(ATBGameState, Reason);
	DOREPLIFETIME(ATBGameState, EndTime);
	DOREPLIFETIME(ATBGameState, Collected);
	DOREPLIFETIME(ATBGameState, BoxedCount);
	DOREPLIFETIME(ATBGameState, Box);
}

float ATBGameState::Remaining() const
{
	return Phase == ETBPhase::Playing ? FMath::Max(0., EndTime - GetServerWorldTimeSeconds()) : 0.f;
}
