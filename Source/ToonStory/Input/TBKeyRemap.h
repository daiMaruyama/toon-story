// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class APlayerController;

namespace TB
{
	enum class ERemapResult : uint8
	{
		OK,
		Conflict,  // 同じ範囲の別の行が、すでにそのキーを使っている（または、変えたあとにかぶりが残る）
		Unusable,  // 割り当てに使えないキー
		NoPartner, // 入れ替えようとしたが、かぶる相手がいない
		Error,     // UserSettingsが無い、行がIMCにちょうど1つ入っていない、キーボードの枠が無い、書き込みの失敗
	};

	struct FRemapResult
	{
		ERemapResult Result = ERemapResult::OK;
		TArray<FName> Conflicting; // Conflictのとき、かぶった相手のMappable名を全部
		FName SwappedWith;         // SwapRemapがOKのとき、入れ替えた相手
	};

	/**
	 * 割り当てに使えるキーか。Invalid、Escape（メニューの固定キー）、パッド、マウスの軸、マウスホイールの上下、
	 * Command（LeftCommand/RightCommand）、ConsoleKeysは使えない。マウスのボタン、Alt、Shift、Ctrl、Fキーは使える。
	 */
	TOONSTORY_API bool IsUsableKey(const FKey& Key);
	/**
	 * 付け替えてよいかを、13行×キーの表で調べる。かぶりはキーボード側だけを見て、左右のキーは別のキーとして扱う。
	 * 範囲は、その行がどのIMCに入っているかで決まる。Commonは全部、それ以外は自分のIMCとCommonだけ。
	 */
	TOONSTORY_API FRemapResult CheckRemap(const APlayerController* PC, FName MappingName, const FKey& NewKey);
	/** キーボード側のスロットをNewKeyに替える。判定がOKのときだけ替える。保存はしない。 */
	TOONSTORY_API FRemapResult Remap(const APlayerController* PC, FName MappingName, const FKey& NewKey);
	/**
	 * かぶった相手とキーを入れ替える。相手がちょうど1行で、入れ替えたあとにかぶりが残らないときだけ行う。
	 * 相手が複数、または入れ替えるとかぶりが残るときは、何も変えずにConflictを返す。相手がいなければNoPartner。
	 * 画面は、ResetToDefaultがかぶり先1行のConflictを返したら「入れ替えて戻しますか」と案内し、SwapRemap(行, 既定のキー)で戻す。
	 */
	TOONSTORY_API FRemapResult SwapRemap(const APlayerController* PC, FName MappingName, const FKey& NewKey);
	/** 1行のキーボード側を既定のキーに戻す。パッドは触らない。戻すとかぶるときは、戻さずにConflictを返す。 */
	TOONSTORY_API FRemapResult ResetToDefault(const APlayerController* PC, FName MappingName);
	/** 全部の行のキーボード側を既定のキーに戻す。パッドは触らない。13行の枠が全部引けなければ、何も変えずにErrorを返す。 */
	TOONSTORY_API FRemapResult ResetAll(const APlayerController* PC);
}
