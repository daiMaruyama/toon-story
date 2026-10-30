// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBChat.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"

#if WITH_TB_STEAM
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

// Steamの伏せ字。どこまで伏せるかは、受け取った人がSteamのアカウント設定で決めたとおりになる。
// LAN（Null）で遊んでいるときやSteamが動いていないときは、そのまま返す。
FString TB::Chat::Filter(const UWorld* World, const FString& Text, const FString& SenderNetId)
{
#if WITH_TB_STEAM
	const IOnlineSubsystem* Online = World ? Online::GetSubsystem(World) : nullptr;
	ISteamUtils* Utils = Online && Online->GetSubsystemName() == STEAM_SUBSYSTEM ? SteamUtils() : nullptr;
	const uint64 SenderId = FCString::Strtoui64(*SenderNetId, nullptr, 10);
	if (!Utils || SenderId == 0 || Text.IsEmpty())
	{
		return Text;
	}
	static const bool bReady = Utils->InitFilterText(0);
	if (!bReady)
	{
		return Text; // ゲームの言語に伏せ字が無いときは、Steamがそのまま返すのと同じ。
	}
	const FTCHARToUTF8 Input(*Text);
	TArray<ANSICHAR> Output;
	Output.SetNumZeroed(Input.Length() + 1);
	Utils->FilterText(k_ETextFilteringContextChat, CSteamID(SenderId), Input.Get(), Output.GetData(), Output.Num());
	return FString(UTF8_TO_TCHAR(Output.GetData()));
#else
	return Text;
#endif
}
