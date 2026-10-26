// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class APlayerController;

/** IMCのPlayer Mappable Key Settingsの名前。文字列を散らさず、ここから使う。 */
namespace TB::Keys
{
	inline const FName MoveForward(TEXT("Common_Move_Forward"));
	inline const FName MoveBack(TEXT("Common_Move_Back"));
	inline const FName MoveLeft(TEXT("Common_Move_Left"));
	inline const FName MoveRight(TEXT("Common_Move_Right"));
	inline const FName Jump(TEXT("Common_Jump"));
	inline const FName HumanCrouch(TEXT("Human_Crouch"));
	inline const FName HumanInteract(TEXT("Human_Interact"));
	inline const FName HumanDrop(TEXT("Human_Drop"));
	inline const FName ToySprint(TEXT("Toy_Sprint"));
	inline const FName ToyInteract(TEXT("Toy_Interact"));
	inline const FName ToyTaunt(TEXT("Toy_Taunt"));
	inline const FName LobbyReady(TEXT("Lobby_Ready"));
	inline const FName LobbyStart(TEXT("Lobby_Start"));

	/** 上の名前の全部 */
	TOONSTORY_API const TArray<FName>& All();
}

namespace TB
{
	/** 画面用の短いキー名。Invalidは「未割り当て」。 */
	TOONSTORY_API FString FormatKey(const FKey& Key);
	/**
	 * ローカルプレイヤーの今の割り当てから、行のキーボード側でスロットが一番小さい有効なキーの名前を返す。
	 * UserSettingsや行が引けないときはIMCの既定のキーを返し、どこにも無いときだけ「?」。
	 */
	TOONSTORY_API FString KeyLabel(const APlayerController* PC, FName MappingName);
	/** 移動の4つをまとめた表示。4つとも1文字ならWASDのようにつなげ、そうでなければ/で区切る。全部引けなければ「?」。 */
	TOONSTORY_API FString MoveKeysLabel(const APlayerController* PC);
	/** 試合中の画面の下に出す操作ヒント。人間と人形で出す行が違う。 */
	TOONSTORY_API FString MatchHintText(const APlayerController* PC, bool bHuman);
}
