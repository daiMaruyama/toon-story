#include "TBController.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Core/TBSession.h"
#include "Core/TBGameHelpers.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void ATBController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
		{
			Session->Activate();
		}
	}
}

void ATBController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction("Ready", IE_Pressed, this, &ATBController::TBReady);
	InputComponent->BindAction("Start", IE_Pressed, this, &ATBController::TBStart);
}

void ATBController::TBHost()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Host();
	}
}

void ATBController::TBFind()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Find();
	}
}

void ATBController::TBJoin(int32 Index)
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Join(Index);
	}
}

void ATBController::TBLeave()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Leave();
	}
}

void ATBController::TBReady()
{
	ServerReady();
}

void ATBController::TBStart()
{
	ServerStart();
}

void ATBController::TBRules(int32 HumanCount, int32 ToyCount, float Seconds, int32 Items)
{
	FTBSettings Settings;
	Settings.Humans = HumanCount;
	Settings.Toys = ToyCount;
	Settings.Duration = Seconds;
	Settings.RequiredItems = Items;
	ServerRules(Settings);
}

void ATBController::TBPrefer(int32 Team)
{
	ServerPreference(Team == 1 ? ETBTeam::Human : Team == 2 ? ETBTeam::Toy : ETBTeam::None);
}

void ATBController::ServerReady_Implementation()
{
	auto* MatchInfo = TB::GS(GetWorld());
	auto* ToyPlayerInfo = GetPlayerState<ATBPlayerState>();
	if (MatchInfo && MatchInfo->Phase == ETBPhase::Lobby && ToyPlayerInfo)
	{
		ToyPlayerInfo->bReady = !ToyPlayerInfo->bReady;
		ToyPlayerInfo->ForceNetUpdate();
	}
}

void ATBController::ServerStart_Implementation()
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->StartRound(this);
	}
}

void ATBController::ServerRules_Implementation(FTBSettings Rules)
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->SetRules(this, Rules);
	}
}

void ATBController::ServerPreference_Implementation(ETBTeam Team)
{
	auto* MatchInfo = TB::GS(GetWorld());
	auto* ToyPlayerInfo = GetPlayerState<ATBPlayerState>();
	if (MatchInfo && MatchInfo->Phase == ETBPhase::Lobby && ToyPlayerInfo &&
	    (Team == ETBTeam::None || Team == ETBTeam::Human || Team == ETBTeam::Toy))
	{
		ToyPlayerInfo->Preference = Team;
		ToyPlayerInfo->bReady = false;
	}
}

void ATBController::ClientNotice_Implementation(const FString& Message)
{
	Notice = Message;
}
