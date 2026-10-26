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

/**
 * サーバーが送る通知。キー名は{Mappable名}の印で入れ、受け取ったクライアントがExpandKeyTokensで自分の割り当てに置き換える。
 * 印はPrintf系でだけ使う（FString::FormatやFText::Formatは{}を引数として先に読む）。
 */
namespace TB::Notices
{
	inline constexpr const TCHAR* ItemsAutoCollect = TEXT("Items collect automatically while touching. {Toy_Interact} is for rescue.");
	inline constexpr const TCHAR* ReadyRequired = TEXT("Every player must press {Lobby_Ready} to be ready.");

	/** 上の全部 */
	inline TArray<const TCHAR*> All()
	{
		return {ItemsAutoCollect, ReadyRequired};
	}
}

namespace TB
{
	/** キー設定の表の1行。表示名は画面に出す日本語。 */
	struct FKeyRowInfo
	{
		FName Name;
		FString Label;
	};

	/** キー設定の見出しと、その下の行。 */
	struct FKeySection
	{
		FString Heading;
		TArray<FKeyRowInfo> Rows;
	};

	/** 画面の並び。共通、にんげん、おもちゃ、ロビー。TB::Keys::Allの13名がちょうど1回ずつ出る。 */
	TOONSTORY_API const TArray<FKeySection>& KeySections();

	/** }の手前で一番近い{から始まる名前を、そのクライアントのキー名に置き換える。TB::Keys::Allにない名前は残してWarningを出す。 */
	TOONSTORY_API FString ExpandKeyTokens(const APlayerController* PC, const FString& Text);
	/** 画面用の短いキー名。Invalidは「未割り当て」。 */
	TOONSTORY_API FString FormatKey(const FKey& Key);
	/** 行のキーボード側の割り当て。付け替えで、同じスロットと機器IDをMapPlayerKeyに渡す。 */
	struct FKeyboardKey
	{
		FKey Key;
		FKey DefaultKey;
		int32 Slot = 0; // EPlayerMappableKeySlotの値
		FName DeviceId;
	};
	/**
	 * 行のキーボード側（既定のキーがパッドでないもの）の割り当てを返す。有効なキーのうちスロットが一番小さいもの。
	 * 有効なものが無ければ、外した枠のうち一番小さいスロット。行が引けないときはfalse。
	 */
	TOONSTORY_API bool FindKeyboardKey(const APlayerController* PC, FName MappingName, FKeyboardKey& Out);
	/** 画面に出す、FindKeyboardKeyのキー名。引けないときはIMCの既定のキーを返し、どこにも無いときだけ「?」。 */
	TOONSTORY_API FString KeyLabel(const APlayerController* PC, FName MappingName);
	/** 移動の4つをまとめた表示。4つとも1文字ならWASDのようにつなげ、そうでなければ/で区切る。全部引けなければ「?」。 */
	TOONSTORY_API FString MoveKeysLabel(const APlayerController* PC);
	/** 試合中の画面の下に出す操作ヒント。人間と人形で出す行が違う。 */
	TOONSTORY_API FString MatchHintText(const APlayerController* PC, bool bHuman);
}
