#include "TBController.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Core/TBSession.h"
#include "Core/TBGameHelpers.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"

void ATBController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController())
	{
		return;
	}
	const auto* State = TB::GS(GetWorld());
	const bool InLobby = State && State->Phase == ETBPhase::Lobby;
	if (!State || State->Phase != ETBPhase::Playing) bLeaveMenuOpen = false;
	const bool ShowMenu = State && (State->Phase != ETBPhase::Playing || bLeaveMenuOpen);
	UpdateLobbyCamera(InLobby);
	UpdateMenuInput(ShowMenu);
}

void ATBController::UpdateLobbyCamera(bool InLobby)
{
	// ロビー中はマップに置いたTBLobbyCameraから見せる。Pawnの所持で視点が戻されても毎フレーム掛け直す。
	if (InLobby)
	{
		if (!bLobbyCameraSearched)
		{
			bLobbyCameraSearched = true;
			for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
			{
				if (It->ActorHasTag(TEXT("TBLobbyCamera")))
				{
					LobbyCamera = *It;
					break;
				}
			}
		}
		if (LobbyCamera.IsValid() && GetViewTarget() != LobbyCamera.Get())
		{
			SetViewTarget(LobbyCamera.Get());
			bLobbyCameraActive = true;
		}
	}
	else if (bLobbyCameraActive && GetPawn())
	{
		SetViewTargetWithBlend(GetPawn(), .35f);
		bLobbyCameraActive = false;
	}
}

void ATBController::UpdateMenuInput(bool ShowMenu)
{
	if (bLobbyInput != ShowMenu)
	{
		bLobbyInput = ShowMenu;
		bShowMouseCursor = ShowMenu;
		bEnableClickEvents = ShowMenu;
		SetIgnoreLookInput(ShowMenu);
		SetIgnoreMoveInput(ShowMenu);
		if (ShowMenu)
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			SetInputMode(Mode);
		}
		else
		{
			SetInputMode(FInputModeGameOnly());
		}
	}
}

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
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ATBController::ToggleLeaveMenu);
	InputComponent->BindAction("Ready", IE_Pressed, this, &ATBController::TBReady);
	InputComponent->BindAction("Start", IE_Pressed, this, &ATBController::TBStart);
}

void ATBController::ToggleLeaveMenu()
{
	const auto* State = TB::GS(GetWorld());
	if (State && State->Phase == ETBPhase::Playing) bLeaveMenuOpen = !bLeaveMenuOpen;
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

void ATBController::TBLobby()
{
	ServerReturnToLobby();
}

void ATBController::ServerReturnToLobby_Implementation()
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->ReturnToLobby(this);
	}
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
