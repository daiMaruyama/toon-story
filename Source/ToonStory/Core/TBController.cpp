// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBController.h"
#include "Audio/TBAudioEvents.h"
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Core/TBSession.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBInputSettings.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "UI/TBScreenTransition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/AudioComponent.h"
#include "UObject/UObjectIterator.h"
#include "VoipListenerSynthComponent.h"

namespace
{
	// ほかの人の声を鳴らす部品は、エンジンがステージを移ったあとにしか片付けない。片付けるステージ（移る途中のつなぎの
	// ステージも含む）に登録されたまま残ると、あとで音の処理が消えた音を触って落ちるので、片付けのたびに止めて外す。
	// 声がまた届けば、エンジンが今のステージに作り直す。
	void ReleaseVoicePlayback(UWorld* World, bool, bool)
	{
		for (TObjectIterator<UVoipListenerSynthComponent> It; It; ++It)
		{
			if (It->GetWorld() != World)
			{
				continue;
			}
			It->Stop();
			if (UAudioComponent* Audio = It->GetAudioComponent(); Audio && Audio->IsRegistered())
			{
				Audio->UnregisterComponent();
			}
			if (It->IsRegistered())
			{
				It->UnregisterComponent();
			}
		}
	}
}

void ATBController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	ReleaseVoicePlayback(GetWorld(), false, false);
	SetViewTarget(this);
	LobbyCamera.Reset();
	bLobbyCameraSearched = false;
	bLobbyCameraActive = false;
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void ATBController::ClientRoomClosed_Implementation()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>()) Session->RoomClosed();
}

void ATBController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController())
	{
		return;
	}
	const auto* State = TB::GS(GetWorld());
	const bool InLobby = State && State->Phase == ETBPhase::Lobby;
	if (!State || State->Phase != ETBPhase::Playing) bLeaveMenuOpen = false;
	const bool ShowMenu = State && (State->Phase != ETBPhase::Playing || bLeaveMenuOpen);
	UpdateLobbyCamera(InLobby);
	UpdateMenuInput(ShowMenu);
	UpdateScreenTransition();
	UpdateViewAspect();
	SyncInputContexts();
	if (NoticeExpires > 0 && GetWorld()->GetTimeSeconds() > NoticeExpires)
	{
		Notice.Reset();
		NoticeExpires = 0;
	}
}

// 画面比を測る。基準より横長なら黒帯を出し（ATBCharacter::CalcCamera）、画面が狭い人の視界の判定を映っている
// 範囲に合わせられるよう、変わったときだけサーバーへ伝える。
void ATBController::UpdateViewAspect()
{
	int32 SizeX = 0;
	int32 SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0)
	{
		return; // 最小化中など
	}
	const float Aspect = FMath::Clamp(float(SizeX) / SizeY, .25f, 8.f);
	if (FMath::IsNearlyEqual(Aspect, ViewAspect, .001f))
	{
		return;
	}
	ViewAspect = Aspect;
	if (!HasAuthority())
	{
		ServerSetViewAspect(Aspect);
	}
}

void ATBController::ServerSetViewAspect_Implementation(float Aspect)
{
	// 判定では基準の画面比より広げる方向には使わないので、ここでは壊れた値だけ弾く。
	if (FMath::IsFinite(Aspect))
	{
		ViewAspect = FMath::Clamp(Aspect, .25f, 8.f);
	}
}

// 切替はサーバーで確定して届くので、自分の状態を見てから演出を出す。
void ATBController::UpdateScreenTransition()
{
	const auto* State = TB::GS(GetWorld());
	const auto* Info = GetPlayerState<ATBPlayerState>();
	if (!State || !Info)
	{
		return;
	}
	// ロビーに戻る
	const bool bReturnedToLobby = State->Phase == ETBPhase::Lobby && LastPhase != ETBPhase::Lobby;
	// 箱に入れられる、助け出される、はどちらも箱の部屋との間を瞬間移動する。
	const bool bBoxed = Info->ToyState == ETBToyState::Boxed;
	const bool bWasBoxed = LastToyState == ETBToyState::Boxed;
	// 最後の1体の収納はそのまま試合終了になるので、ロビー以外なら出す。
	const bool bEnteredBox = bBoxed && !bWasBoxed && State->Phase != ETBPhase::Lobby;
	// 救助は試合中だけ。ロビーへ戻る時の状態リセットが先に届いても出さない（二重の暗転を防ぐ）。
	const bool bLeftBox = !bBoxed && bWasBoxed && State->Phase == ETBPhase::Playing;
	// 箱に入れられる時とロビーへの移動は暗転。助け出される時はフタが開く光を白で表す。
	if (bLeftBox)
	{
		TBScreen::PlayTransition(this, FLinearColor::White);
	}
	else if (bEnteredBox || bReturnedToLobby)
	{
		TBScreen::PlayTransition(this);
	}
	LastPhase = State->Phase;
	LastToyState = Info->ToyState;
}

