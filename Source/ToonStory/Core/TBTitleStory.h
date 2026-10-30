// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TBTitleStory.generated.h"

/** タイトルの物語を、ゲーム起動後の最初の1回だけ流すための記憶。マップを移っても残る。 */
UCLASS()
class TOONSTORY_API UTBTitleStory : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	// 物語を流し終えた（または飛ばした）か。試合から戻ったときはすぐメニューにする。
	bool bShown = false;
};
