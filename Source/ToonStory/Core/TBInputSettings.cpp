// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBInputSettings.h"
#include "InputAction.h"

void UTBInputSettings::HoldActions()
{
	for (const TSoftObjectPtr<UInputAction>* Soft : {&LookAction, &MoveAction, &JumpAction, &CrouchAction, &SprintAction,
	                                                 &InteractAction, &DropAction, &TauntAction, &ReadyAction,
	                                                 &StartAction, &MenuAction})
	{
		if (UInputAction* Action = Soft->LoadSynchronous())
		{
			HeldActions.AddUnique(Action);
		}
	}
}
