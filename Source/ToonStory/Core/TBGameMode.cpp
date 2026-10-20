// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "TBLobbySettings.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Core/TBSession.h"
#include "Core/TBGameHelpers.h"
#include "Character/TBCharacter.h"
#include "Character/TBToyCharacter.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBDummyToy.h"
#include "GamePlay/TBPickup.h"
#include "GamePlay/TBBlock.h"
#include "GamePlay/TBRuleMath.h"
#include "Data/TBRuleData.h"
#include "UI/TBHUD.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/ConstructorHelpers.h"

void ATBGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		SpawnPoints.Add(*It);
	}
	bBuildTestArena = !TBLobbySettings::IsLobby(GetWorld()) && bBuildTestArena && !TActorIterator<ATBBox>(GetWorld());
}

// 部屋を作った直後のルールはマスターデータから決める。移動後は TBLobbySettings::Restore が引き継いだ値で上書きする。
void ATBGameMode::InitGameState()
{
	Super::InitGameState();
	if (auto* MatchInfo = GetGameState<ATBGameState>())
	{
		const auto& Match = TB::Rules().Match;
		// 範囲の外の初期値だと、ロビーでルールを変えようとしても SetRules がすべて断ってしまう。
		MatchInfo->Settings.Duration = FMath::Clamp(Match.DefaultDuration, Match.MinDuration, Match.MaxDuration);
		MatchInfo->Settings.RequiredItems =
		    FMath::Clamp(Match.DefaultRequiredItems, 1, FMath::Max(1, TBLobbySettings::AvailableItems(GetWorld())));
	}
}

ATBGameMode::ATBGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PlayerStateClass = ATBPlayerState::StaticClass();
	GameStateClass = ATBGameState::StaticClass();
	PlayerControllerClass = ATBController::StaticClass();
	// おもちゃの候補や見た目はBPで設定し、未導入時は既存キャラクターを使う。
	static ConstructorHelpers::FClassFinder<APawn> ToyPawn(TEXT("/Game/Characters/Toys/BP_ToyCharacter"));
	DefaultPawnClass = ToyPawn.Succeeded() ? ToyPawn.Class.Get() : ATBCharacter::StaticClass();
	HUDClass = ATBHUD::StaticClass();
	BoxClass = ATBBox::StaticClass();
}

void ATBGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (bBuildTestArena)
	{
		BuildArena();
	}
	if (auto* MatchInfo = TB::GS(GetWorld()))
	{
		const FString Map = GetWorld()->GetMapName();
		MatchInfo->PlayerLimit = GameSession ? GameSession->MaxPlayers : 0;
		MatchInfo->StageName = Map.Contains(TEXT("ToyWorldDemo")) ? TEXT("ToyWorld Demo")
		                       : Map.Contains(TEXT("ToyWorld"))   ? TEXT("紙のひみつクラブ")
		                       : Map.Contains(TEXT("KidsRoom"))   ? TEXT("Kids' Rooms")
		                       : Map.Contains(TEXT("ArchViz"))    ? TEXT("ArchViz Apartment")
		                                                          : TEXT("Test Arena");
		TActorIterator<ATBBox> It(GetWorld());
		if (It)
		{
			MatchInfo->Box = *It;
		}
	}
	TBLobbySettings::Restore(this);
	SyncLobbyCounts();
}

void ATBGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	PlaceInLobby(NewPlayer);
}

