// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBChat.h"
#include "Containers/StringConv.h"
#include "Data/TBRuleData.h"
#include "HAL/PlatformTime.h"

FString TB::Chat::Limit(const FString& Text)
{
	const int32 MaxLength = Rules().Chat.MaxLength;
	if (Text.Len() <= MaxLength)
	{
		return Text;
	}
	return Text.Left(StringConv::IsHighSurrogate(Text[MaxLength - 1]) ? MaxLength - 1 : MaxLength);
}

FString TB::Chat::Clean(const FString& Text)
{
	FString Result;
	Result.Reserve(Text.Len());
	for (const TCHAR Character : Text)
	{
		Result.AppendChar(FChar::IsControl(Character) ? TEXT(' ') : Character);
	}
	Result.TrimStartAndEndInline();
	Result = Limit(Result);
	Result.TrimEndInline();
	return Result;
}

void UTBChatLog::Add(const FString& Name, const FString& Text, bool bSelf)
{
	Lines.Add({Name, Text, bSelf, FPlatformTime::Seconds()});
	if (Lines.Num() > TB::Chat::MaxLines)
	{
		Lines.RemoveAt(0, Lines.Num() - TB::Chat::MaxLines);
	}
	++Revision;
}

void UTBChatLog::Clear()
{
	Lines.Reset();
	++Revision;
}
