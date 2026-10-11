#include "TBScreenTransition.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace TBScreen
{
	// 簡易版
	void PlayTransition(APlayerController* PlayerController)
	{
		if (PlayerController && PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->StartCameraFade(1.f, 0.f, .4f, FLinearColor::Black);
		}
	}
}