bool ATBGameMode::PlaceInLobby(AController* Player)
{
	const auto* State = TB::GS(GetWorld());
	auto* Pawn = Player ? Cast<ATBCharacter>(Player->GetPawn()) : nullptr;
	if (!State || State->Phase != ETBPhase::Lobby || !Pawn) return false;
	Pawn->ApplyState();
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("TBLobbyCamera"))) continue;
		const auto* Camera = It->GetCameraComponent();
		const FVector Eye = Camera->GetComponentLocation();
		const FVector Forward = Camera->GetForwardVector();
		const FVector Right = Camera->GetRightVector();
		const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
		FCollisionQueryParams Query(SCENE_QUERY_STAT(LobbySpawn), false, Pawn);
		// Search visible, walkable ground in front of the actual authored lobby camera.
		for (float Distance : {350.f, 500.f, 750.f, 1000.f, 1400.f, 1900.f})
		{
			// Keep the dedicated lobby group above the bottom ready panel.
			if (TBLobbySettings::IsLobby(GetWorld()) && Distance < 700.f) continue;
			for (float Side : {0.f, -80.f, 80.f, -160.f, 160.f, -280.f, 280.f, -420.f, 420.f})
			{
				const FVector Target = Eye + Forward * Distance + Right * Side;
				FHitResult Floor;
				if (!GetWorld()->SweepSingleByChannel(Floor, FVector(Target.X, Target.Y, Eye.Z),
				    FVector(Target.X, Target.Y, Eye.Z - 2000), FQuat::Identity, ECC_Pawn,
				    FCollisionShape::MakeCapsule(Radius + 4, HalfHeight), Query) ||
				    Floor.bStartPenetrating || Floor.ImpactNormal.Z < .7f) continue;
				const FVector Center = Floor.Location + FVector(0, 0, 3);
				const FVector View = Camera->GetComponentTransform().InverseTransformPosition(Center);
				const float TanHalfFOV = FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * .5f));
				if (View.X < 100 || FMath::Abs(View.Y) + Radius > View.X * TanHalfFOV * .8f ||
				    FMath::Abs(View.Z) + HalfHeight > View.X * TanHalfFOV / 1.7778f * .8f) continue;
				FCollisionQueryParams VisibilityQuery = Query;
				VisibilityQuery.bTraceComplex = true;
				FHitResult Obstacle;
				if (GetWorld()->LineTraceSingleByChannel(Obstacle, Eye, Center, ECC_Visibility, VisibilityQuery))
				{
					continue;
				}
				if (GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn,
				        FCollisionShape::MakeCapsule(Radius + 4, HalfHeight), Query))
				{
					continue;
				}
				if (Pawn->TeleportTo(Center, FRotator(0, (Eye - Center).Rotation().Yaw, 0)))
				{
					Player->SetControlRotation(Pawn->GetActorRotation());
					if (auto* PC = Cast<APlayerController>(Player))
					{
						PC->ClientSetRotation(Pawn->GetActorRotation(), true);
					}
					Pawn->ForceNetUpdate();
					return true;
				}
			}
		}
	}
	return false; // Keep the map's PlayerStart if no safe camera-visible ground exists.
}

void ATBGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It) PlaceInLobby(It->Get());
	TBLobbySettings::Restore(this);
	SyncLobbyCounts();
}

// 検証用の床・壁・箱・開始位置・アイテムをサーバーで一度だけ生成する。
void ATBGameMode::BuildArena()
{
	if (!bBuildTestArena)
	{
		return;
	}
	bBuildTestArena = false;
	auto Block = [this](FVector Position, FVector Scale)
	{
		auto* BlockActor = GetWorld()->SpawnActor<ATBBlock>(Position, FRotator::ZeroRotator);
		if (BlockActor)
		{
			BlockActor->Size = Scale;
			BlockActor->OnRep_Size();
			BlockActor->ForceNetUpdate();
		}
	};
	Block(FVector(0, 0, -30), FVector(60, 60, .6));
	Block(FVector(-3000, 0, 200), FVector(.3, 60, 4));
	Block(FVector(3000, 0, 200), FVector(.3, 60, 4));
	Block(FVector(0, -3000, 200), FVector(60, .3, 4));
	Block(FVector(0, 3000, 200), FVector(60, .3, 4));
	Block(FVector(-600, -500, 160), FVector(2, 8, 3.2));
	Block(FVector(-600, 900, 160), FVector(2, 6, 3.2));
	Block(FVector(1000, -1100, 160), FVector(8, 2, 3.2));
	GetWorld()->SpawnActor<ATBBox>(BoxClass, FVector(1300, 1000, 0), FRotator::ZeroRotator);
	// マップに置かれたPlayerStartがあればそれを使い、なければ定員分を生成する。
	const bool NeedsStarts = SpawnPoints.IsEmpty();
	for (int32 Index = 0; NeedsStarts && GameSession && Index < GameSession->MaxPlayers; ++Index)
	{
		auto* Position = GetWorld()->SpawnActor<APlayerStart>(
		    FVector(-2000 + (Index % 4) * 300, -1800 + (Index / 4) * 350, 120), FRotator::ZeroRotator);
		if (Position)
		{
			SpawnPoints.Add(Position);
		}
	}
	// アイテムの配置数は参加定員とは独立させる。
	for (int32 Index = 0; Index < 8; ++Index)
	{
		GetWorld()->SpawnActor<ATBPickup>(FVector(-1900 + (Index % 4) * 950, -2200 + (Index / 4) * 4400, 15),
		                                  FRotator::ZeroRotator);
	}
}

