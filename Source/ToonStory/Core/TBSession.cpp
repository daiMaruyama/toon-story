#include "TBSession.h"
#include "Core/TBGameState.h"
#include "Core/TBGameHelpers.h"
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

namespace
{
	const FName GameKey(TEXT("TOYBOX_BUILD"));
	const FString BuildTag(TEXT("ToonStory_ResidentialStages_20261003"));
	const FName LobbyKey(TEXT("LOBBY_OPEN"));
} // namespace
void UTBSession::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UTBSession::NetworkFailed);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UTBSession::TravelFailed);
	}
}

void UTBSession::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(ReturnTicker);
	FTSTicker::GetCoreTicker().RemoveTicker(CleanupTimeout);
	if (UWorld* World = GetWorld())
	{
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

void UTBSession::Message(const FString& Text)
{
	Status = Text;
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
		Message(TEXT("No online session interface."));
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
	if (CurrentOperation == ESessionOperation::Travelling) CurrentOperation = ESessionOperation::Idle;
	bReturningToTitle = false;
	Acquire();
	Changed.Broadcast();
}

bool UTBSession::HasSession() const
{
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

// 部屋作成を依頼する。作成成功後のマップ移動はCreatedで行う。
void UTBSession::Host()
{
	if (IsBusy() || !Acquire())
	{
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Message(TEXT("Existing session: TBLeave before hosting again."));
		return;
	}
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(GetWorld());
	const bool bIsLAN = OnlineSubsystem && OnlineSubsystem->GetSubsystemName() == FName(TEXT("NULL"));
	FOnlineSessionSettings Settings;
	const auto* GameMode = GetWorld()->GetAuthGameMode();
	if (!GameMode || !GameMode->GameSession || GameMode->GameSession->MaxPlayers < 2)
	{
		Message(TEXT("Hosting requires an offline/server world and MaxPlayers >= 2."));
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
	Message(TEXT("Creating room..."));
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		CurrentOperation = ESessionOperation::Idle;
		Message(TEXT("CreateSession could not start. Check Steam login/App ID."));
	}
}

// 非同期の部屋作成が完了したら、ステージを1つ選んでリッスンサーバーとして開く。
void UTBSession::Created(FName Name, bool bSucceeded)
{
	if (Name != NAME_GameSession || CurrentOperation != ESessionOperation::Creating)
	{
		return;
	}
	CurrentOperation = ESessionOperation::Idle;
	if (bReturnRequested) { BeginReturn(); return; }
	if (!bSucceeded)
	{
		Message(TEXT("Room creation failed."));
		return;
	}
	CurrentOperation = ESessionOperation::Travelling;
	Message(TEXT("Room created. Opening listen server..."));
	TArray<FName> Stages;
	for (const TCHAR* Map : {TEXT("/Game/Maps/TB_ArchViz"), TEXT("/Game/Maps/TB_KidsRoom")})
	{
		if (FPackageName::DoesPackageExist(Map))
		{
			Stages.Add(FName(Map));
		}
	}
	if (Stages.IsEmpty())
	{
		Message(TEXT("Residential stages are not installed. Opening test arena."));
	}
	const FName Selected =
	    Stages.IsEmpty() ? FName(TEXT("/Game/Maps/Arena")) : Stages[FMath::RandRange(0, Stages.Num() - 1)];
	// 抽選はホストで1回だけ。参加者は接続時にホストと同じマップへ移動する。
	UGameplayStatics::OpenLevel(GetGameInstance(), Selected, true, TEXT("listen"));
}

// 同じビルド識別子の部屋を検索する。表示用文字列と参加用結果は分けて保持する。
void UTBSession::Find()
{
	if (IsBusy() || !Acquire())
	{
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Message(TEXT("TBLeave before searching."));
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
		Message(TEXT("FindSessions could not start."));
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
	bUpdatingLobbyOpen = State->Phase == ETBPhase::Lobby;
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
		if (State && bUpdatingLobbyOpen != (State->Phase == ETBPhase::Lobby))
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
	if (bReturnRequested) { BeginReturn(); return; }
	Results.Reset();
	Rooms.Reset();
	if (!bSucceeded || !Search.IsValid())
	{
		Message(TEXT("Search failed."));
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
	if (IsBusy() || !Acquire())
	{
		return;
	}
	if (!Results.IsValidIndex(Index))
	{
		Message(TEXT("Invalid room index. TBFind first."));
		return;
	}
	BeginJoin(Results[Index]);
}

void UTBSession::BeginJoin(const FOnlineSessionSearchResult& SearchResult)
{
	if (IsBusy() || !SearchResult.IsValid() || !Sessions.IsValid())
	{
		return;
	}
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		Message(TEXT("TBLeave before joining another room."));
		return;
	}
	bool bLobbyOpen = false;
	if (!SearchResult.Session.SessionSettings.Get(LobbyKey, bLobbyOpen) || !bLobbyOpen ||
	    SearchResult.Session.NumOpenPublicConnections <= 0)
	{
		Message(TEXT("Room is closed or full. TBFind to refresh."));
		return;
	}
	CurrentOperation = ESessionOperation::Joining;
	Message(TEXT("Joining room..."));
	if (!Sessions->JoinSession(0, NAME_GameSession, SearchResult))
	{
		CurrentOperation = ESessionOperation::Idle;
		Message(TEXT("JoinSession could not start."));
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
	if (bReturnRequested) { BeginReturn(); return; }
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		RequestReturn(TEXT("Could not join the room. Please refresh the room list and try again."));
		return;
	}
	FString URL;
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (!PlayerController || !Sessions->GetResolvedConnectString(NAME_GameSession, URL) || URL.IsEmpty())
	{
		RequestReturn(TEXT("Could not resolve the host address. Please try again."));
		return;
	}
	CurrentOperation = ESessionOperation::Travelling;
	Message(TEXT("Connecting to host..."));
	PlayerController->ClientTravel(URL, TRAVEL_Absolute);
}

void UTBSession::Accepted(bool bSucceeded, int32 LocalUser, TSharedPtr<const FUniqueNetId> Id,
                          const FOnlineSessionSearchResult& SearchResult)
{
	if (!bSucceeded || LocalUser != 0 || !Acquire())
	{
		return;
	}
	FString Tag;
	if (!SearchResult.Session.SessionSettings.Get(GameKey, Tag) || Tag != BuildTag)
	{
		Message(TEXT("Invite build mismatch."));
		return;
	}
	BeginJoin(SearchResult);
}

// The core ticker survives a failed travel tearing down the old world.
void UTBSession::RequestReturn(const FString& Reason)
{
	if (bReturningToTitle) return;
	if (!bReturnRequested) ReturnReason = Reason;
	bReturnRequested = true;
	Message(ReturnReason);
	FTSTicker::GetCoreTicker().RemoveTicker(ReturnTicker);
	ReturnTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		ReturnTicker.Reset();
		BeginReturn();
		return false;
	}));
}

