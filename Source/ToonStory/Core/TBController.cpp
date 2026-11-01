// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBController.h"
#include "Algo/AllOf.h"
#include "Audio/TBAudioEvents.h"
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Core/TBLobbySettings.h"
#include "GamePlay/TBHumanEntrance.h"
#include "Core/TBSession.h"
#include "Core/TBChat.h"
#include "Core/TBGameHelpers.h"
#include "Data/TBRuleData.h"
#include "UI/TBChatBox.h"
#include "UI/TBHUD.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/TBInputSettings.h"
#include "Input/TBKeyLabels.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "UI/TBScreenTransition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
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
	bPreparationTravelMask = true;
	bPreparationTravelFailed = false;
	bPreparationBlack = true;
	NextEntranceFootstep = 0;
	TBScreen::HoldBlack(this, true);
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
	UpdateLobbyCamera(InLobby || ShouldHidePreparationView());
	UpdateMenuInput(ShowMenu);
	UpdateScreenTransition();
	UpdateViewAspect();
	SyncInputContexts();
	UpdateChat();
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

void ATBController::ClientPreparationBegin_Implementation(uint32 Round)
{
	PreparationStartRound = Round;
	bPreparationStartMask = true;
	bPreparationBlack = true;
	TBScreen::HoldBlack(this, true);
}

void ATBController::ClientPreparationCancelled_Implementation(uint32 Round)
{
	if (Round != PreparationStartRound) return;
	bPreparationStartMask = false;
	bPreparationTravelMask = false;
}

void ATBController::ClientPreparationTravelCancelled_Implementation()
{
	// Does not cancel a newer start. Only releases the old travel mask once pending really ends.
	bPreparationTravelFailed = true;
}

void ATBController::PreparationTravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
	if (World != GetWorld()) return;
	if (HasAuthority()) ClientPreparationTravelCancelled();
	else if (IsLocalController()) bPreparationTravelFailed = true;
}

void ATBController::ClientPreparationRole_Implementation(uint32 Round, ETBTeam Team)
{
	PreparationViewRound = Round;
	PreparationViewTeam = Team;
}

bool ATBController::ShouldHidePreparationView() const
{
	if (GetWorld()->GetMapName().EndsWith(TEXT("Title"))) return false;
	const auto* State = TB::GS(GetWorld());
	const auto* Info = GetPlayerState<ATBPlayerState>();
	if (!State) return true;
	if (bPreparationStartMask && (State->Phase != ETBPhase::Playing || !State->HidePreparation.bValid ||
	    State->HidePreparation.Round != PreparationStartRound || PreparationViewRound != PreparationStartRound ||
	    !Info || Info->Team != PreparationViewTeam)) return true;
	if (State->Phase == ETBPhase::Results || State->Phase == ETBPhase::Aborted) return false;
	if (State->Phase == ETBPhase::Lobby)
		return State->bTravelPending || (bPreparationTravelMask && !bPreparationTravelFailed && !TBLobbySettings::IsLobby(GetWorld()));
	if (!Info || Info->Team == ETBTeam::None || !State->HidePreparation.bValid) return true;
	if (State->HidePreparation.Round != PreparationViewRound || Info->Team != PreparationViewTeam) return true;
	if (LastPhase != ETBPhase::Playing && State->HidePreparation.Round == LastCompletedPreparationRound) return true;
	return State->HidePreparation.bHiding && Info->Team == ETBTeam::Human;
}

