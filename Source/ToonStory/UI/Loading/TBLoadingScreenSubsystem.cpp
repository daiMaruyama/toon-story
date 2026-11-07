// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBLoadingScreenSubsystem.h"
#include "STBLoadingScreen.h"
#include "TBLoadingScreenSettings.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBSession.h"
#include "UI/TBPaperStyle.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "Misc/PackageName.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWeakWidget.h"

bool UTBLoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && GetDefault<UTBLoadingScreenSettings>()->bEnabled;
}

void UTBLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	for (const auto& Path : GetDefault<UTBLoadingScreenSettings>()->Toys)
	{
		if (UTexture2D* Texture = Path.LoadSynchronous())
		{
			Texture->bForceMiplevelsToBeResident = true;
			ToyTextures.Add(Texture);
		}
	}
	PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ThisClass::PreLoadMap);
	PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::PostLoadMap);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::Failed);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::TravelFailed);
	}
}

void UTBLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	StopMovie();
	RemoveOverlay();
	MovieScreen.Reset();
	Super::Deinitialize();
}

// マップを丸ごと読み直す直前。読み込みの間はゲームの処理が止まるので、MoviePlayer に描かせる。
void UTBLoadingScreenSubsystem::PreLoadMap(const FString& MapName)
{
	const UWorld* World = GetGameInstance()->GetWorld();
	// 起動直後の最初の読み込みと、エディタのプレイには出さない。裏で切り替える移動はかぶせる方で扱う。
	if (!bHadWorld || GIsEditor || !IsMoviePlayerEnabled() || (World && World->IsInSeamlessTravel())) return;
	RemoveOverlay();
	MovieScreen = SNew(STBLoadingScreen);
	// 部屋の操作中ならその状態を、そうでなければ行き先から決める（部屋をつくった直後は操作が終わっている）。
	const auto* Session = GetGameInstance()->GetSubsystem<UTBSession>();
	const bool bToTitle = FPackageName::GetShortName(MapName) == FPackageName::GetShortName(UGameMapsSettings::GetGameDefaultMap());
	MovieScreen->SetStatus(FText::FromString(Session && Session->IsBusy() ? TBPaper::Status(Session->Status)
	                                         : bToTitle                   ? FString(TEXT("タイトルにもどっています…"))
	                                                                      : FString(TEXT("部屋に入っています…"))));
	MovieScreen->Restart();
	FLoadingScreenAttributes Attributes;
	Attributes.WidgetLoadingScreen = MovieScreen;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;
	Attributes.bMoviesAreSkippable = false;
	Attributes.MinimumLoadingScreenDisplayTime = 0.f;
	GetMoviePlayer()->SetupLoadingScreen(Attributes);
	if (!GetMoviePlayer()->IsMovieCurrentlyPlaying()) GetMoviePlayer()->PlayMovie();
	bHardLoad = true;
}

// 読み終わったら、同じ絵をゲーム画面にかぶせ直し、キャラが出るまで待ってから薄れさせる。
void UTBLoadingScreenSubsystem::PostLoadMap(UWorld* World)
{
	bHadWorld = true;
	if (!bHardLoad) return;
	LoadedAt = FPlatformTime::Seconds();
	ShowOverlay();
}

void UTBLoadingScreenSubsystem::Failed(UWorld*, UNetDriver*, ENetworkFailure::Type, const FString&)
{
	StopMovie();
	bHardLoad = false;
}

void UTBLoadingScreenSubsystem::TravelFailed(UWorld*, ETravelFailure::Type, const FString&)
{
	StopMovie();
	bHardLoad = false;
}

void UTBLoadingScreenSubsystem::StopMovie()
{
	if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying()) GetMoviePlayer()->StopMovie();
}

void UTBLoadingScreenSubsystem::ShowOverlay()
{
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	if (!Viewport) return;
	// 読み込み直後にエンジンが画面の部品を組み直して外すことがあるので、外れていたら付け直す。
	if (Overlay && OverlayViewport.Get() == Viewport && Overlay->IsParentValid()) return;
	const TSharedPtr<STBLoadingScreen> Keep = Screen;
	RemoveOverlay();
	Screen = Keep;
	if (!Screen)
	{
		Screen = SNew(STBLoadingScreen);
		if (bHardLoad && MovieScreen) Screen->ContinueFrom(*MovieScreen);
	}
	Overlay = SNew(SWeakWidget).PossiblyNullContent(Screen);
	// チャットやメニューより手前、確認用の表示より奥。
	Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 900);
	OverlayViewport = Viewport;
}