AActor* ATBGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (bBuildTestArena)
	{
		BuildArena();
	}
	if (!SpawnPoints.IsEmpty())
	{
		return SpawnPoints[SpawnCursor++ % SpawnPoints.Num()];
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ATBGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& Id, FString& Error)
{
	Super::PreLogin(Options, Address, Id, Error);
	if (!Error.IsEmpty())
	{
		return;
	}
	auto* MatchInfo = TB::GS(GetWorld());
	if (MatchInfo && (MatchInfo->Phase != ETBPhase::Lobby || MatchInfo->bTravelPending))
	{
		Error = TEXT("Match already started. Join the next lobby.");
	}
	if (GameSession && GetNumPlayers() >= GameSession->MaxPlayers)
	{
		Error = TEXT("Lobby is full.");
	}
}

void ATBGameMode::PostLogin(APlayerController* PlayerController)
{
	Super::PostLogin(PlayerController);
	SyncLobbyCounts();
	if (auto* ToyCharacter = Cast<ATBCharacter>(PlayerController->GetPawn()))
	{
		ToyCharacter->ApplyState();
	}
}

bool ATBGameMode::IsHost(const APlayerController* PlayerController) const
{
	return PlayerController && PlayerController->IsLocalController() && PlayerController->HasAuthority();
}

int32 ATBGameMode::LobbyPlayerCount() const
{
	int32 Count = 0;
	if (const auto* State = TB::GS(GetWorld()))
	{
		for (const APlayerState* Player : State->PlayerArray)
		{
			Count += Cast<ATBPlayerState>(Player) ? 1 : 0;
		}
	}
	return Count;
}

void ATBGameMode::SyncLobbyCounts()
{
	auto* State = TB::GS(GetWorld());
	if (!State || State->Phase != ETBPhase::Lobby || State->bTravelPending) return;
	const int32 Count = LobbyPlayerCount();
	// Preserve children where possible, leaving one toy when there are at least two players.
	const int32 Humans = Count < 2 ? Count : FMath::Clamp(State->Settings.Humans, 1, Count - 1);
	if (State->Settings.Humans == Humans && State->Settings.Toys == Count - Humans) return;
	State->Settings.Humans = Humans;
	State->Settings.Toys = Count - Humans;
	State->ForceNetUpdate();
}