// 切替はサーバーで確定して届くので、自分の状態を見てから演出を出す。
void ATBController::UpdateScreenTransition()
{
	const auto* State = TB::GS(GetWorld());
	const auto* Info = GetPlayerState<ATBPlayerState>();
	const bool WasApproach = bEntranceApproach;
	bEntranceApproach = false;
	const bool Hold = ShouldHidePreparationView();
	if (Hold)
	{
		bPreparationBlack = true;
		if (State && State->Phase == ETBPhase::Playing && State->HidePreparation.bValid &&
		    State->HidePreparation.bHiding && Info && Info->Team == ETBTeam::Human &&
		    PreparationViewTeam == Info->Team && PreparationViewRound == State->HidePreparation.Round &&
		    (!bPreparationStartMask || PreparationStartRound == State->HidePreparation.Round))
		{
			if (const auto* Entrance = ATBHumanEntrance::Find(GetWorld()))
			{
				FVector Location; FRotator Rotation;
				if (Entrance->CameraPose(State->HideProgress(), Location, Rotation))
				{
					if (!EntranceCamera.IsValid()) EntranceCamera = GetWorld()->SpawnActor<ACameraActor>();
					if (EntranceCamera.IsValid())
					{
						EntranceCamera->SetActorLocationAndRotation(Location, Rotation);
						SetViewTarget(EntranceCamera.Get());
						bEntranceApproach = true;
						TBScreen::HoldBlack(this, false);
						if (GetWorld()->GetTimeSeconds() >= NextEntranceFootstep)
						{
							NextEntranceFootstep = GetWorld()->GetTimeSeconds() + Entrance->FootstepIntervalSeconds;
							if (auto* Audio = GetGameInstance()->GetSubsystem<UTBAudioSubsystem>())
								Audio->PlaySoundEvent(ETBSoundEvent::HumanFootstep);
						}
						return;
					}
				}
			}
		}
		TBScreen::HoldBlack(this, true); // Reapply to a replacement camera manager after travel.
		return;
	}
	if (WasApproach)
	{
		if (GetPawn()) SetViewTarget(GetPawn());
		if (State && State->Phase == ETBPhase::Playing && !State->IsPreparing())
			if (const auto* Entrance = ATBHumanEntrance::Find(GetWorld()); Entrance && Entrance->DoorSound)
			{
				const auto* Audio = GetGameInstance()->GetSubsystem<UTBAudioSubsystem>();
				const float Volume = Audio ? Audio->GetMasterVolume() * Audio->GetEffectsVolume() : 1.f;
				UGameplayStatics::PlaySound2D(this, Entrance->DoorSound, Volume);
			}
		if (EntranceCamera.IsValid()) EntranceCamera->Destroy();
		EntranceCamera.Reset();
	}
	if (bPreparationBlack)
	{
		TBScreen::HoldBlack(this, false);
		bPreparationBlack = false;
	}
	bPreparationTravelMask = false;
	bPreparationStartMask = false;
	if (State && State->Phase == ETBPhase::Playing && State->HidePreparation.bValid && !State->HidePreparation.bHiding)
		LastCompletedPreparationRound = State->HidePreparation.Round;
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
		if (bPreparationBlack) SetViewTarget(GetPawn());
		else SetViewTargetWithBlend(GetPawn(), .35f);
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
		ApplyMenuInputMode();
	}
}

void ATBController::ApplyMenuInputMode()
{
	if (bLobbyInput)
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

void ATBController::BeginPlay()
{
	Super::BeginPlay();
	if (GEngine)
		PreparationTravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ATBController::PreparationTravelFailed);
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
	if (GEngine) GEngine->OnTravelFailure().Remove(PreparationTravelFailureHandle);
	ClearInputContexts();
	Super::EndPlay(Reason);
}

// 陣営はPlayerStateのTeamで決まる。ホストでは通知なしに書き換わるので、Pawnの持ち替えではなくTeamを毎フレーム見る。
void ATBController::SyncInputContexts()
{
	const auto* Settings = GetDefault<UTBInputSettings>();
	const auto* Info = GetPlayerState<ATBPlayerState>();
	const ETBTeam Team = Info ? Info->Team : ETBTeam::None;
	const auto& RoleContext = Team == ETBTeam::Human ? Settings->HumanContext
	                        : Team == ETBTeam::Toy   ? Settings->ToyContext
	                                                 : TSoftObjectPtr<UInputMappingContext>();
	// ロビーのキーはロビーの間だけ。GameStateが無いときは付けない。
	const auto* State = TB::GS(GetWorld());
	const auto& LobbyContext = State && State->Phase == ETBPhase::Lobby ? Settings->LobbyContext
	                                                                     : TSoftObjectPtr<UInputMappingContext>();
	TArray<TWeakObjectPtr<const UInputMappingContext>> Wanted;
	for (const auto* Soft : {&Settings->CommonContext, &LobbyContext, &RoleContext})
	{
		if (const auto* Context = Soft->IsNull() ? nullptr : Soft->LoadSynchronous())
		{
			Wanted.Add(Context);
		}
	}
	auto* Input = IsLocalController() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()) : nullptr;
	// 記録が同じでも、エンジン側で外されていたら付け直す。
	if (Wanted == AppliedContexts &&
	    (!Input || Algo::AllOf(AppliedContexts, [Input](const auto& Context) { return Context.IsValid() && Input->HasMappingContext(Context.Get()); })))
	{
		return;
	}
	ClearInputContexts();
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

// UserSettingsはSuper::ReceivedPlayerで作られる。直後の最初のGCより前に、キー設定用にIMCを登録する。
void ATBController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		UTBInputSettings::RegisterContexts(GetLocalPlayer());
	}
}

void ATBController::SetupInputComponent()
{
	Super::SetupInputComponent();
	auto* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Enhanced)
	{
		return;
	}
	const auto* Settings = GetDefault<UTBInputSettings>();
	if (const UInputAction* Menu = Settings->MenuAction.LoadSynchronous())
	{
		Enhanced->BindAction(Menu, ETriggerEvent::Started, this, &ATBController::ToggleLeaveMenu);
	}
	if (const UInputAction* Ready = Settings->ReadyAction.LoadSynchronous())
	{
		Enhanced->BindAction(Ready, ETriggerEvent::Started, this, &ATBController::TBReady);
	}
	if (const UInputAction* Start = Settings->StartAction.LoadSynchronous())
	{
		Enhanced->BindAction(Start, ETriggerEvent::Started, this, &ATBController::TBStart);
	}
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
	// キー名は{Mappable名}の印で届く。自分の割り当てに置き換える。
	Notice = TB::ExpandKeyTokens(this, Message);
	NoticeExpires = GetWorld()->GetTimeSeconds() + 5;
	if (bError) ClientPlaySoundEvent(ETBSoundEvent::UIError, FVector::ZeroVector);
}

