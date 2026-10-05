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
	if (!bSucceeded)
	{
		Message(TEXT("Room creation failed."));
		return;
	}
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
	if (CurrentOperation != ESessionOperation::Idle || !Acquire())
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
	if (CurrentOperation != ESessionOperation::Idle || !Acquire())
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
	if (CurrentOperation != ESessionOperation::Idle || !SearchResult.IsValid() || !Sessions.IsValid())
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
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		Message(TEXT("Join failed. If a local session remains, TBLeave then retry."));
		return;
	}
	FString URL;
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (!PlayerController || !Sessions->GetResolvedConnectString(NAME_GameSession, URL) || URL.IsEmpty())
	{
		Message(TEXT("Could not resolve connection. TBLeave to clean up."));
		return;
	}
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
		Message(TEXT("Online operation still pending. Wait for its completion before leaving."));
		return;
	}
	if (!Acquire())
	{
		ReturnOffline();
		return;
	}
	CurrentOperation = ESessionOperation::Closing;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyUpdateTimer);
	}
	Message(TEXT("Leaving room..."));
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		if (!Sessions->DestroySession(NAME_GameSession))
		{
			CurrentOperation = ESessionOperation::Idle;
			Message(TEXT("DestroySession could not start. Retry TBLeave."));
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
	if (!bSucceeded)
	{
		CurrentOperation = ESessionOperation::Idle;
		Message(TEXT("Session cleanup failed; retry TBLeave or restart the game."));
		return;
	}
	ReturnOffline();
}

void UTBSession::ReturnOffline()
{
	bLobbyUpdateInFlight = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LobbyUpdateTimer);
	}
	CurrentOperation = ESessionOperation::Idle;
	Results.Reset();
	Rooms.Reset();
	const TCHAR* Lobby = FPackageName::DoesPackageExist(TEXT("/Game/Maps/TB_KidsRoom")) ? TEXT("/Game/Maps/TB_KidsRoom")
	                                                                                    : TEXT("/Game/Maps/Arena");
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(Lobby), true);
	Message(TEXT("Offline. Last match ended; TBHost / TBFind for a new lobby."));
}

// 自分のWorldの通信失敗だけを扱い、通知後に通常の退出処理へ進む。
void UTBSession::NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
	if (!World || World != GetWorld())
	{
		return;
	}
	// サーバー側での単一クライアント切断はLogoutが扱う。ホストの部屋は閉じない。
	if (World->GetNetMode() != NM_Client &&
	    (Type == ENetworkFailure::ConnectionLost || Type == ENetworkFailure::ConnectionTimeout))
	{
		return;
	}
	Message(FString::Printf(TEXT("Network failure: %s"), *Error));
	// 切断理由を表示してから退出する。
	if (CurrentOperation == ESessionOperation::Idle)
	{
		World->GetTimerManager().SetTimer(CloseTimer, FTimerDelegate::CreateUObject(this, &UTBSession::Leave), 2.f,
		                                  false);
	}
}

void UTBSession::TravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
	if (World != GetWorld())
	{
		return;
	}
	Message(FString::Printf(
	    TEXT("Travel failed: %s. Check the destination map is saved/cooked; TBLeave cleans session."), *Error));
}
