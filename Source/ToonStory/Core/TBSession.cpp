// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBSession.h"
#include "Core/TBController.h"
#include "Core/TBChat.h"
#include "UObject/UObjectGlobals.h"
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBGameState.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBLobbySettings.h"
#include "Core/TBGameMode.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Misc/PackageName.h"
#include "Misc/CoreDelegates.h"

namespace
{
	constexpr double OfflineReturnTimeoutSeconds = 30.0;
	const FName GameKey(TEXT("TOYBOX_BUILD"));
	const FString BuildTag(TEXT("ToonStory_RandomItems_MomEvent_20261101"));
	const FName LobbyKey(TEXT("LOBBY_OPEN"));
} // namespace
void UTBSession::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EndFrameHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &UTBSession::FinishTravelRecovery);
	MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UTBSession::OfflineMapLoaded);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UTBSession::NetworkFailed);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UTBSession::TravelFailed);
	}
}

void UTBSession::Deinitialize()
{
	StopOfflineReturnWatch();
	bOfflineReturnFailurePending = false;
	OfflineReturnFailureWorld.Reset();
	FCoreDelegates::OnEndFrame.Remove(EndFrameHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
	GetGameInstance()->GetTimerManager().ClearTimer(CloseTimer);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CloseTimer);
		World->GetTimerManager().ClearTimer(LobbyUpdateTimer);
	}
	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
		Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateSessionHandle);
	}
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	Sessions.Reset();
	Super::Deinitialize();
}

void UTBSession::Message(const FString& Text, bool bError)
{
	Status = Text;
	if (bError)
		if (auto* Audio = GetGameInstance()->GetSubsystem<UTBAudioSubsystem>()) Audio->PlaySoundEvent(ETBSoundEvent::UIError);
	UE_LOG(LogTemp, Log, TEXT("[ToyBoxSession] %s"), *Text);
	Changed.Broadcast();
}

// Worldに対応するオンライン機能を取得し、完了通知を一度だけ登録する。
bool UTBSession::Acquire()
{
	if (Sessions.IsValid())
	{
		return true;
	}
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	if (!OnlineSubsystem || !OnlineSubsystem->GetSessionInterface().IsValid())
	{
		Message(TEXT("No online session interface."), true);
		return false;
	}
	Sessions = OnlineSubsystem->GetSessionInterface();
	CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
	    FOnCreateSessionCompleteDelegate::CreateUObject(this, &UTBSession::Created));
	FindSessionsHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
	    FOnFindSessionsCompleteDelegate::CreateUObject(this, &UTBSession::Found));
	JoinSessionHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
	    FOnJoinSessionCompleteDelegate::CreateUObject(this, &UTBSession::Joined));
	DestroySessionHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
	    FOnDestroySessionCompleteDelegate::CreateUObject(this, &UTBSession::Destroyed));
	InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
	    FOnSessionUserInviteAcceptedDelegate::CreateUObject(this, &UTBSession::Accepted));
	UpdateSessionHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(
	    FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UTBSession::LobbyUpdated));
	return true;
}

void UTBSession::Activate()
{
	Acquire();
}

// 部屋作成を依頼する。作成成功後のマップ移動はCreatedで行う。
void UTBSession::Host()
{
	if (CurrentOperation != ESessionOperation::Idle || !Acquire())
	{
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Message(TEXT("Existing session: TBLeave before hosting again."), true);
		return;
	}
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const bool bIsLAN = OnlineSubsystem && OnlineSubsystem->GetSubsystemName() == FName(TEXT("NULL"));
	FOnlineSessionSettings Settings;
	const auto* GameMode = GetWorld()->GetAuthGameMode();
	if (!GameMode || !GameMode->GameSession || GameMode->GameSession->MaxPlayers < 2)
	{
		Message(TEXT("Hosting requires an offline/server world and MaxPlayers >= 2."), true);
		return;
	}
	Settings.NumPublicConnections = GameMode->GameSession->MaxPlayers;
	Settings.bIsLANMatch = bIsLAN;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	// Steamはメンバー変更時にもこの値でロビーを参加可能にする。試合前は開けておく。
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = !bIsLAN;
	Settings.bAllowJoinViaPresence = !bIsLAN;
	Settings.bUseLobbiesIfAvailable = !bIsLAN;
	Settings.BuildUniqueId = 2;
	Settings.Set(GameKey, BuildTag, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(LobbyKey, true, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	CurrentOperation = ESessionOperation::Creating;
	ClearChat();
	Message(TEXT("Creating room..."));
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		CurrentOperation = ESessionOperation::Idle;
		Message(TEXT("CreateSession could not start. Check Steam login/App ID."), true);
	}
}

// 部屋作成後は専用ロビーをリッスンサーバーとして開く。
void UTBSession::Created(FName Name, bool bSucceeded)
{
	if (Name != NAME_GameSession || CurrentOperation != ESessionOperation::Creating)
	{
		return;
	}
	CurrentOperation = ESessionOperation::Idle;
	if (!bSucceeded)
	{
		Message(TEXT("Room creation failed."), true);
		return;
	}
	Message(TEXT("Room created. Opening listen server..."));
	const TCHAR* LobbyMap = TEXT("/Game/Maps/TB_Lobby");
	if (!FPackageName::DoesPackageExist(LobbyMap))
	{
		Message(TEXT("Lobby map is not installed. Opening test arena."));
		LobbyMap = TEXT("/Game/Maps/Arena");
	}
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(LobbyMap), true, TEXT("listen"));
}