// ホストからの設定変更を検証し、各プレイヤーのReadyは維持する。
void ATBGameMode::SetRules(APlayerController* PlayerController, const FTBSettings& Rules)
{
	auto* MatchInfo = TB::GS(GetWorld());
	if (!IsHost(PlayerController) || !MatchInfo || (MatchInfo->Phase != ETBPhase::Lobby || MatchInfo->bTravelPending))
	{
		return;
	}
	const int32 Count = LobbyPlayerCount();
	FTBSettings Adjusted = Rules;
	// A single changed field determines the other; explicit pairs must already match the lobby.
	if (Rules.Humans != MatchInfo->Settings.Humans && Rules.Toys == MatchInfo->Settings.Toys)
	{
		if (Rules.Humans < 0 || Rules.Humans > Count) return;
		Adjusted.Toys = Count - Rules.Humans;
	}
	else if (Rules.Toys != MatchInfo->Settings.Toys && Rules.Humans == MatchInfo->Settings.Humans)
	{
		if (Rules.Toys < 0 || Rules.Toys > Count) return;
		Adjusted.Humans = Count - Rules.Toys;
	}
	// 下限はロビーの最短より緩め、テスト用コマンドの短い試合も通す。
	if (!GameSession || Adjusted.Humans < (Count > 0 ? 1 : 0) || Adjusted.Toys < (Count > 1 ? 1 : 0) ||
	    int64(Adjusted.Humans) + Adjusted.Toys != Count || Count > GameSession->MaxPlayers || !FMath::IsFinite(Rules.Duration) ||
	    Rules.Duration < 10 || Rules.Duration > TB::Rules().Match.MaxDuration || Rules.RequiredItems < 1)
	{
		return;
	}
	if (Rules.RequiredItems > TBLobbySettings::AvailableItems(GetWorld()))
	{
		return;
	}
	CastChecked<ATBController>(PlayerController)->ClientPlaySoundEvent(ETBSoundEvent::UIChange, FVector::ZeroVector);
	MatchInfo->Settings = Adjusted;
	MatchInfo->ForceNetUpdate();
}

// 人数・Ready・マップ構成を確認してから陣営を割り当て、試合を開始する。
void ATBGameMode::StartRound(APlayerController* PlayerController)
{
	auto* MatchInfo = TB::GS(GetWorld());
	auto* HostController = Cast<ATBController>(PlayerController);
	if (!IsHost(PlayerController) || !MatchInfo || (MatchInfo->Phase != ETBPhase::Lobby || MatchInfo->bTravelPending))
	{
		return;
	}
	TArray<ATBPlayerState*> Players;
	for (APlayerState* ToyPlayerInfo : MatchInfo->PlayerArray)
	{
		if (auto* CandidateState = Cast<ATBPlayerState>(ToyPlayerInfo))
		{
			Players.Add(CandidateState);
		}
	}
	if (MatchInfo->Settings.Humans < 1 || MatchInfo->Settings.Toys < 1 ||
	    Players.Num() != MatchInfo->Settings.Humans + MatchInfo->Settings.Toys)
	{
		if (HostController)
		{
			HostController->ClientNotice(TEXT("At least one child and one toy are required to start."));
		}
		return;
	}
	for (auto* ToyPlayerInfo : Players)
	{
		if (!ToyPlayerInfo->bReady || !ToyPlayerInfo->GetPawn())
		{
			if (HostController)
			{
				HostController->ClientNotice(TEXT("Every player must press R to be ready."));
			}
			return;
		}
	}
	if (TBLobbySettings::ShouldTravelToStage(GetWorld()))
	{
		if (!TBLobbySettings::Travel(this, true) && HostController) HostController->ClientNotice(TEXT("試合ステージへ移動できませんでした。"));
		return;
	}
	int32 Items = 0, Boxes = 0;
	for (TActorIterator<ATBPickup> It(GetWorld()); It; ++It)
	{
		if (!It->bTaken)
		{
			++Items;
		}
	}
	for (TActorIterator<ATBBox> It(GetWorld()); It; ++It)
	{
		MatchInfo->Box = *It;
		++Boxes;
	}
	if (Boxes != 1 || Items < MatchInfo->Settings.RequiredItems)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToyBoxMatch] Start refused on %s: boxes %d, items %d/%d"),
		       *GetWorld()->GetMapName(), Boxes, Items, MatchInfo->Settings.RequiredItems);
		if (HostController)
		{
			HostController->ClientNotice(
			    FString::Printf(TEXT("Map requires exactly one box and enough items (boxes %d, items %d/%d)."), Boxes,
			                    Items, MatchInfo->Settings.RequiredItems));
		}
		return;
	}
	// 同じ希望内では抽選し、人間希望・希望なし・おもちゃ希望の順に人間枠を割り当てる。
	for (int32 Index = Players.Num() - 1; Index > 0; --Index)
	{
		Players.Swap(Index, FMath::RandRange(0, Index));
	}
	TArray<ATBPlayerState*> Ordered;
	for (ETBTeam Pref : {ETBTeam::Human, ETBTeam::None, ETBTeam::Toy})
	{
		for (auto* ToyPlayerInfo : Players)
		{
			if (ToyPlayerInfo->Preference == Pref)
			{
				Ordered.Add(ToyPlayerInfo);
			}
		}
	}
	for (int32 Index = 0; Index < Ordered.Num(); ++Index)
	{
		auto* ToyPlayerInfo = Ordered[Index];
		ToyPlayerInfo->Team = Index < MatchInfo->Settings.Humans ? ETBTeam::Human : ETBTeam::Toy;
		ToyPlayerInfo->ToyState = ETBToyState::Free;
		ToyPlayerInfo->bFrozen = false;
		ToyPlayerInfo->Items = 0;
		ToyPlayerInfo->OnRep_State();
		// チーム確定後、プレイ開始前に見た目を選ぶ。
		if (auto* Toy = Cast<ATBToyCharacter>(ToyPlayerInfo->GetPawn())) Toy->ChooseRandomToy();
		ToyPlayerInfo->ForceNetUpdate();
		if (auto* ToyCharacter = Cast<ATBCharacter>(ToyPlayerInfo->GetPawn()))
		{
			if (auto* Start = ChoosePlayerStart(ToyCharacter->GetController()))
			{
				ToyCharacter->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation());
			}
			ToyCharacter->ClearHeldInteraction();
		}
	}
	MatchInfo->Collected = 0;
	MatchInfo->BoxedCount = 0;
	MatchInfo->Winner = ETBWinner::None;
	MatchInfo->EndTime = MatchInfo->GetServerWorldTimeSeconds() + MatchInfo->Settings.Duration;
	MatchInfo->Phase = ETBPhase::Playing;
	MatchInfo->ForceNetUpdate();
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->RefreshLobbyAvailability();
	}
}

