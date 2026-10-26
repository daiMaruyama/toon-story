// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBInputSettings.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"

TArray<const UInputMappingContext*> UTBInputSettings::GetAllContexts() const
{
	TArray<const UInputMappingContext*> Result;
	for (const TSoftObjectPtr<UInputMappingContext>* Soft : {&CommonContext, &HumanContext, &ToyContext, &LobbyContext})
	{
		if (const UInputMappingContext* Context = Soft->IsNull() ? nullptr : Soft->LoadSynchronous())
		{
			Result.Add(Context);
		}
	}
	return Result;
}

void UTBInputSettings::RegisterContexts(const ULocalPlayer* LocalPlayer)
{
	auto* Input = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	auto* User = Input ? Input->GetUserSettings() : nullptr;
	if (!User)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToonStoryInput] UserSettings is not available; contexts were not registered."));
		return;
	}
	for (const UInputMappingContext* Context : GetDefault<UTBInputSettings>()->GetAllContexts())
	{
		User->RegisterInputMappingContext(Context);
	}
}