// 同じビルド識別子の部屋を検索する。表示用文字列と参加用結果は分けて保持する。
void UTBSession::Find()
{
	if (CurrentOperation != ESessionOperation::Idle || !Acquire())
	{
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Message(TEXT("TBLeave before searching."), true);
		return;
	}
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const bool bIsLAN = OnlineSubsystem && OnlineSubsystem->GetSubsystemName() == FName(TEXT("NULL"));
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = bIsLAN;
	Search->MaxSearchResults = 200;
	if (!bIsLAN)
	{
		Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}
	Search->QuerySettings.Set(GameKey, BuildTag, EOnlineComparisonOp::Equals);
	Results.Reset();
	Rooms.Reset();
	CurrentOperation = ESessionOperation::Finding;
	Message(TEXT("Searching rooms..."));
	if (!Sessions->FindSessions(0, Search.ToSharedRef()))
	{
		CurrentOperation = ESessionOperation::Idle;
		Message(TEXT("FindSessions could not start."), true);
	}
}

// 再試行時にも現在のPhaseを参照し、古い「閉じる」要求が再戦ロビーを閉じないようにする。
void UTBSession::RefreshLobbyAvailability()
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode() || !Sessions.IsValid() ||
	    CurrentOperation != ESessionOperation::Idle || bLobbyUpdateInFlight)
	{
		return;
	}
	auto* Current = Sessions->GetSessionSettings(NAME_GameSession);
	const auto* State = TB::GS(GetWorld());
	if (!Current || !State)
	{
		return; // PIEの直接接続にはオンラインセッションがない。
	}
	FOnlineSessionSettings Settings = *Current;
	bUpdatingLobbyOpen = State->Phase == ETBPhase::Lobby && !State->bTravelPending;
	Settings.bShouldAdvertise = bUpdatingLobbyOpen;
	// ゲームのPhaseに合わせる。常にfalseだと、Steamが待機中の部屋まで検索から隠す。
	Settings.bAllowJoinInProgress = bUpdatingLobbyOpen;
	Settings.bAllowInvites = bUpdatingLobbyOpen;
	Settings.bAllowJoinViaPresence = bUpdatingLobbyOpen && Settings.bUsesPresence;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.Set(LobbyKey, bUpdatingLobbyOpen, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	bLobbyUpdateInFlight = true;
	if (!Sessions->UpdateSession(NAME_GameSession, Settings, true))
	{
		LobbyUpdated(NAME_GameSession, false);
	}
}

void UTBSession::LobbyUpdated(FName Name, bool bSucceeded)
{
	if (Name != NAME_GameSession || !bLobbyUpdateInFlight)
	{
		return;
	}
	bLobbyUpdateInFlight = false;
	if (!GetWorld() || CurrentOperation != ESessionOperation::Idle)
	{
		return;
	}
	if (!bSucceeded)
	{
		Message(TEXT("Could not update room advertisement; retrying."));
		GetWorld()->GetTimerManager().SetTimer(LobbyUpdateTimer, this, &UTBSession::RefreshLobbyAvailability, 2.f,
		                                       false);
	}
	else
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(LobbyUpdateTimer);
		}
		const auto* State = TB::GS(GetWorld());
		if (State && bUpdatingLobbyOpen != (State->Phase == ETBPhase::Lobby && !State->bTravelPending))
		{
			RefreshLobbyAvailability();
		}
	}
}