void UTBLoadingScreenSubsystem::RemoveOverlay()
{
	if (Overlay && OverlayViewport.IsValid()) OverlayViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
	Overlay.Reset();
	OverlayViewport.Reset();
	Screen.Reset();
}

// コントローラーが画面を黒くしている間（試合への移動・配置待ち・ロビーへの戻り）。
// 人間がおもちゃの隠れるのを待つカウントダウンは試合の一部なので、かぶせずにそのまま見せる。
bool UTBLoadingScreenSubsystem::ShouldCoverSeamless() const
{
	const auto* Controller = Cast<ATBController>(GetGameInstance()->GetFirstLocalPlayerController());
	if (!Controller || !Controller->ShouldHidePreparationView() || Controller->IsShowingEntranceApproach()) return false;
	const auto* State = Controller->GetWorld() ? Controller->GetWorld()->GetGameState<ATBGameState>() : nullptr;
	const auto* Info = Controller->GetPlayerState<ATBPlayerState>();
	const bool bCountdown = State && Info && State->Phase == ETBPhase::Playing && Info->Team == ETBTeam::Human &&
	                        State->HidePreparation.bValid && State->HidePreparation.bHiding;
	return !bCountdown;
}

void UTBLoadingScreenSubsystem::Tick(float DeltaTime)
{
	const auto* Settings = GetDefault<UTBLoadingScreenSettings>();
	bool bCover = ShouldCoverSeamless();
	// ロビーと試合の切り替わり。読み込みはほとんどないが、いきなり画面が変わらないよう、少しの間かぶせてフェードでつなぐ。
	UWorld* World = GetGameInstance()->GetWorld();
	if (const auto* State = World ? World->GetGameState<ATBGameState>() : nullptr)
	{
		const uint8 Phase = uint8(State->Phase);
		if (PhaseWorld.Get() == World && Phase != LastPhase &&
		    (Phase == uint8(ETBPhase::Lobby) || LastPhase == uint8(ETBPhase::Lobby)))
		{
			SwitchUntil = FPlatformTime::Seconds() + Settings->MatchSwitchSeconds;
		}
		// ロビーに戻ってきたところか、試合へ向かうところか。試合への移動はロビーのうちに始まる。
		bReturning = State->Phase == ETBPhase::Lobby && !State->bTravelPending;
		PhaseWorld = World;
		LastPhase = Phase;
	}
	if (FPlatformTime::Seconds() < SwitchUntil) bCover = true;
	// 読み終わっても、MoviePlayer はしばらく画面の一番上に出たまま。消えるまでは下のかぶせを薄れさせない。
	if (bHardLoad && IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		LoadedAt = FPlatformTime::Seconds();
		bCover = true;
	}
	else if (bHardLoad)
	{
		// キャラのいない画面（タイトル）は待たずに明ける。
		const auto* Controller = GetGameInstance()->GetFirstLocalPlayerController();
		const bool bReady = Controller && (Controller->GetPawn() || !Controller->IsA<ATBController>() ||
		                                   FPlatformTime::Seconds() - LoadedAt > Settings->PawnWaitSeconds);
		if (bReady) bHardLoad = false;
		else bCover = true;
	}
	if (bCover)
	{
		const bool bNew = !Overlay;
		ShowOverlay();
		if (!Screen) return;
		if (bNew && !bHardLoad)
		{
			// 裏で切り替える移動は、ここから出しはじめる。読み直しのあとは、ロード中の絵をそのまま続ける。
			Screen->SetStatus(FText::FromString(bReturning ? TEXT("ロビーにもどっています…") : TEXT("あそぶ準備をしています…")));
			Screen->Restart();
		}
		Screen->SetExit(0.f);
		return;
	}
	if (!Overlay || !Screen) return;
	ShowOverlay();
	// 読み込み直後の1フレームは、読み込みにかかった時間がまるごと入るので、一度に進めすぎない。
	const float Step = FMath::Min(DeltaTime, 1.f / 30.f);
	const float Exit = Settings->FadeOutSeconds > 0.f ? Screen->GetExit() + Step / Settings->FadeOutSeconds : 1.f;
	Screen->SetExit(Exit);
	if (Exit >= 1.f) RemoveOverlay();
}
