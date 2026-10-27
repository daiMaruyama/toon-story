// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Input/TBKeyPrompt.h"
#include "Input/TBKeyLabels.h"

namespace
{
	TB::FKeyPrompt Make(TB::EKeyPromptKind Kind, const FString& Text = FString())
	{
		TB::FKeyPrompt Prompt;
		Prompt.Kind = Kind;
		Prompt.Text = Text;
		return Prompt;
	}
}

FString TB::KeyRowName(FName Name)
{
	for (const FKeySection& Section : KeySections())
	{
		for (const FKeyRowInfo& Row : Section.Rows)
		{
			if (Row.Name == Name)
			{
				return Section.Heading + TEXT("：") + Row.Label;
			}
		}
	}
	return Name.ToString();
}

FString TB::WaitingText(FName Row)
{
	return FString::Printf(TEXT("「%s」にするキーを押してください　Escで取り消し"), *KeyRowName(Row));
}

TB::FKeyPrompt TB::PromptForRemap(const FRemapResult& Result, const FKey& Key)
{
	switch (Result.Result)
	{
	case ERemapResult::OK:
		return Make(EKeyPromptKind::Done);
	case ERemapResult::Unusable:
		return Make(EKeyPromptKind::Retry, TEXT("そのキーは使えません。ほかのキーを押してください"));
	case ERemapResult::Conflict:
		if (Result.Conflicting.Num() == 1)
		{
			return Make(EKeyPromptKind::AskSwap, FString::Printf(TEXT("「%s」は「%s」が使っています。入れ替えますか？"),
			                                                     *FormatKey(Key), *KeyRowName(Result.Conflicting[0])));
		}
		{
			// 重なっている相手を並べる。
			FString Names;
			for (const FName& Other : Result.Conflicting)
			{
				Names += FString::Printf(TEXT("「%s」"), *KeyRowName(Other));
			}
			return Make(EKeyPromptKind::Notice, TEXT("複数の操作と重なるので使えません\n") + Names);
		}
	default:
		return Make(EKeyPromptKind::Notice, TEXT("変更できませんでした"));
	}
}

TB::FKeyPrompt TB::PromptForReset(const FRemapResult& Result, const FKey& DefaultKey)
{
	if (Result.Result != ERemapResult::Conflict)
	{
		return PromptForRemap(Result, DefaultKey);
	}
	if (Result.Conflicting.Num() == 1)
	{
		return Make(EKeyPromptKind::AskSwap, FString::Printf(TEXT("「%s」は「%s」が使っています。入れ替えて戻しますか？"),
		                                                     *FormatKey(DefaultKey), *KeyRowName(Result.Conflicting[0])));
	}
	FString Names;
	for (const FName& Other : Result.Conflicting)
	{
		Names += FString::Printf(TEXT("「%s」"), *KeyRowName(Other));
	}
	return Make(EKeyPromptKind::Notice, TEXT("複数の操作と重なるので戻せません\n") + Names);
}

TB::FKeyPrompt TB::PromptForSwap(const FRemapResult& Result)
{
	switch (Result.Result)
	{
	case ERemapResult::OK:
		return Make(EKeyPromptKind::Done);
	case ERemapResult::NoPartner:
		return Make(EKeyPromptKind::Redo);
	case ERemapResult::Conflict:
		return Make(EKeyPromptKind::Notice, TEXT("入れ替えると別の操作と重なるので、入れ替えできませんでした"));
	case ERemapResult::Unusable:
		return Make(EKeyPromptKind::Notice, TEXT("そのキーは使えません"));
	default:
		return Make(EKeyPromptKind::Notice, TEXT("変更できませんでした"));
	}
}