void UTBSession::Found(bool bSucceeded)
{
	if (CurrentOperation != ESessionOperation::Finding)
	{
		return;
	}
	CurrentOperation = ESessionOperation::Idle;
	Results.Reset();
	Rooms.Reset();
	if (!bSucceeded || !Search.IsValid())
	{
		Message(TEXT("Search failed."), true);
		return;
	}
	for (const auto& SearchResult : Search->SearchResults)
	{
		FString Tag;
		bool bLobbyOpen = false;
		if (!SearchResult.IsValid() || !SearchResult.Session.SessionSettings.Get(GameKey, Tag) || Tag != BuildTag ||
		    !SearchResult.Session.SessionSettings.Get(LobbyKey, bLobbyOpen) || !bLobbyOpen ||
		    SearchResult.Session.NumOpenPublicConnections <= 0)
		{
			continue;
		}
		Results.Add(SearchResult);
		Rooms.Add(FString::Printf(TEXT("%s | open %d/%d | ping %d"), *SearchResult.Session.OwningUserName,
		                          SearchResult.Session.NumOpenPublicConnections,
		                          SearchResult.Session.SessionSettings.NumPublicConnections, SearchResult.PingInMs));
	}
	Message(FString::Printf(TEXT("Found %d rooms. Use TBJoin <index>."), Results.Num()));
}

void UTBSession::Join(int32 Index)
{
	if (CurrentOperation != ESessionOperation::Idle || !Acquire())
	{
		return;
	}
	if (!Results.IsValidIndex(Index))
	{
		Message(TEXT("Invalid room index. TBFind first."), true);
		return;
	}
	BeginJoin(Results[Index]);
}

void UTBSession::BeginJoin(const FOnlineSessionSearchResult& SearchResult)
{
	if (CurrentOperation != ESessionOperation::Idle || !SearchResult.IsValid())
	{
		return;
	}
	FString Tag;
	if (!SearchResult.Session.SessionSettings.Get(GameKey, Tag) || Tag != BuildTag)
	{
		Message(TEXT("Room build mismatch. TBFind to refresh."), true);
		return;
	}
	if (!Sessions.IsValid()) return;
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Message(TEXT("TBLeave before joining another room."), true);
		return;
	}
	bool bLobbyOpen = false;
	if (!SearchResult.Session.SessionSettings.Get(LobbyKey, bLobbyOpen) || !bLobbyOpen ||
	    SearchResult.Session.NumOpenPublicConnections <= 0)
	{
		Message(TEXT("Room is closed or full. TBFind to refresh."), true);
		return;
	}
	CurrentOperation = ESessionOperation::Joining;
	ClearChat();
	Message(TEXT("Joining room..."));
	if (!Sessions->JoinSession(0, NAME_GameSession, SearchResult))
	{
		CurrentOperation = ESessionOperation::Idle;
		Message(TEXT("JoinSession could not start."), true);
	}
}

// セッション参加後、接続先を解決して実際のゲームサーバーへ移動する。
void UTBSession::Joined(FName Name, EOnJoinSessionCompleteResult::Type Result)
{
	if (Name != NAME_GameSession || CurrentOperation != ESessionOperation::Joining)
	{
		return;
	}
	CurrentOperation = ESessionOperation::Idle;
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		Message(TEXT("Join failed. If a local session remains, TBLeave then retry."), true);
		return;
	}
	FString URL;
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (!PlayerController || !Sessions->GetResolvedConnectString(NAME_GameSession, URL) || URL.IsEmpty())
	{
		Message(TEXT("Could not resolve connection. TBLeave to clean up."), true);
		return;
	}
	Message(TEXT("Connecting to host..."));
	PlayerController->ClientTravel(URL, TRAVEL_Absolute);
}

void UTBSession::Accepted(bool bSucceeded, int32 LocalUser, TSharedPtr<const FUniqueNetId> Id,
                          const FOnlineSessionSearchResult& SearchResult)
{
	if (!bSucceeded || LocalUser != 0)
	{
		return;
	}
	FString Tag;
	if (!SearchResult.Session.SessionSettings.Get(GameKey, Tag) || Tag != BuildTag)
	{
		Message(TEXT("Invite build mismatch."), true);
		return;
	}
	if (Acquire()) BeginJoin(SearchResult);
}

