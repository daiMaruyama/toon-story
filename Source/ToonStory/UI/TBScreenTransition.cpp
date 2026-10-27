// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBScreenTransition.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace TBScreen
{
	void HoldBlack(APlayerController* PlayerController, bool Hold)
	{
		if (PlayerController && PlayerController->PlayerCameraManager)
		{
			if (Hold) PlayerController->PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, false);
			else PlayerController->PlayerCameraManager->StopCameraFade();
		}
	}
	// 簡易版
	void PlayTransition(APlayerController* PlayerController, const FLinearColor& Color)
	{
		if (PlayerController && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->StartCameraFade(1.f, 0.f, .4f, Color);
		}
	}
}