void ATBController::UpdateLobbyCamera(bool InLobby)
{
	// ロビー中はマップに置いたTBLobbyCameraから見せる。Pawnの所持で視点が戻されても毎フレーム掛け直す。
	if (InLobby)
	{
		if (!bLobbyCameraSearched)
		{
			bLobbyCameraSearched = true;
			for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
			{
				if (It->ActorHasTag(TEXT("TBLobbyCamera")))
				{
					LobbyCamera = *It;
					It->GetCameraComponent()->bConstrainAspectRatio = false;
					break;
				}
			}
		}
		if (LobbyCamera.IsValid() && GetViewTarget() != LobbyCamera.Get())
		{
			SetViewTarget(LobbyCamera.Get());
			bLobbyCameraActive = true;
		}
	}
	else if (bLobbyCameraActive && GetPawn())
	{
		SetViewTargetWithBlend(GetPawn(), .35f);
		bLobbyCameraActive = false;
	}
}

void ATBController::UpdateMenuInput(bool ShowMenu)
{
	if (bLobbyInput != ShowMenu)
	{
		bLobbyInput = ShowMenu;
		bShowMouseCursor = ShowMenu;
		bEnableClickEvents = ShowMenu;
		SetIgnoreLookInput(ShowMenu);
		SetIgnoreMoveInput(ShowMenu);
		if (ShowMenu)
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			SetInputMode(Mode);
		}
		else
		{
			SetInputMode(FInputModeGameOnly());
		}
	}
}

void ATBController::BeginPlay()
{
	Super::BeginPlay();
	// 切断でタイトルへ戻るときや、移る途中のつなぎのステージの片付けでも外すよう、片付けの通知に一度だけ登録する。
	static const FDelegateHandle VoiceCleanup = FWorldDelegates::OnWorldCleanup.AddStatic(&ReleaseVoicePlayback);
	if (IsLocalController())
	{
		if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
		{
			Session->Activate();
		}
	}
}

void ATBController::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearInputContexts();
	Super::EndPlay(Reason);
}

// 陣営は PlayerState の Team で決まる。ホストでは通知なしに書き換わるので、Pawn の持ち替えではなく Team を毎フレーム見る。
void ATBController::SyncInputContexts()
{
	const auto* Settings = GetDefault<UTBInputSettings>();
	const auto* Info = GetPlayerState<ATBPlayerState>();
	const ETBTeam Team = Info ? Info->Team : ETBTeam::None;
	const auto& RoleContext = Team == ETBTeam::Human ? Settings->HumanContext
	                        : Team == ETBTeam::Toy   ? Settings->ToyContext
	                                                 : TSoftObjectPtr<UInputMappingContext>();
	TArray<TWeakObjectPtr<const UInputMappingContext>> Wanted;
	for (const auto* Soft : {&Settings->CommonContext, &RoleContext})
	{
		if (const auto* Context = Soft->IsNull() ? nullptr : Soft->LoadSynchronous())
		{
			Wanted.Add(Context);
		}
	}
	if (Wanted == AppliedContexts)
	{
		return;
	}
	ClearInputContexts();
	auto* Input = IsLocalController() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
	if (!Input)
	{
		return;
	}
	for (int32 Index = 0; Index < Wanted.Num(); ++Index)
	{
		Input->AddMappingContext(Wanted[Index].Get(), Index);
		AppliedContexts.Add(Wanted[Index]);
	}
}

void ATBController::ClearInputContexts()
{
	if (auto* Input = AppliedContexts.IsEmpty() ? nullptr : ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (const auto& Context : AppliedContexts)
		{
			if (Context.IsValid()) Input->RemoveMappingContext(Context.Get());
		}
	}
	AppliedContexts.Reset();
}