// 処理中の非同期操作は取り消さず、完了を待ってから退出してもらう。
void UTBSession::Leave()
{
	// 作成・参加の完了を待ち、退出後にセッションが残るのを防ぐ。
	if (CurrentOperation == ESessionOperation::Closing)
	{
		return;
	}
	if (CurrentOperation != ESessionOperation::Idle)
	{
		Message(TEXT("Online operation still pending. Wait for its completion before leaving."), true);
		return;
	}
	if (!Acquire())
	{
		ReturnOffline();
		return;
	}
	CurrentOperation = ESessionOperation::Closing;
	// GameInstance timers survive network-driver teardown and map changes.
	GetGameInstance()->GetTimerManager().SetTimer(CloseTimer, this, &UTBSession::ReturnOffline, 5.f, false);
	if (GetWorld() && GetWorld()->GetNetMode() == NM_ListenServer)
	{
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (auto* PC = Cast<ATBController>(It->Get()); PC && !PC->IsLocalController()) PC->ClientRoomClosed();
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyUpdateTimer);
	}
	Message(TEXT("Leaving room..."));
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		if (!Sessions->DestroySession(NAME_GameSession))
		{
			ReturnOffline();
		}
	}
	else
	{
		ReturnOffline();
	}
}

void UTBSession::Destroyed(FName Name, bool bSucceeded)
{
	if (Name != NAME_GameSession || CurrentOperation != ESessionOperation::Closing)
	{
		return;
	}
	ReturnOffline();
}

void UTBSession::ReturnOffline()
{
	if (bReturningOffline) return;
	bReturningOffline = true;
	CurrentOperation = ESessionOperation::Closing;
	bLobbyUpdateInFlight = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(LobbyUpdateTimer);
	Results.Reset();
	Rooms.Reset();
	Search.Reset();
	// Core ticker and wall time do not depend on the old world's timer/driver surviving travel.
	OfflineReturnDeadline = FPlatformTime::Seconds() + OfflineReturnTimeoutSeconds;
	OfflineReturnWatch = FTSTicker::GetCoreTicker().AddTicker(
	    FTickerDelegate::CreateUObject(this, &UTBSession::TickOfflineReturn));
	if (auto* Audio = GetGameInstance()->GetSubsystem<UTBAudioSubsystem>()) Audio->QueueTravelSound(ETBSoundEvent::LobbyLeave);
	// Do not browse from a net-driver/session callback. Give the close RPC time to flush.
	GetGameInstance()->GetTimerManager().SetTimer(CloseTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UGameplayStatics::OpenLevel(GetGameInstance(), FName(TEXT("/Game/Maps/Title")), true);
	}), .25f, false);
}

void UTBSession::StopOfflineReturnWatch()
{
	FTSTicker::GetCoreTicker().RemoveTicker(OfflineReturnWatch);
	OfflineReturnWatch.Reset();
	OfflineReturnDeadline = 0;
}

bool UTBSession::TickOfflineReturn(float DeltaSeconds)
{
	if (!bReturningOffline) return false;
	if (FPlatformTime::Seconds() >= OfflineReturnDeadline)
	{
		FailOfflineReturn(FString::Printf(TEXT("タイトルの読み込みが%.0f秒以内に完了しませんでした。"), OfflineReturnTimeoutSeconds));
		return false;
	}
	return true;
}

void UTBSession::FailOfflineReturn(const FString& Reason, UWorld* FailedWorld)
{
	if (!bReturningOffline) return;
	StopOfflineReturnWatch();
	GetGameInstance()->GetTimerManager().ClearTimer(CloseTimer);
	bReturningOffline = false;
	CurrentOperation = ESessionOperation::Idle;
	bOfflineReturnFailurePending = true;
	OfflineReturnFailureWorld = FailedWorld ? FailedWorld : GetWorld();
	const FString Failure = TEXT("タイトルへ戻れませんでした。部屋を出る操作を再試行してください。\n") + Reason;
	Message(Failure, true);
	if (UWorld* World = OfflineReturnFailureWorld.Get(); World && !World->bIsTearingDown)
	{
		for (auto It = World->GetPlayerControllerIterator(); It; ++It)
			if (auto* PC = Cast<ATBController>(It->Get())) PC->ClientNotice(Failure);
	}
	// Never OpenLevel here: failure delegates can run inside Browse/LoadMap.
}