// 接続・PlayerState・マップを保持し、試合に属する状態だけを初期化する。
void ATBGameMode::ReturnToLobby(APlayerController* PlayerController)
{
	auto* State = TB::GS(GetWorld());
	if (!IsHost(PlayerController) || !State || (State->Phase != ETBPhase::Results && State->Phase != ETBPhase::Aborted))
	{
		return;
	}
	if (GetGameInstance()->GetSubsystem<UTBLobbySettings>()->bDedicatedLobby)
	{
		if (!TBLobbySettings::Travel(this, false)) return;
		for (APlayerState* Player : State->PlayerArray)
		{
			if (auto* Info = Cast<ATBPlayerState>(Player)) { Info->Team = ETBTeam::None; Info->bReady = false; }
		}
		return;
	}
	DestroyRoundCharacters();
	for (APlayerState* Player : State->PlayerArray)
	{
		if (auto* Info = Cast<ATBPlayerState>(Player))
		{
			Info->Team = ETBTeam::None;
			Info->ToyState = ETBToyState::Free;
			Info->bFrozen = false;
			Info->bReady = false;
			Info->Items = 0;
			Info->ForceNetUpdate();
		}
	}
	ResetStageInteractions();
	State->Collected = 0;
	State->BoxedCount = 0;
	State->EndTime = 0;
	State->Winner = ETBWinner::None;
	State->Reason.Reset();
	State->Phase = ETBPhase::Lobby;
	SyncLobbyCounts();
	State->ForceNetUpdate();
	SpawnCursor = 0;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (auto* Controller = Cast<ATBController>(It->Get()))
		{
			RestartPlayer(Controller);
			Controller->ClientNotice(TEXT("Same room. Choose your role and ready up for another round."), false);
		}
	}
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->RefreshLobbyAvailability();
	}
}

void ATBGameMode::DestroyRoundCharacters()
{
	for (TActorIterator<ATBDummyToy> It(GetWorld()); It; ++It)
	{
		It->ResetPlacement(true);
	}
	// 運搬の親子関係を解除してから全Pawnを再生成する。
	// 移動補間、入力、取得途中・収納途中のタイマーも新しいPawnで初期状態になる。
	TArray<ATBCharacter*> OldCharacters;
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		It->ClearHeldInteraction();
		It->Carrier = nullptr;
		It->CarriedToy = nullptr;
		It->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		OldCharacters.Add(*It);
	}
	for (auto* Character : OldCharacters)
	{
		if (auto* Controller = Character->GetController())
		{
			Controller->UnPossess();
		}
		Character->Destroy();
	}
}

