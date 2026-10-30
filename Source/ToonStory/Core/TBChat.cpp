// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBChat.h"
#include "Containers/StringConv.h"
#include "Core/TBController.h"
#include "Data/TBRuleData.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
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

FString TB::Chat::DisplayName(const APlayerState* Player)
{
	return Player ? Player->GetPlayerName().Left(18) : FString();
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