bool ATBController::CanUseChat() const
{
	const auto* State = TB::GS(GetWorld());
	if (!IsLocalController() || !State || !TB::Chat::IsAvailable(State->Phase) || ShouldHidePreparationView())
	{
		return false;
	}
	const auto* HUD = Cast<ATBHUD>(GetHUD());
	return !HUD || !HUD->IsChatBlocked();
}

bool ATBController::IsChatOpen() const
{
	return ChatBox && ChatBox->IsOpen();
}

// 使える間だけ画面に置く。使えなくなったら（試合開始、設定やオプションを開いた）閉じて外す。履歴はUTBChatLogに残る。
void ATBController::UpdateChat()
{
	if (!CanUseChat())
	{
		if (ChatBox)
		{
			CloseChat();
			ChatBox->RemoveFromParent();
			ChatBox = nullptr;
		}
		return;
	}
	if (!ChatBox)
	{
		ChatBox = CreateWidget<UTBChatBox>(this);
		if (!ChatBox)
		{
			return;
		}
		// オプション画面（50）より奥。
		ChatBox->AddToViewport(40);
	}
	ChatBox->SetResultsLayout(TB::GS(GetWorld())->Phase != ETBPhase::Lobby);
}

bool ATBController::InputKey(const FInputKeyEventArgs& Params)
{
	// 開いている間のEnterは入力欄が受け取るので、ここへは閉じているときだけ来る。
	if (Params.Key == EKeys::Enter && Params.Event == IE_Pressed && ChatBox && !ChatBox->IsOpen())
	{
		OpenChat();
		return true;
	}
	return Super::InputKey(Params);
}

void ATBController::OpenChat()
{
	if (ChatBox && !ChatBox->IsOpen())
	{
		ChatBox->SetOpen(true);
	}
}

void ATBController::CloseChat()
{
	if (!IsChatOpen())
	{
		return;
	}
	ChatBox->SetOpen(false);
	// 試合が始まった同じフレームで閉じることもあるので、入力モードはロビー用と決めつけない。
	ApplyMenuInputMode();
	// ロビー用の入力モードはフォーカスを動かさないので、隠した入力欄から画面へ戻す。戻さないとEnterやロビーのキーが効かない。
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

bool ATBController::SendChat(const FString& Text)
{
	const double Now = FPlatformTime::Seconds();
	if (TB::Chat::IsTooSoon(LastChatSent, Now, TB::Rules().Chat.SendIntervalSeconds))
	{
		return false;
	}
	LastChatSent = Now;
	ServerSendChat(Text);
	return true;
}

void ATBController::ServerSendChat_Implementation(const FString& Text)
{
	const auto* State = TB::GS(GetWorld());
	const FString Message = TB::Chat::Clean(Text);
	const double Now = FPlatformTime::Seconds();
	if (!State || !TB::Chat::IsAvailable(State->Phase) || !PlayerState || Message.IsEmpty() ||
	    TB::Chat::IsTooSoon(LastChatAccepted, Now, TB::Rules().Chat.SendIntervalSeconds * TB::Chat::ServerSlack))
	{
		return;
	}
	LastChatAccepted = Now;
	const FString Name = TB::Chat::DisplayName(PlayerState);
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (auto* PC = Cast<ATBController>(It->Get()))
		{
			PC->ClientReceiveChat(Name, Message, PlayerState->GetPlayerId(), PlayerState->GetUniqueId().ToString());
		}
	}
}

void ATBController::ClientReceiveChat_Implementation(const FString& Name, const FString& Text, int32 SenderId,
                                                     const FString& SenderNetId)
{
	auto* Log = GetGameInstance()->GetSubsystem<UTBChatLog>();
	if (!Log)
	{
		return;
	}
	const bool bSelf = PlayerState && PlayerState->GetPlayerId() == SenderId;
	Log->Add(Name, TB::Chat::Filter(GetWorld(), TB::Chat::Clean(Text), SenderNetId), bSelf);
	// 画面のほかの所を見ていても届いたと分かるよう、ほかの人の発言だけ鳴らす。
	if (!bSelf)
	{
		ClientPlaySoundEvent(ETBSoundEvent::ChatMessage, FVector::ZeroVector);
	}
}

// 出入りの音はロビーの人数の変化で別に鳴るので、ここでは鳴らさない。
void ATBController::ClientReceiveChatNotice_Implementation(ETBChatNotice Kind, const FString& Name)
{
	if (auto* Log = GetGameInstance()->GetSubsystem<UTBChatLog>())
	{
		Log->AddNotice(TB::Chat::NoticeText(Kind, TB::Chat::Clean(Name)));
	}
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
