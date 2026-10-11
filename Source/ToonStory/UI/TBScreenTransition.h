#pragma once
#include "CoreMinimal.h"

class APlayerController;

/** 画面の切替演出。演出を作るときはこのファイルを触る。 */
namespace TBScreen
{
	// 瞬間移動や視点の切替を隠す用。そのプレイヤー自身の画面にだけ出す想定。
	void PlayTransition(APlayerController* PlayerController);
}