void UTBSession::Leave()
{
	RequestReturn(TEXT("You left the room."));
}

void UTBSession::BeginReturn()
{
	if (CurrentOperation == ESessionOperation::Closing || bReturningToTitle) return;
	// An in-flight create/join may still create a session. Its callback resumes cleanup.
	if (CurrentOperation == ESessionOperation::Creating || CurrentOperation == ESessionOperation::Joining ||
	    CurrentOperation == ESessionOperation::Finding) return;
	bReturnRequested = true;
	CurrentOperation = ESessionOperation::Closing;
	bLobbyUpdateInFlight = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(LobbyUpdateTimer);
	if (!HasSession()) { ReturnOffline(); return; }
	// Do not trap the player in a disconnected stage if a backend never answers.
	CleanupTimeout = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		CleanupTimeout.Reset();
		ReturnReason += TEXT(" Connection cleanup timed out. Retry cleanup on the title screen.");
		ReturnOffline();
		return false;
	}), 10.f);
	if (!Sessions->DestroySession(NAME_GameSession))
	{
		ReturnReason += TEXT(" Connection cleanup could not start. Retry cleanup on the title screen.");
		ReturnOffline();
	}
}

void UTBSession::Destroyed(FName Name, bool bSucceeded)
{
	if (Name != NAME_GameSession || CurrentOperation != ESessionOperation::Closing) return;
	if (!bSucceeded) ReturnReason += TEXT(" Connection cleanup failed. Retry cleanup on the title screen.");
	ReturnOffline();
}

void UTBSession::ReturnOffline()
{
	FTSTicker::GetCoreTicker().RemoveTicker(CleanupTimeout);
	CleanupTimeout.Reset();
	CurrentOperation = ESessionOperation::Idle;
	bReturnRequested = false;
	Results.Reset();
	Rooms.Reset();
	Search.Reset();
	const bool AlreadyOnTitle = GetWorld() && GetWorld()->GetNetMode() == NM_Standalone &&
	    UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("Title");
	Message(ReturnReason.IsEmpty() ? TEXT("Create a room or find friends to begin.") : ReturnReason);
	ReturnReason.Reset();
	if (!AlreadyOnTitle)
	{
		bReturningToTitle = true;
		UGameplayStatics::OpenLevel(GetGameInstance(), FName(TEXT("/Game/Maps/Title")), true);
	}
	Changed.Broadcast();
}

void UTBSession::NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
	if (!World || World != GetWorld()) return;
	// One departing client must not eject everyone else from the host's room.
	if ((World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer) &&
	    (Type == ENetworkFailure::ConnectionLost || Type == ENetworkFailure::ConnectionTimeout)) return;
	RequestReturn(FString::Printf(TEXT("Connection lost: %s"), *Error));
}

void UTBSession::TravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
	if (!World || World != GetWorld()) return;
	if (bReturningToTitle)
	{
		bReturningToTitle = false;
		Message(TEXT("Could not open Title. Check that /Game/Maps/Title is installed and cooked."));
		return; // Never recurse on a missing/corrupt title map.
	}
	RequestReturn(FString::Printf(TEXT("Could not open the room: %s"), *Error));
}