// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Input/TBKeyRemap.h"

namespace TB
{
	enum class EKeyPromptKind : uint8
	{
		Done,    // 付け替えが済んだ。板を閉じる
		Retry,   // 使えないキー。待ちを続ける
		AskSwap, // かぶりが1行。入れ替えを聞く
		Notice,  // 事情をお知らせして終わる
		Redo,    // 入れ替える相手がいなくなった。Remapをやり直す
	};

	struct FKeyPrompt
	{
		EKeyPromptKind Kind = EKeyPromptKind::Done;
		FString Text;
	};

	/** 「見出し：表示名」。表に無い名前は名前そのまま。 */
	TOONSTORY_API FString KeyRowName(FName Name);
	/** キーを待っている間に板へ出す文。 */
	TOONSTORY_API FString WaitingText(FName Row);
	/** Remapの結果から、次に画面がすることと板の文を決める。Keyは押されたキー。 */
	TOONSTORY_API FKeyPrompt PromptForRemap(const FRemapResult& Result, const FKey& Key);
	/** SwapRemapの結果から、次に画面がすることと板の文を決める。 */
	TOONSTORY_API FKeyPrompt PromptForSwap(const FRemapResult& Result);
}