void UTBSession::OfflineMapLoaded(UWorld* World)
{
	if (!World || World->GetGameInstance() != GetGameInstance() || !bReturningOffline ||
	    !World->GetMapName().EndsWith(TEXT("Title"))) return;
	StopOfflineReturnWatch();
	GetGameInstance()->GetTimerManager().ClearTimer(CloseTimer);
	bReturningOffline = false;
	CurrentOperation = ESessionOperation::Idle;
	Message(bKicked ? TEXT("ホストによって部屋から退出しました。") : bRoomClosed ? TEXT("ホストが退出したため、部屋が解散しました。") : !DisconnectReason.IsEmpty() ? DisconnectReason :
	    TEXT("Offline. Last match ended; TBHost / TBFind for a new lobby."));
	bRoomClosed = false;
	bKicked = false;
	DisconnectReason.Reset();
}

void UTBSession::Kicked()
{
 if (bOfflineReturnFailurePending) return;
 bKicked = true;
 RoomClosed();
}

void UTBSession::RoomClosed()
{
	if (bOfflineReturnFailurePending || bReturningOffline || CurrentOperation == ESessionOperation::Closing) return;
	bRoomClosed = true;
	CurrentOperation = ESessionOperation::Idle;
	Leave();
}

void UTBSession::NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
	if (!World || World != GetWorld()) return;
	if (bOfflineReturnFailurePending) return;
	if (World->GetNetMode() != NM_Client &&
	    (Type == ENetworkFailure::ConnectionLost || Type == ENetworkFailure::ConnectionTimeout)) return;
	UE_LOG(LogTemp, Warning, TEXT("[ToyBoxSession] Connection ended: %s"), *Error);
	if (bReturningOffline || CurrentOperation == ESessionOperation::Closing) return;
	DisconnectReason = TEXT("接続が切れました。通信状況を確認してください。\n") + Error;
	CurrentOperation = ESessionOperation::Idle;
	Leave();
}

void UTBSession::TravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
	if (!World || World->GetGameInstance() != GetGameInstance()) return;
	if (bOfflineReturnFailurePending) return;
	if (bReturningOffline)
	{
		FailOfflineReturn(Error, World);
		return;
	}
	if (CurrentOperation == ESessionOperation::Closing) return;
	UE_LOG(LogTemp, Warning, TEXT("[ToyBoxSession] Travel failed: %s"), *Error);
	const FString Reason = TEXT("マップの読み込みに失敗しました。\n") + Error;
	CurrentOperation = ESessionOperation::Idle;
	TBLobbySettings::CancelTravel(World);
	if (World->GetAuthGameMode<ATBGameMode>() && !World->bIsTearingDown)
	{
		TravelRecoveryWorld = World;
		Message(Reason, true);
		for (auto It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (auto* PC = Cast<ATBController>(It->Get())) PC->ClientNotice(Reason);
		}
		return;
	}
	DisconnectReason = Reason;
	Leave();
}

void UTBSession::FinishTravelRecovery()
{
	if (bOfflineReturnFailurePending)
	{
		bOfflineReturnFailurePending = false;
		UWorld* FailedWorld = OfflineReturnFailureWorld.Get();
		OfflineReturnFailureWorld.Reset();
		// The engine's failure listener can queue a disconnect after our listener returns.
		// Remove only that fallback, at end of frame, retaining the UI for an explicit retry.
		if (FailedWorld && !FailedWorld->bIsTearingDown && FailedWorld->GetGameInstance() == GetGameInstance())
		{
			if (GEngine)
				if (auto* Context = GEngine->GetWorldContextFromWorld(FailedWorld))
					if (Context->TravelURL == TEXT("?closed")) Context->TravelURL.Reset();
			if (FailedWorld->NextURL == TEXT("?closed")) FailedWorld->NextURL.Reset();
		}
	}
	UWorld* World = TravelRecoveryWorld.Get();
	TravelRecoveryWorld.Reset();
	if (!World || World != GetWorld() || World->bIsTearingDown || bReturningOffline ||
	    CurrentOperation == ESessionOperation::Closing || !World->GetAuthGameMode<ATBGameMode>()) return;
	// Failure listeners run before or after the engine listener. Wait until end of frame,
	// after it queues ?closed but before next frame can browse away and destroy the host.
	if (auto* Context = GEngine->GetWorldContextFromWorld(World))
	{
		if (Context->TravelURL == TEXT("?closed")) Context->TravelURL.Reset();
		if (World->GetNetMode() == NM_ListenServer) Context->LastURL.AddOption(TEXT("listen"));
	}
	World->NextURL.Reset();
}

