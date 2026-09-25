// Copyright Epic Games, Inc. All Rights Reserved.


#include "ToonStoryPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "ToonStory.h"
#include "ToonStoryGameMode.h"
#include "Core/ToonStoryPlayerState.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AToonStoryPlayerController::MatchReady(bool bReady)
{
	ServerSetMatchReady(bReady);
}

void AToonStoryPlayerController::ToggleMatchReady()
{
	if (const AToonStoryPlayerState* State = GetPlayerState<AToonStoryPlayerState>())
	{
		MatchReady(!State->IsMatchReady());
	}
}

void AToonStoryPlayerController::ServerSetMatchReady_Implementation(bool bReady)
{
	if (AToonStoryGameMode* Mode = GetWorld()->GetAuthGameMode<AToonStoryGameMode>())
	{
		Mode->SetPlayerReady(this, bReady);
	}
}

void AToonStoryPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogToonStory, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AToonStoryPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AToonStoryPlayerController::ToggleMatchReady);

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AToonStoryPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
