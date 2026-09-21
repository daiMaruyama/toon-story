#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ToonMatchHUD.generated.h"

/** Minimal playable loop display; a designed Widget HUD can replace this later. */
UCLASS()
class TOONSTORY_API AToonMatchHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