void UTBSession::ClearChat()
{
	if (auto* Log = GetGameInstance()->GetSubsystem<UTBChatLog>()) Log->Clear();
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
class FBuildTestSessionInfo : public FOnlineSessionInfo
{
	FUniqueNetIdStringRef Id = FUniqueNetIdString::Create(TEXT("BuildCompatibility"), FName(TEXT("NULL")));
public:
	virtual const uint8* GetBytes() const override { return Id->GetBytes(); }
	virtual int32 GetSize() const override { return Id->GetSize(); }
	virtual bool IsValid() const override { return true; }
	virtual FString ToString() const override { return Id->ToString(); }
	virtual FString ToDebugString() const override { return Id->ToDebugString(); }
	virtual const FUniqueNetId& GetSessionId() const override { return *Id; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBSessionBuildCompatibilityTest,"ToonStory.Session.BuildCompatibility",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBSessionBuildCompatibilityTest::RunTest(const FString&)
{
	// 実際の検索結果・招待・参加入口を通す。オンライン接続や既存セッションは変更しない。
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	TStrongObjectPtr<UTBSession> Session(NewObject<UTBSession>(Instance.Get()));
	auto MakeResult=[](const FString& Tag)
	{
		FOnlineSessionSearchResult Result;
		Result.Session.SessionInfo=MakeShared<FBuildTestSessionInfo>();
		Result.Session.OwningUserId=FUniqueNetIdString::Create(TEXT("BuildHost"),FName(TEXT("NULL")));
		Result.Session.NumOpenPublicConnections=1;
		Result.Session.SessionSettings.NumPublicConnections=2;
		Result.Session.SessionSettings.Set(GameKey,Tag,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Result.Session.SessionSettings.Set(LobbyKey,true,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		return Result;
	};
	const FString OldTag(TEXT("ToonStory_ResidentialStages_20261028"));
	TestNotEqual(TEXT("Network schema change separates old build"),BuildTag,OldTag);
	const auto Current=MakeResult(BuildTag);
	const auto Old=MakeResult(OldTag);
	const auto ItemsOnly=MakeResult(TEXT("ToonStory_RandomItemSpawns_20261031"));
	const auto MomOnly=MakeResult(TEXT("ToonStory_MomEvent_20261101"));
	auto Missing=Current;
	Missing.Session.SessionSettings.Remove(GameKey);
	Session->Search=MakeShared<FOnlineSessionSearch>();
	Session->Search->SearchResults={Old,ItemsOnly,MomOnly,Missing,Current};
	Session->CurrentOperation=UTBSession::ESessionOperation::Finding;
	Session->Found(true);
	TestEqual(TEXT("Search exposes only matching build"),Session->Rooms.Num(),1);
	FString SelectedTag;
	if (Session->Results.Num()==1) Session->Results[0].Session.SessionSettings.Get(GameKey,SelectedTag);
	TestEqual(TEXT("Search retained the current build"),SelectedTag,BuildTag);
	for (const auto& Incompatible:{Old,ItemsOnly,MomOnly,Missing})
	{
		Session->Status.Reset();
		Session->BeginJoin(Incompatible);
		TestTrue(TEXT("Join rechecks build before connecting"),Session->Status.Contains(TEXT("Room build mismatch")));
		TestFalse(TEXT("Rejected join stays idle"),Session->IsBusy());
		Session->Accepted(true,0,{},Incompatible);
		TestTrue(TEXT("Invitation rejects old or missing build"),Session->Status.Contains(TEXT("Invite build mismatch")));
		TestFalse(TEXT("Rejected invitation stays idle"),Session->IsBusy());
	}
	Session->Status=TEXT("No online connection in this test");
	Session->BeginJoin(Current);
	TestEqual(TEXT("Same-build join passes version guard"),Session->Status,FString(TEXT("No online connection in this test")));
	return true;
}
#endif
