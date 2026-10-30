// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBChat.h"
#include "Containers/StringConv.h"
#include "Core/TBController.h"
#include "Data/TBRuleData.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"

namespace
{
	// 見えないのに幅や向きを変える文字。空に見える発言や、向きを逆にしたなりすましに使える。
	bool IsHiddenFormat(TCHAR Character)
	{
		return (Character >= 0x200B && Character <= 0x200F) || (Character >= 0x202A && Character <= 0x202E) ||
		       (Character >= 0x2060 && Character <= 0x2069) || Character == 0xFEFF;
	}

	// 見えない文字は消し、改行などの制御文字は空白にして、前後を詰める。
	FString Strip(const FString& Text)
	{
		FString Result;
		Result.Reserve(Text.Len());
		for (const TCHAR Character : Text)
		{
			if (!IsHiddenFormat(Character))
			{
				Result.AppendChar(FChar::IsControl(Character) ? TEXT(' ') : Character);
			}
		}
		Result.TrimStartAndEndInline();
		return Result;
	}
}

FString TB::Chat::Limit(const FString& Text, int32 MaxLength)
{
	if (MaxLength <= 0 || Text.Len() <= MaxLength)
	{
		return MaxLength <= 0 ? FString() : Text;
	}
	return Text.Left(StringConv::IsHighSurrogate(Text[MaxLength - 1]) ? MaxLength - 1 : MaxLength);
}

FString TB::Chat::Clean(const FString& Text)
{
	FString Result = Limit(Strip(Text), Rules().Chat.MaxLength);
	Result.TrimEndInline();
	return Result;
}

FString TB::Chat::DisplayName(const APlayerState* Player)
{
	return Player ? Limit(Strip(Player->GetPlayerName()), NameLength) : FString();
}

FString TB::Chat::NoticeText(ETBChatNotice Kind, const FString& Name)
{
	switch (Kind)
	{
	case ETBChatNotice::Joined:
		return Name + TEXT("があそびにきた！");
	case ETBChatNotice::Kicked:
		return Name + TEXT("はホストにおうちへ帰された");
	default:
		return Name + TEXT("がかえっていった");
	}
}

void TB::Chat::BroadcastNotice(UWorld* World, ETBChatNotice Kind, const FString& Name, const AController* Except)
{
	if (!World || Name.IsEmpty())
	{
		return;
	}
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (auto* PC = Cast<ATBController>(It->Get()); PC && static_cast<const AController*>(PC) != Except)
		{
			PC->ClientReceiveChatNotice(Kind, Name);
		}
	}
}

void UTBChatLog::Add(const FString& Name, const FString& Text, bool bSelf)
{
	Push({Name, Text, bSelf});
}

void UTBChatLog::AddNotice(const FString& Text)
{
	FTBChatLine Line;
	Line.Text = Text;
	Line.bNotice = true;
	Push(MoveTemp(Line));
}

void UTBChatLog::Push(FTBChatLine&& Line)
{
	Line.Time = FPlatformTime::Seconds();
	Line.Id = NextId++;
	Lines.Add(MoveTemp(Line));
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