void ATBGameMode::ResetStageInteractions()
{
	for (TActorIterator<ATBPickup> It(GetWorld()); It; ++It)
	{
		It->bTaken = false;
		It->OnRep_Taken();
		It->ForceNetUpdate();
	}
	for (TActorIterator<ATBBox> It(GetWorld()); It; ++It)
	{
		It->RescueProgress = 0;
		It->Rescuers = 0;
		It->ForceNetUpdate();
	}
}

// 頭・胴・足の3点について、視野の範囲と壁による遮蔽を確認する。
bool ATBGameMode::Watched(ATBCharacter* Toy, ATBCharacter* Human) const
{
	if (!Toy || !Human || !Human->GetController())
	{
		return false;
	}
	const FVector Eye = Human->GetPawnViewLocation();
	const FRotationMatrix Basis(Human->GetControlRotation());
	const FVector View = Basis.GetUnitAxis(EAxis::X);
	const FVector Right = Basis.GetUnitAxis(EAxis::Y);
	const FVector Up = Basis.GetUnitAxis(EAxis::Z);
	// 視野角と画面比率から、画面の端より少し内側を判定範囲にする。
	const auto& Vision = TB::Rules().Vision;
	// その人の画面に映っている範囲で判定する。画面比は本人のPCから届いた値（届く前は16:9）。
	const auto* HumanController = Cast<ATBController>(Human->GetController());
	const auto Half = TBRuleMath::JudgeHalfAngles(Vision.FieldOfView, Vision.AspectWidth, Vision.AspectHeight,
	                                              Vision.EdgeMargin, HumanController ? HumanController->ViewAspect : 0.f,
	                                              Vision.MaxViewAspect);
	const float TanH = FMath::Tan(FMath::DegreesToRadians(Half.Horizontal));
	const float TanV = FMath::Tan(FMath::DegreesToRadians(Half.Vertical));
	const float SampleHeight = Toy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * Vision.BodySampleRatio;
	for (float HeightOffset : {SampleHeight, 0.f, -SampleHeight})
	{
		const FVector SamplePosition = Toy->GetActorLocation() + FVector(0, 0, HeightOffset);
		const FVector Delta = SamplePosition - Eye;
		const float Depth = FVector::DotProduct(View, Delta);
		// 画面外のおもちゃを凍結させないよう、縦横の視野を判定する。
		if (Depth > 0 && FMath::Abs(FVector::DotProduct(Right, Delta)) <= Depth * TanH &&
		    FMath::Abs(FVector::DotProduct(Up, Delta)) <= Depth * TanV &&
		    TB::ClearPath(Human, Toy, Eye, SamplePosition))
		{
			return true;
		}
	}
	return false;
}

// 誰か1人に見られていれば凍結。全員の視線が外れた次の評価で解除する。
void ATBGameMode::EvaluateGaze()
{
	TArray<ATBCharacter*> Humans, Toys;
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		auto* ToyPlayerInfo = It->TBPS();
		if (!ToyPlayerInfo)
		{
			continue;
		}
		if (ToyPlayerInfo->Team == ETBTeam::Human)
		{
			Humans.Add(*It);
		}
		else if (ToyPlayerInfo->Team == ETBTeam::Toy &&
		         (ToyPlayerInfo->ToyState == ETBToyState::Free || ToyPlayerInfo->ToyState == ETBToyState::Boxed))
		{
			Toys.Add(*It);
		}
	}
	for (auto* Toy : Toys)
	{
		bool Seen = false;
		for (auto* Human : Humans)
		{
			if (Watched(Toy, Human))
			{
				Seen = true;
				break;
			}
		}
		Toy->SetFrozen(Seen);
	}
}

void ATBGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TBLobbySettings::TickArrival(this);
	if (!TB::Playing(GetWorld()))
	{
		return;
	}
	EvaluateGaze();
	CheckWin();
}

