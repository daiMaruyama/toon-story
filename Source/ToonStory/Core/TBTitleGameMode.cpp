#include "Core/TBTitleGameMode.h"
#include "Core/TBSession.h"
#include "UI/TBTitleMenu.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"

ATBTitleGameMode::ATBTitleGameMode()
{
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;
	// Keep the base empty HUD: ClientSetHUD attempts SpawnActor even for nullptr.
	PlayerControllerClass = ATBTitleController::StaticClass();
}

void ATBTitleController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	if (TActorIterator<ACameraActor> It(GetWorld()); It)
	{
		SetViewTarget(*It);
	}
	GetGameInstance()->GetSubsystem<UTBSession>()->Activate();
	Menu = CreateWidget<UTBTitleMenu>(this, UTBTitleMenu::StaticClass());
	if (Menu)
	{
		Menu->AddToViewport();
		FInputModeGameAndUI Input;
		Input.SetWidgetToFocus(Menu->TakeWidget());
		Input.SetHideCursorDuringCapture(false);
		Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Input);
	}
	bShowMouseCursor = true;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void ATBTitleController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Menu) Menu->RemoveFromParent();
	Super::EndPlay(Reason);
}

void ATBTitleController::TBHost() { GetGameInstance()->GetSubsystem<UTBSession>()->Host(); }
void ATBTitleController::TBFind() { GetGameInstance()->GetSubsystem<UTBSession>()->Find(); }
void ATBTitleController::TBJoin(int32 Index) { GetGameInstance()->GetSubsystem<UTBSession>()->Join(Index); }
void ATBTitleController::TBLeave() { GetGameInstance()->GetSubsystem<UTBSession>()->Leave(); }