void ATBController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ATBController::ToggleLeaveMenu);
	InputComponent->BindAction("Ready", IE_Pressed, this, &ATBController::TBReady);
	InputComponent->BindAction("Start", IE_Pressed, this, &ATBController::TBStart);
}

void ATBController::ToggleLeaveMenu()
{
	const auto* State = TB::GS(GetWorld());
	if (State && State->Phase == ETBPhase::Playing) bLeaveMenuOpen = !bLeaveMenuOpen;
}

void ATBController::TBHost()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Host();
	}
}

void ATBController::TBFind()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Find();
	}
}

void ATBController::TBJoin(int32 Index)
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Join(Index);
	}
}

void ATBController::TBLeave()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Leave();
	}
}

void ATBController::TBQuit()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ATBController::TBReady()
{
	ServerReady();
}

void ATBController::TBLobby()
{
	ServerReturnToLobby();
}

void ATBController::ServerReturnToLobby_Implementation()
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->ReturnToLobby(this);
	}
}

void ATBController::TBStart()
{
	ServerStart();
}

void ATBController::TBRules(int32 HumanCount, int32 ToyCount, float Seconds, int32 Items)
{
	FTBSettings Settings;
	if (const auto* State = TB::GS(GetWorld())) Settings = State->Settings;
	Settings.Humans = HumanCount;
	Settings.Toys = ToyCount;
	Settings.Duration = Seconds;
	Settings.RequiredItems = Items;
	ServerRules(Settings);
}

void ATBController::TBPrefer(int32 Team)
{
	ServerPreference(Team == 1 ? ETBTeam::Human : Team == 2 ? ETBTeam::Toy : ETBTeam::None);
}

void ATBController::ServerReady_Implementation()
{
	auto* MatchInfo = TB::GS(GetWorld());
	auto* ToyPlayerInfo = GetPlayerState<ATBPlayerState>();
	if (MatchInfo && MatchInfo->Phase == ETBPhase::Lobby && !MatchInfo->bTravelPending && ToyPlayerInfo)
	{
		ToyPlayerInfo->bReady = !ToyPlayerInfo->bReady;
		ToyPlayerInfo->ForceNetUpdate();
		UTBAudioEvents::Emit(this, ToyPlayerInfo->bReady ? ETBSoundEvent::Ready : ETBSoundEvent::Unready, ToyPlayerInfo, FVector::ZeroVector);
	}
}

void ATBController::ServerStart_Implementation()
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->StartRound(this);
	}
}

void ATBController::ServerRules_Implementation(FTBSettings Rules)
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->SetRules(this, Rules);
	}
}

void ATBController::ServerPreference_Implementation(ETBTeam Team)
{
	auto* MatchInfo = TB::GS(GetWorld());
	auto* ToyPlayerInfo = GetPlayerState<ATBPlayerState>();
	if (MatchInfo && MatchInfo->Phase == ETBPhase::Lobby && !MatchInfo->bTravelPending && ToyPlayerInfo &&
	    (Team == ETBTeam::None || Team == ETBTeam::Human || Team == ETBTeam::Toy))
	{
		if (ToyPlayerInfo->Preference != Team) ClientPlaySoundEvent(ETBSoundEvent::UIChange, FVector::ZeroVector);
		ToyPlayerInfo->Preference = Team;
		ToyPlayerInfo->ForceNetUpdate();
		if (auto* Mode = GetWorld()->GetAuthGameMode<ATBGameMode>()) Mode->SyncLobbyCounts();
	}
}

void ATBController::ClientNotice_Implementation(const FString& Message, bool bError)
{
	Notice = Message;
	NoticeExpires = GetWorld()->GetTimeSeconds() + 5;
	if (bError) ClientPlaySoundEvent(ETBSoundEvent::UIError, FVector::ZeroVector);
}

void ATBController::ClientPlaySoundEvent_Implementation(ETBSoundEvent Event, FVector Location)
{
	// A shared output uses the same primary listener as voice, BGM and team-only loops.
	if (!IsLocalController() || !GetGameInstance() ||
	    GetGameInstance()->GetFirstLocalPlayerController(GetWorld()) != this)
	{
		return;
	}
	if (auto* Audio = GetGameInstance()->GetSubsystem<UTBAudioSubsystem>())
	{
		Audio->PlaySoundEvent(Event, Location);
	}
}