// この呼び出しでは時間切れ → アイテム達成 → 全員収納の順で判定する。
void ATBGameMode::CheckWin()
{
	auto* MatchInfo = TB::GS(GetWorld());
	if (!MatchInfo || MatchInfo->Phase != ETBPhase::Playing)
	{
		return;
	}
	int32 Toys = 0, Boxed = 0;
	for (APlayerState* ToyPlayerInfo : MatchInfo->PlayerArray)
	{
		if (auto* ToyPlayerState = Cast<ATBPlayerState>(ToyPlayerInfo))
		{
			if (ToyPlayerState->Team == ETBTeam::Toy)
			{
				++Toys;
				if (ToyPlayerState->ToyState == ETBToyState::Boxed)
				{
					++Boxed;
				}
			}
		}
	}
	MatchInfo->BoxedCount = Boxed;
	switch (TBRuleMath::EvaluateWin(MatchInfo->Remaining(), MatchInfo->Collected, MatchInfo->Settings.RequiredItems,
	                                Toys, Boxed))
	{
		case TBRuleMath::EWinReason::TimeExpired:
			Finish(ETBWinner::Humans, TEXT("Time expired"));
			break;
		case TBRuleMath::EWinReason::ItemsCollected:
			Finish(ETBWinner::Toys, TEXT("All required items collected"));
			break;
		case TBRuleMath::EWinReason::AllToysBoxed:
			Finish(ETBWinner::Humans, TEXT("All toys boxed"));
			break;
		default:
			break;
	}
}

void ATBGameMode::Finish(ETBWinner Winner, const FString& Why, bool Abort)
{
	auto* MatchInfo = TB::GS(GetWorld());
	if (!MatchInfo || MatchInfo->Phase != ETBPhase::Playing)
	{
		return;
	}
	MatchInfo->Winner = Winner;
	MatchInfo->Reason = Why;
	MatchInfo->Phase = Abort ? ETBPhase::Aborted : ETBPhase::Results;
	MatchInfo->ForceNetUpdate();
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		It->ClearHeldInteraction();
		It->ApplyState();
	}
}

// 退出者の状態を整理し、両陣営が残っていれば試合を続ける。
void ATBGameMode::Logout(AController* Exiting)
{
	if (!Exiting) return;
	if (GetWorld()->bIsTearingDown)
	{
		Super::Logout(Exiting);
		return;
	}
	auto* MatchInfo = TB::GS(GetWorld());
	const bool WasPlaying = MatchInfo && MatchInfo->Phase == ETBPhase::Playing;
	auto* Pending = GetGameInstance()->GetSubsystem<UTBLobbySettings>();
	if (Pending->bStartOnArrival && Exiting->PlayerState) Pending->bArrivalCancelled = true;
	if (auto* ToyCharacter = Cast<ATBCharacter>(Exiting->GetPawn()))
	{
		ToyCharacter->ClearHeldInteraction();
		if (IsValid(ToyCharacter->Carrier))
		{
			ToyCharacter->Carrier->DropToy();
		}
		ToyCharacter->DropToy();
	}
	// Super::LogoutではPlayerArrayからまだ削除されないため、勝敗判定前に除外する。
	if (MatchInfo && Exiting->PlayerState)
	{
		MatchInfo->RemovePlayerState(Exiting->PlayerState);
	}
	Super::Logout(Exiting);
	SyncLobbyCounts();
	if (!WasPlaying)
	{
		return;
	}
	int32 Humans = 0, Toys = 0;
	for (APlayerState* State : MatchInfo->PlayerArray)
	{
		if (const auto* Player = Cast<ATBPlayerState>(State))
		{
			Humans += Player->Team == ETBTeam::Human ? 1 : 0;
			Toys += Player->Team == ETBTeam::Toy ? 1 : 0;
		}
	}
	if (Humans == 0 || Toys == 0)
	{
		Finish(Humans == 0 ? ETBWinner::Toys : ETBWinner::Humans,
		       Humans == 0 ? TEXT("All humans disconnected") : TEXT("All toys disconnected"));
	}
	else
	{
		CheckWin();
	}
}
