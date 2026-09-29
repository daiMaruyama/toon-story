#include "TBSession.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	const FName GameKey(TEXT("TOYBOX_BUILD"));
	const FString BuildTag(TEXT("ToonStory_FixedBox_20260930"));
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
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CloseTimer);
	}
	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
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
	Settings.NumPublicConnections = 8;
	Settings.bIsLANMatch = bIsLAN;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = !bIsLAN;
	Settings.bAllowJoinViaPresence = !bIsLAN;
	Settings.bUseLobbiesIfAvailable = !bIsLAN;
	Settings.BuildUniqueId = 2;
	Settings.Set(GameKey, BuildTag, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	CurrentOperation = ESessionOperation::Creating;
	Message(TEXT("Creating room..."));
	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		CurrentOperation = ESessionOperation::Idle;
		Message(TEXT("CreateSession could not start. Check Steam login/App ID."));
	}
}

// 非同期の部屋作成が完了したら、Arenaをリッスンサーバーとして開く。
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
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(TEXT("/Game/Maps/Arena")), true, TEXT("listen"));
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
		if (!SearchResult.IsValid() || !SearchResult.Session.SessionSettings.Get(GameKey, Tag) || Tag != BuildTag)
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
	// Waiting for a create/join callback avoids creating an orphaned session after Leave.
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
	CurrentOperation = ESessionOperation::Idle;
	Results.Reset();
	Rooms.Reset();
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(TEXT("/Game/Maps/Arena")), true);
	Message(TEXT("Offline. Last match ended; TBHost / TBFind for a new lobby."));
}

// 自分のWorldの通信失敗だけを扱い、通知後に通常の退出処理へ進む。
void UTBSession::NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
	if (World != GetWorld())
	{
		return;
	}
	Message(FString::Printf(TEXT("Network failure: %s"), *Error));
	// Keep the reason for a moment, then run normal session cleanup. No fake reconnect.
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
	Message(FString::Printf(TEXT("Travel failed: %s. Check Arena is saved/cooked; TBLeave cleans session."), *Error));
}
