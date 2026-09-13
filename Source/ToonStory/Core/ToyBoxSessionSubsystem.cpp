#include "Core/ToyBoxSessionSubsystem.h"

#include "Core/ToyBoxTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

namespace
{
	/** 検索でこのキーワードが一致するものだけを拾う。 */
	const FString ToyBoxSessionKeyword = TEXT("ToyBox");
}

IOnlineSessionPtr UToyBoxSessionSubsystem::GetSessionInterface() const
{
	IOnlineSubsystem* Online = IOnlineSubsystem::Get();
	return Online ? Online->GetSessionInterface() : nullptr;
}

void UToyBoxSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
			this, &UToyBoxSessionSubsystem::HandleNetworkFailure);
	}
}

void UToyBoxSessionSubsystem::Deinitialize()
{
	if (GEngine && NetworkFailureHandle.IsValid())
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		NetworkFailureHandle.Reset();
	}

	ClearDelegates();
	Super::Deinitialize();
}

void UToyBoxSessionSubsystem::HandleNetworkFailure(
	UWorld* /*World*/,
	UNetDriver* /*NetDriver*/,
	ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	const FString Reason = ErrorString.IsEmpty()
		? FString(ENetworkFailure::ToString(FailureType))
		: ErrorString;

	UE_LOG(LogToyBox, Warning, TEXT("接続が切れた: %s"), *Reason);

	OnDisconnected.Broadcast(Reason);
}

void UToyBoxSessionSubsystem::ClearDelegates()
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	if (CreateSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		CreateSessionCompleteHandle.Reset();
	}
	if (FindSessionsCompleteHandle.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		FindSessionsCompleteHandle.Reset();
	}
	if (JoinSessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();
	}
	if (DestroySessionCompleteHandle.IsValid())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		DestroySessionCompleteHandle.Reset();
	}
}

void UToyBoxSessionSubsystem::HostSession(int32 MaxPlayers, bool bUseLAN, const FString& MapPath)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		UE_LOG(LogToyBox, Error, TEXT("セッションインターフェースが取得できない"));
		OnHostComplete.Broadcast(false);
		return;
	}

	PendingMapPath = MapPath;

	TSharedRef<FOnlineSessionSettings> Settings = MakeShared<FOnlineSessionSettings>();
	Settings->NumPublicConnections = FMath::Max(1, MaxPlayers);
	Settings->NumPrivateConnections = 0;
	Settings->bIsLANMatch = bUseLAN;
	Settings->bShouldAdvertise = true;
	Settings->bUsesPresence = true;
	Settings->bAllowJoinViaPresence = true;
	Settings->bUseLobbiesIfAvailable = true;

	// 途中参加は許さない。陣営は開始時に一括で配るので、
	// 始まったあとに来られると人数比が崩れる。
	Settings->bAllowJoinInProgress = false;

	Settings->Set(SEARCH_KEYWORDS, ToyBoxSessionKeyword, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UToyBoxSessionSubsystem::HandleCreateSessionComplete));

	if (!Sessions->CreateSession(0, NAME_GameSession, *Settings))
	{
		ClearDelegates();
		OnHostComplete.Broadcast(false);
	}
}

void UToyBoxSessionSubsystem::HandleCreateSessionComplete(FName /*SessionName*/, bool bWasSuccessful)
{
	ClearDelegates();

	if (!bWasSuccessful)
	{
		OnHostComplete.Broadcast(false);
		return;
	}

	OnHostComplete.Broadcast(true);

	// リッスンサーバーとしてマップを開く。以降の移動は Seamless Travel。
	UWorld* World = GetWorld();
	if (World && !PendingMapPath.IsEmpty())
	{
		World->ServerTravel(PendingMapPath + TEXT("?listen"));
	}
}

void UToyBoxSessionSubsystem::FindSessions(bool bUseLAN, int32 MaxResults)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		OnSearchComplete.Broadcast(0);
		return;
	}

	SearchSettings = MakeShared<FOnlineSessionSearch>();
	SearchSettings->bIsLanQuery = bUseLAN;
	SearchSettings->MaxSearchResults = FMath::Max(1, MaxResults);
	SearchSettings->QuerySettings.Set(SEARCH_KEYWORDS, ToyBoxSessionKeyword, EOnlineComparisonOp::Equals);

	FindSessionsCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UToyBoxSessionSubsystem::HandleFindSessionsComplete));

	if (!Sessions->FindSessions(0, SearchSettings.ToSharedRef()))
	{
		ClearDelegates();
		OnSearchComplete.Broadcast(0);
	}
}

void UToyBoxSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	ClearDelegates();

	const int32 NumResults = (bWasSuccessful && SearchSettings.IsValid())
		? SearchSettings->SearchResults.Num()
		: 0;

	OnSearchComplete.Broadcast(NumResults);
}

TArray<FString> UToyBoxSessionSubsystem::GetSearchResultLabels() const
{
	TArray<FString> Labels;
	if (!SearchSettings.IsValid())
	{
		return Labels;
	}

	for (const FOnlineSessionSearchResult& Result : SearchSettings->SearchResults)
	{
		Labels.Add(FString::Printf(TEXT("%s (ping %d ms)"),
			*Result.Session.OwningUserName,
			Result.PingInMs));
	}

	return Labels;
}

void UToyBoxSessionSubsystem::JoinFoundSession(int32 ResultIndex)
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !SearchSettings.IsValid() || !SearchSettings->SearchResults.IsValidIndex(ResultIndex))
	{
		OnJoinComplete.Broadcast(false);
		return;
	}

	JoinSessionCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UToyBoxSessionSubsystem::HandleJoinSessionComplete));

	if (!Sessions->JoinSession(0, NAME_GameSession, SearchSettings->SearchResults[ResultIndex]))
	{
		ClearDelegates();
		OnJoinComplete.Broadcast(false);
	}
}

void UToyBoxSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	ClearDelegates();

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		OnJoinComplete.Broadcast(false);
		return;
	}

	const IOnlineSessionPtr Sessions = GetSessionInterface();
	FString ConnectString;
	if (!Sessions.IsValid() || !Sessions->GetResolvedConnectString(SessionName, ConnectString))
	{
		OnJoinComplete.Broadcast(false);
		return;
	}

	OnJoinComplete.Broadcast(true);

	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* PC = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	if (PC)
	{
		PC->ClientTravel(ConnectString, TRAVEL_Absolute);
	}
}

void UToyBoxSessionSubsystem::DestroyCurrentSession()
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		return;
	}

	DestroySessionCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UToyBoxSessionSubsystem::HandleDestroySessionComplete));

	Sessions->DestroySession(NAME_GameSession);
}

void UToyBoxSessionSubsystem::HandleDestroySessionComplete(FName /*SessionName*/, bool /*bWasSuccessful*/)
{
	ClearDelegates();
}
