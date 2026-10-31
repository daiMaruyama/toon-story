// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBGameMode.h"
#include "GamePlay/Items/TBMirror.h"
#include "GamePlay/Events/TBEventDirector.h"
#include "GamePlay/Items/TBItemSpawnDirector.h"
#include "GamePlay/TBRoomLighting.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "TBLobbySettings.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Input/TBKeyLabels.h"
#include "Core/TBSession.h"
#include "Core/TBGameHelpers.h"
#include "Character/TBCharacter.h"
#include "Character/TBNPCController.h"
#include "Character/TBNPCNavigation.h"
#include "Character/TBToyCharacter.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBHumanEntrance.h"
#include "GamePlay/TBBoxEscapeCourse.h"
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
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Survives GameMode replacement during seamless travel; controllers must not accept the prior round's role.
	uint32 NextPreparationRound = 0;
}

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
	EventDirector = CreateDefaultSubobject<UTBEventDirector>(TEXT("EventDirector"));
	ItemSpawns = CreateDefaultSubobject<UTBItemSpawnDirector>(TEXT("ItemSpawns"));
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
		MatchInfo->StageName = Map.Contains(TEXT("StylizedHouse")) ? TEXT("Stylized House")
		                       : Map.Contains(TEXT("ToyWorldDemo")) ? TEXT("ToyWorld Demo")
		                       : Map.Contains(TEXT("ToyWorld"))   ? TEXT("紙のひみつクラブ")
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
	if (NewPlayer && NewPlayer->IsPlayerController()) PlaceInLobby(NewPlayer);
}

bool ATBGameMode::PlaceInLobby(AController* Player)
{
	const auto* State = TB::GS(GetWorld());
	auto* Pawn = Player ? Cast<ATBCharacter>(Player->GetPawn()) : nullptr;
	if (!State || State->Phase != ETBPhase::Lobby || !Pawn) return false;
	auto* World = GetWorld();
	const TWeakObjectPtr<ATBCharacter> OriginalPawn(Pawn);
	const TWeakObjectPtr<AController> OriginalController(Player);
	const FTransform OriginalTransform = Pawn->GetActorTransform();
	const FVector OriginalVelocity = Pawn->GetCharacterMovement()->Velocity;
	const EMovementMode OriginalMode = Pawn->GetCharacterMovement()->MovementMode;
	const uint8 OriginalCustomMode = Pawn->GetCharacterMovement()->CustomMovementMode;
	auto SameLobbyParticipant = [&]()
	{
		return IsValid(World) && !World->bIsTearingDown && IsValid(State) && TB::GS(World) == State &&
		    State->Phase == ETBPhase::Lobby && OriginalPawn.IsValid() && OriginalController.IsValid() &&
		    !Pawn->IsActorBeingDestroyed() && !Player->IsActorBeingDestroyed() &&
		    Player->GetPawn() == Pawn && Pawn->GetController() == Player;
	};
	auto RestoreOriginal = [&]()
	{
		if (!SameLobbyParticipant()) return false;
		const bool Restored = Pawn->GetActorTransform().Equals(OriginalTransform, .1f) ||
		    Pawn->TeleportTo(OriginalTransform.GetLocation(), OriginalTransform.Rotator());
		if (!SameLobbyParticipant()) return false;
		if (Restored && Pawn->GetActorTransform().Equals(OriginalTransform, .1f))
		{
			auto* Movement = Pawn->GetCharacterMovement();
			Movement->SetMovementMode(OriginalMode, OriginalCustomMode);
			if (!SameLobbyParticipant()) return false;
			Movement->Velocity = OriginalVelocity;
			if (Pawn->GetActorTransform().Equals(OriginalTransform, .1f)) return true;
		}
		// Collision-aware recovery can fail. Do not report a normal unchanged-position refusal in this case.
		FailRoundStart(TEXT("ロビー配置後の元の位置へ安全に戻せませんでした。"), false);
		return false;
	};
	Pawn->ApplyState();
	if (!SameLobbyParticipant()) return false;
	auto Occupied = [&](FVector Center, float Radius, float HalfHeight)
	{
		for (TActorIterator<ATBCharacter> Other(World); Other; ++Other)
		{
			if (*Other == Pawn || Other->IsActorBeingDestroyed() || !IsValid(Other->GetController()) ||
			    Other->GetController()->IsActorBeingDestroyed() || Other->GetController()->GetPawn() != *Other) continue;
			const auto* OtherCapsule = Other->GetCapsuleComponent();
			const float OtherRadius = OtherCapsule->GetScaledCapsuleRadius();
			const float OtherHalf = OtherCapsule->GetScaledCapsuleHalfHeight();
			const FVector Delta = Center - OtherCapsule->GetComponentLocation();
			const float AxisGap = FMath::Max(0.f, float(FMath::Abs(Delta.Z)) -
			    FMath::Max(0.f, HalfHeight - Radius) - FMath::Max(0.f, OtherHalf - OtherRadius));
			if (Delta.SizeSquared2D() + FMath::Square(AxisGap) <= FMath::Square(Radius + OtherRadius + 2.f)) return true;
		}
		return false;
	};
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
		const TWeakObjectPtr<ACameraActor> LobbyCamera(*It);
		auto ClearAt = [&](FVector Center)
		{
			if (!SameLobbyParticipant() || !LobbyCamera.IsValid() || !IsValid(Camera)) return false;
			const float ActualRadius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
			const float ActualHalf = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			if (Occupied(Center, ActualRadius, ActualHalf)) return false;
			const FVector View = Camera->GetComponentTransform().InverseTransformPosition(Center);
			const float TanHalfFOV = FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * .5f));
			if (View.X < 100 || FMath::Abs(View.Y) + ActualRadius > View.X * TanHalfFOV * .8f ||
			    FMath::Abs(View.Z) + ActualHalf > View.X * TanHalfFOV / Camera->AspectRatio * .8f) return false;
			FCollisionQueryParams VisibilityQuery = Query;
			VisibilityQuery.bTraceComplex = true;
			FHitResult Obstacle;
			return !World->LineTraceSingleByChannel(Obstacle, Camera->GetComponentLocation(), Center, ECC_Visibility, VisibilityQuery) &&
			    !World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn,
			        FCollisionShape::MakeCapsule(ActualRadius + 4, ActualHalf), Query);
		};
		// Search visible, walkable ground in front of the actual authored lobby camera.
		for (float Distance : {350.f, 500.f, 750.f, 1000.f, 1400.f, 1900.f})
		{
			// Keep the dedicated lobby group above the bottom ready panel.
			if (TBLobbySettings::IsLobby(GetWorld()) && Distance < 700.f) continue;
			for (float Side : {0.f, -80.f, 80.f, -160.f, 160.f, -280.f, 280.f, -420.f, 420.f})
			{
				if (!SameLobbyParticipant()) return false;
				const FVector Target = Eye + Forward * Distance + Right * Side;
				FHitResult Floor;
				if (!GetWorld()->SweepSingleByChannel(Floor, FVector(Target.X, Target.Y, Eye.Z),
				    FVector(Target.X, Target.Y, Eye.Z - 2000), FQuat::Identity, ECC_Pawn,
				    FCollisionShape::MakeCapsule(Radius + 4, HalfHeight), Query) ||
				    Floor.bStartPenetrating || Floor.ImpactNormal.Z < .7f) continue;
				const FVector Center = Floor.Location + FVector(0, 0, 3);
				if (!ClearAt(Center)) continue;
				const bool Teleported = Pawn->TeleportTo(Center, FRotator(0, (Eye - Center).Rotation().Yaw, 0));
				if (!SameLobbyParticipant()) return false;
				const FVector Arrived = Pawn->GetActorLocation();
				const auto* ActualCapsule = Pawn->GetCapsuleComponent();
				FHitResult ActualFloor;
				if (Teleported && Arrived.Equals(Center, 1.f) && ClearAt(Arrived) &&
				    World->SweepSingleByChannel(ActualFloor, Arrived + FVector(0, 0, 1), Arrived - FVector(0, 0, 8),
				        FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(ActualCapsule->GetScaledCapsuleRadius() + 4,
				            ActualCapsule->GetScaledCapsuleHalfHeight()), Query) &&
				    !ActualFloor.bStartPenetrating && ActualFloor.ImpactNormal.Z >= .7f)
				{
					Player->SetControlRotation(Pawn->GetActorRotation());
					if (!SameLobbyParticipant()) return false;
					if (auto* PC = Cast<APlayerController>(Player))
					{
						PC->ClientSetRotation(Pawn->GetActorRotation(), true);
					}
					if (!SameLobbyParticipant()) return false;
					Pawn->ForceNetUpdate();
					return true;
				}
				if (!RestoreOriginal()) return false;
			}
		}
	}
	RestoreOriginal(); // Normally unchanged; failed collision-aware restoration explicitly aborts instead.
	return false;
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
	if (bStartingRound && RoundEntrance.IsValid())
	{
		const auto* Info = Player ? Player->GetPlayerState<ATBPlayerState>() : nullptr;
		if (Info && Info->Team == ETBTeam::Human)
		{
			if (!RoundEntrance->DoorSlots.IsValidIndex(EntranceCursor)) return nullptr;
			return RoundEntrance->DoorSlots[EntranceCursor++].Get();
		}
		for (int32 Attempt = 0; Attempt < SpawnPoints.Num(); ++Attempt)
		{
			auto* Start = SpawnPoints[SpawnCursor++ % SpawnPoints.Num()].Get();
			bool DoorSlot = false;
			for (const auto& Slot : RoundEntrance->DoorSlots) DoorSlot |= Slot.Get() == Start;
			if (IsValid(Start) && !DoorSlot) return Start;
		}
		return nullptr;
	}
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
	// シームレストラベルでステージを移ったときはPostLoginを通らないので、部屋に入ったときだけ出る。
	TB::Chat::BroadcastNotice(GetWorld(), ETBChatNotice::Joined, TB::Chat::DisplayName(PlayerController->PlayerState));
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
			Count += Cast<ATBPlayerState>(Player) && !Player->IsABot() ? 1 : 0;
		}
	}
	return Count;
}

void ATBGameMode::SyncLobbyCounts()
{
	auto* State = TB::GS(GetWorld());
	if (!State || State->Phase != ETBPhase::Lobby || State->bTravelPending) return;
	const int32 Count = LobbyPlayerCount();
	// Real players take precedence if someone joins after NPC counts were selected.
	const int32 ParticipantLimit = GameSession ? GameSession->MaxPlayers : 0;
	State->Settings.NPCHumans = FMath::Clamp(State->Settings.NPCHumans, 0, FMath::Max(0, ParticipantLimit - Count));
	State->Settings.NPCToys = FMath::Clamp(State->Settings.NPCToys, 0, FMath::Max(0, ParticipantLimit - Count - State->Settings.NPCHumans));
	// Reserve a real player for each team that has no NPC, when both teams can be filled.
	const int32 MinimumHumans = Count > 0 && State->Settings.NPCHumans == 0 ? 1 : 0;
	const int32 MaximumHumans = Count > 0 && State->Settings.NPCToys == 0 && (Count > 1 || State->Settings.NPCHumans > 0) ? Count - 1 : Count;
	int32 HumanPreferences = 0, ToyPreferences = 0;
	for (const APlayerState* Player : State->PlayerArray)
	{
		const auto* Info = Cast<ATBPlayerState>(Player);
		if (!Info || Info->IsABot()) continue;
		HumanPreferences += Info->Preference == ETBTeam::Human ? 1 : 0;
		ToyPreferences += Info->Preference == ETBTeam::Toy ? 1 : 0;
	}
	// Only "either" players may fill missing teams. Explicit choices are never overridden.
	const int32 BalancedHumans = FMath::Clamp(State->Settings.Humans, MinimumHumans, MaximumHumans);
	const int32 Humans = FMath::Clamp(BalancedHumans, HumanPreferences, Count - ToyPreferences);
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
	const int32 ParticipantLimit = GameSession ? GameSession->MaxPlayers : 0;
	const int32 NPCLimit = FMath::Max(0, ParticipantLimit - 1); // Reserve a slot for the host.
	// 下限はロビーの最短より緩め、テスト用コマンドの短い試合も通す。
	if (!GameSession || Adjusted.Humans < 0 || Adjusted.Toys < 0 ||
	    Rules.NPCHumans < 0 || Rules.NPCHumans > NPCLimit || Rules.NPCToys < 0 || Rules.NPCToys > NPCLimit ||
	    int64(Count) + Rules.NPCHumans + Rules.NPCToys > ParticipantLimit ||
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
	SyncLobbyCounts();
	MatchInfo->ForceNetUpdate();
}

// 人数・Ready・マップ構成を確認してから陣営を割り当て、試合を開始する。
void ATBGameMode::StartRound(APlayerController* PlayerController)
{
	auto* MatchInfo = TB::GS(GetWorld());
	auto* HostController = Cast<ATBController>(PlayerController);
	if (bStartingRound || !IsHost(PlayerController) || !MatchInfo || (MatchInfo->Phase != ETBPhase::Lobby || MatchInfo->bTravelPending))
	{
		return;
	}
	TGuardValue<bool> StartingGuard(bStartingRound, true);
	SyncLobbyCounts();
	TArray<ATBPlayerState*> Players;
	for (APlayerState* ToyPlayerInfo : MatchInfo->PlayerArray)
	{
		if (auto* CandidateState = Cast<ATBPlayerState>(ToyPlayerInfo); CandidateState && !CandidateState->IsABot())
		{
			Players.Add(CandidateState);
		}
	}
	if (MatchInfo->Settings.Humans + MatchInfo->Settings.NPCHumans < 1 ||
	    MatchInfo->Settings.Toys + MatchInfo->Settings.NPCToys < 1 ||
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
				HostController->ClientNotice(TB::Notices::ReadyRequired);
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
	if ((MatchInfo->Settings.NPCHumans > 0 || MatchInfo->Settings.NPCToys > 0) &&
	    !TBNPCNavigation::Prepare(GetWorld()))
	{
		if (HostController) HostController->ClientNotice(TEXT("NPCの移動経路を準備できませんでした。"));
		return;
	}
	RoundEntrance = ATBHumanEntrance::Find(GetWorld());
	EntranceCursor = 0;
	FString SetupError;
	int32 Entrances = 0;
	for (TActorIterator<ATBHumanEntrance> It(GetWorld()); It; ++It) ++Entrances;
	if (Entrances > 1 || (TBLobbySettings::StageIndex(GetWorld()) == TBLobbySettings::HouseStageId && !RoundEntrance.IsValid()) ||
	    (RoundEntrance.IsValid() && !RoundEntrance->Validate(GameSession ? GameSession->MaxPlayers : 8, SetupError)))
	{
		FailRoundStart(SetupError.IsEmpty() ? TEXT("ステージの入口設定が不足または重複しています。") : SetupError, true);
		return;
	}
	int32 Courses = 0;
	for (TActorIterator<ATBBoxEscapeCourse> It(GetWorld()); It; ++It)
	{
		if (++Courses > 1) { FailRoundStart(TEXT("脱出コースを一つだけ登録してください。"), true); return; }
		if (It->Box != MatchInfo->Box || !It->Validate(SetupError))
		{ FailRoundStart(SetupError.IsEmpty() ? TEXT("脱出コースの箱参照が違います。") : SetupError, true); return; }
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
	struct FPlacement
	{
		TWeakObjectPtr<ATBPlayerState> Info;
		TWeakObjectPtr<AController> Controller;
		TWeakObjectPtr<ATBCharacter> Body;
		TWeakObjectPtr<AActor> Start;
		FTransform Original, Destination;
		ETBTeam Team;
		ETBTeam AssignedTeam;
		ETBToyState ToyState;
		bool Frozen;
		int32 Items;
		EMovementMode MovementMode;
		uint8 CustomMode;
		FVector Velocity;
		bool Moved = false;
	};
	TArray<FPlacement> Placements;
	const int32 OriginalCursor = SpawnCursor;
	const int32 OriginalEntranceCursor = EntranceCursor;
	auto ValidParticipant = [](const FPlacement& Entry)
	{
		return Entry.Info.IsValid() && Entry.Controller.IsValid() && Entry.Body.IsValid() &&
		       Entry.Info->GetPawn() == Entry.Body.Get() && Entry.Body->GetController() == Entry.Controller.Get() &&
		       Entry.Controller->GetPlayerState<ATBPlayerState>() == Entry.Info.Get();
	};
	if (Ordered.Num() != Players.Num() || TActorIterator<ATBNPCController>(GetWorld()))
	{
		FailRoundStart(TEXT("参加者またはNPCの開始状態を確認できませんでした。"), true);
		return;
	}
	for (auto* Info : Ordered)
	{
		auto* Body = IsValid(Info) ? Cast<ATBCharacter>(Info->GetPawn()) : nullptr;
		auto* Controller = Body ? Body->GetController() : nullptr;
		if (!IsValid(Body) || !IsValid(Controller) || Controller->GetPlayerState<ATBPlayerState>() != Info)
		{
			FailRoundStart(TEXT("参加者のキャラクターを確認できませんでした。"), true);
			return;
		}
		const auto* Movement = Body->GetCharacterMovement();
		FPlacement Entry;
		Entry.Info = Info; Entry.Controller = Controller; Entry.Body = Body;
		Entry.Original = Body->GetActorTransform();
		Entry.Team = Info->Team; Entry.ToyState = Info->ToyState; Entry.Frozen = Info->bFrozen; Entry.Items = Info->Items;
		Entry.AssignedTeam = Placements.Num() < MatchInfo->Settings.Humans ? ETBTeam::Human : ETBTeam::Toy;
		Entry.MovementMode = Movement->MovementMode; Entry.CustomMode = Movement->CustomMovementMode;
		Entry.Velocity = Movement->Velocity;
		Placements.Add(Entry);
	}
	auto Rollback = [&](const FString& Reason)
	{
		if (RoundEntrance.IsValid()) RoundEntrance->SetPreparing(false);
		bool Recovered = true;
		// Restore every role before restoring any collision shape or position.
		for (const auto& Entry : Placements)
		{
			if (!Entry.Info.IsValid()) { Recovered = false; continue; }
			Entry.Info->Team = Entry.Team; Entry.Info->ToyState = Entry.ToyState;
			Entry.Info->bFrozen = Entry.Frozen; Entry.Info->Items = Entry.Items;
		}
		for (const auto& Entry : Placements)
		{
			if (!ValidParticipant(Entry)) { Recovered = false; continue; }
			Entry.Body->ApplyState();
		}
		for (int32 Index = Placements.Num() - 1; Index >= 0; --Index)
		{
			const auto& Entry = Placements[Index];
			if (!ValidParticipant(Entry)) { Recovered = false; continue; }
			if (Entry.Moved)
			{
				bool Restored = false;
#if WITH_DEV_AUTOMATION_TESTS
				if (!bTestFailRollback)
#endif
				Restored = Entry.Body->TeleportTo(Entry.Original.GetLocation(), Entry.Original.Rotator());
				// Collision adjustment is acceptable only within one centimetre of the saved position.
				if (!Restored || !ValidParticipant(Entry) || !Entry.Body->GetActorLocation().Equals(Entry.Original.GetLocation(), 1.f) ||
				    !Entry.Body->GetActorRotation().Equals(Entry.Original.Rotator(), .1f)) Recovered = false;
			}
			if (!ValidParticipant(Entry)) { Recovered = false; continue; }
			auto* Movement = Entry.Body->GetCharacterMovement();
			Movement->SetMovementMode(Entry.MovementMode, Entry.CustomMode);
			if (!ValidParticipant(Entry)) { Recovered = false; continue; }
			Movement->Velocity = Entry.Velocity;
			Entry.Body->ForceNetUpdate();
		}
		SpawnCursor = OriginalCursor;
		EntranceCursor = OriginalEntranceCursor;
		FailRoundStart(Reason, Recovered);
	};
	PreparationViewAttempt = ++NextPreparationRound;
	if (PreparationViewAttempt == 0) PreparationViewAttempt = ++NextPreparationRound;
	if (RoundEntrance.IsValid()) RoundEntrance->SetPreparing(true);
	for (const auto& Entry : Placements)
		if (auto* PC = Cast<ATBController>(Entry.Controller.Get())) PC->ClientPreparationBegin(PreparationViewAttempt);
	for (int32 Index = 0; Index < Placements.Num(); ++Index)
	{
		const auto& Entry = Placements[Index];
		if (!ValidParticipant(Entry)) { Rollback(TEXT("開始準備中に参加者が変わりました。")); return; }
		auto* ToyPlayerInfo = Entry.Info.Get();
		ToyPlayerInfo->Team = Entry.AssignedTeam;
		ToyPlayerInfo->ToyState = ETBToyState::Free;
		ToyPlayerInfo->bFrozen = false;
		ToyPlayerInfo->Items = 0;
		Entry.Body->ApplyState();
	}
	// Choose once, after final roles/body sizes are applied, before moving anybody.
	for (auto& Entry : Placements)
	{
		if (!ValidParticipant(Entry)) { Rollback(TEXT("開始準備中に参加者が変わりました。")); return; }
		auto* Start = ChoosePlayerStart(Entry.Controller.Get());
		if (!IsValid(Start)) { Rollback(TEXT("開始位置がありません。ステージの配置を確認してください。")); return; }
		Entry.Start = Start;
		Entry.Destination = Start->GetActorTransform();
	}
	for (auto& Entry : Placements)
	{
		if (!ValidParticipant(Entry) || !Entry.Start.IsValid())
		{ Rollback(TEXT("開始配置中に参加者または開始位置が変わりました。")); return; }
		if (RoundEntrance.IsValid() && Entry.Info->Team == ETBTeam::Human)
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(TBFinalEntranceBody), false, Entry.Body.Get());
			for (const auto& Participant : Placements) if (Participant.Body.IsValid()) Query.AddIgnoredActor(Participant.Body.Get());
			const auto* Capsule = Entry.Body->GetCapsuleComponent();
			if (!TB::SupportsCapsule(GetWorld(), Entry.Destination.GetLocation(),
			    Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), ECC_Pawn, Query,
			    Entry.Body->GetCharacterMovement()->GetWalkableFloorZ()))
			{ Rollback(TEXT("最終体格で入口の安全な床・頭上空間を確認できませんでした。")); return; }
		}
		const bool Placed = Entry.Body->TeleportTo(Entry.Destination.GetLocation(), Entry.Destination.Rotator());
		Entry.Moved = Placed || (Entry.Body.IsValid() && !Entry.Body->GetActorTransform().Equals(Entry.Original));
		if (!Placed || !ValidParticipant(Entry))
		{ Rollback(TEXT("開始位置が塞がれています。ステージの配置を確認してください。")); return; }
		// TeleportTo can succeed at a collision-adjusted location. Validate the actual arrival.
		if (!Entry.Body->GetActorLocation().Equals(Entry.Destination.GetLocation(), 1.f))
		{ Rollback(TEXT("開始位置から外れて配置されました。ステージの配置を確認してください。")); return; }
		if (RoundEntrance.IsValid() && Entry.Info->Team == ETBTeam::Human)
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(TBArrivedEntranceBody), false, Entry.Body.Get());
			const auto* Capsule = Entry.Body->GetCapsuleComponent();
			if (!TB::SupportsCapsule(GetWorld(), Entry.Body->GetActorLocation(),
			    Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), ECC_Pawn, Query,
			    Entry.Body->GetCharacterMovement()->GetWalkableFloorZ()))
			{ Rollback(TEXT("入口への配置後に安全な床・空間を確認できませんでした。")); return; }
		}
	}
	if (LobbyPlayerCount() != Placements.Num())
	{ Rollback(TEXT("開始準備中に参加人数が変わりました。")); return; }
	// Spawn only after travel and player assignment; NPCs are not network arrivals or ready voters.
	if (!SpawnNPCs())
	{
		RemoveNPCs();
		Rollback(TEXT("NPCを配置できませんでした。"));
		return;
	}
	for (const auto& Entry : Placements)
	{
		if (!ValidParticipant(Entry))
		{ RemoveNPCs(); Rollback(TEXT("開始確定前に参加者が変わりました。")); return; }
	}
	for (const auto& Entry : Placements)
	{
		if (auto* Toy = Cast<ATBToyCharacter>(Entry.Body.Get())) Toy->ChooseRandomToy();
		if (!ValidParticipant(Entry))
		{ RemoveNPCs(); Rollback(TEXT("開始確定中に参加者が変わりました。")); return; }
		Entry.Body->ClearHeldInteraction();
		Entry.Info->OnRep_State();
		if (!ValidParticipant(Entry))
		{ RemoveNPCs(); Rollback(TEXT("開始確定中に参加者が変わりました。")); return; }
	}
	for (const auto& Entry : Placements)
	{
		if (!ValidParticipant(Entry))
		{ RemoveNPCs(); Rollback(TEXT("開始確定中に参加者が変わりました。")); return; }
		Entry.Info->ForceNetUpdate();
		Entry.Body->ForceNetUpdate();
	}
	if (LobbyPlayerCount() != Placements.Num())
	{ RemoveNPCs(); Rollback(TEXT("開始確定中に参加人数が変わりました。")); return; }
	TArray<FPlacement> NPCPlacements;
	for (TActorIterator<ATBNPCController> It(GetWorld()); It; ++It)
	{
		FPlacement Entry{};
		Entry.Controller = *It;
		Entry.Info = It->GetPlayerState<ATBPlayerState>();
		Entry.Body = Cast<ATBCharacter>(It->GetPawn());
		if (!ValidParticipant(Entry))
		{ RemoveNPCs(); Rollback(TEXT("開始確定前にNPCが変わりました。")); return; }
		Entry.AssignedTeam = Entry.Info->Team;
		NPCPlacements.Add(Entry);
	}
	MatchInfo->Collected = 0;
	MatchInfo->BoxedCount = 0;
	MatchInfo->Winner = ETBWinner::None;
	MatchInfo->Reason.Reset();
	MatchInfo->HidePreparation.bValid = true;
	MatchInfo->HidePreparation.bHiding = true;
	PreparationRound = PreparationViewAttempt;
	MatchInfo->HidePreparation.Round = PreparationRound;
	MatchInfo->HidePreparation.StartsAt = MatchInfo->GetServerWorldTimeSeconds();
	MatchInfo->HidePreparation.EndsAt = MatchInfo->HidePreparation.StartsAt + TB::Rules().Match.HidePreparationSeconds;
	MatchInfo->EndTime = 0;
	MatchInfo->Phase = ETBPhase::Playing;
	const uint32 CommittedRound = PreparationRound;
	const auto CommittedEntrance = RoundEntrance;
	auto CheckCommittedParticipants = [&]()
	{
		// A callback may already have ended this round. Never send its old rotation/role afterward.
		if (!IsValid(MatchInfo) || TB::GS(GetWorld()) != MatchInfo || MatchInfo->Phase != ETBPhase::Playing ||
		    PreparationRound != CommittedRound || !MatchInfo->HidePreparation.bValid ||
		    !MatchInfo->HidePreparation.bHiding || MatchInfo->HidePreparation.Round != CommittedRound) return false;
		bool Valid = LobbyPlayerCount() == Placements.Num() &&
		    MatchInfo->PlayerArray.Num() == Placements.Num() + NPCPlacements.Num() &&
		    (!CommittedEntrance.IsExplicitlyNull() ? CommittedEntrance.IsValid() && RoundEntrance == CommittedEntrance : !RoundEntrance.IsValid());
		for (const auto& Entry : Placements)
			Valid &= ValidParticipant(Entry) && Entry.Info->Team == Entry.AssignedTeam;
		for (const auto& Entry : NPCPlacements)
			Valid &= ValidParticipant(Entry) && Entry.Info->Team == Entry.AssignedTeam;
		if (!Valid) Finish(ETBWinner::None, TEXT("開始確定後に参加者が変わりました。ロビーへ戻ってください。"), true);
		return Valid;
	};
	for (const auto& Entry : NPCPlacements)
	{
		if (!CheckCommittedParticipants()) return;
		Entry.Body->ApplyState();
		if (!CheckCommittedParticipants()) return;
	}
	for (const auto& Entry : Placements)
	{
		if (!CheckCommittedParticipants()) return;
		Entry.Body->ApplyState();
		if (!CheckCommittedParticipants()) return;
	}
	// All placement and state callbacks succeeded before any entrance view is changed.
	for (const auto& Entry : Placements)
	{
		if (!CheckCommittedParticipants()) return;
		if (auto* PC = Cast<ATBController>(Entry.Controller.Get()))
		{
			if (CommittedEntrance.IsValid() && Entry.AssignedTeam == ETBTeam::Human)
			{
				const FRotator Facing(0, Entry.Destination.Rotator().Yaw, 0);
				PC->SetControlRotation(Facing);
				if (!CheckCommittedParticipants()) return;
				PC->ClientSetRotation(Facing, true);
				if (!CheckCommittedParticipants()) return;
			}
			PC->ClientPreparationRole(CommittedRound, Entry.AssignedTeam);
			if (!CheckCommittedParticipants()) return;
		}
	}
	ItemSpawns->StartRound();
	MatchInfo->ForceNetUpdate();
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->RefreshLobbyAvailability();
	}
}

void ATBGameMode::FailRoundStart(const FString& Reason, bool bRecovered)
{
	auto* State = TB::GS(GetWorld());
	if (!State) return;
	State->EndTime = 0;
	if (RoundEntrance.IsValid()) RoundEntrance->SetPreparing(false);
	State->HidePreparation = FTBHidePreparation();
	State->Reason = bRecovered ? Reason + TEXT(" 準備をやり直してください。")
	                          : Reason + TEXT(" 元の位置へ安全に戻せませんでした。ロビーへ戻ってください。");
	State->Phase = bRecovered ? ETBPhase::Lobby : ETBPhase::Aborted;
	if (!bRecovered) State->Winner = ETBWinner::None;
	for (APlayerState* Player : State->PlayerArray)
	{
		if (auto* Info = Cast<ATBPlayerState>(Player); Info && !Info->IsABot())
		{
			Info->bReady = false;
			Info->ForceNetUpdate();
		}
	}
	State->ForceNetUpdate();
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (auto* PC = Cast<ATBController>(It->Get()))
		{
			PC->ClientPreparationCancelled(PreparationViewAttempt);
			PC->ClientNotice(State->Reason);
		}
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
		Session->RefreshLobbyAvailability();
}

// 接続・PlayerState・マップを保持し、試合に属する状態だけを初期化する。
void ATBGameMode::ReturnToLobby(APlayerController* PlayerController)
{
	auto* State = TB::GS(GetWorld());
	if (!IsHost(PlayerController) || !State || (State->Phase != ETBPhase::Results && State->Phase != ETBPhase::Aborted))
	{
		return;
	}
	State->HidePreparation = FTBHidePreparation();
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (auto* PC = Cast<ATBController>(It->Get())) PC->ClientPreparationCancelled(PreparationRound);
	RemoveNPCs();
	if (RoundEntrance.IsValid()) RoundEntrance->SetPreparing(false);
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
	ItemSpawns->ResetRound();
	for (TActorIterator<ATBRoomLighting> It(GetWorld()); It; ++It)
	{
		It->ResetLighting();
	}
	for (TActorIterator<ATBPickup> It(GetWorld()); It; ++It)
	{
		It->bTaken = false;
		It->OnRep_Taken();
		It->ForceNetUpdate();
	}
	for (TActorIterator<ATBBox> It(GetWorld()); It; ++It)
	{
		It->ResetCaptivity();
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
	// An NPC that has not noticed this toy must not freeze it just by facing it.
	if (const auto* NPC = Cast<ATBNPCController>(Human->GetController()); NPC && !NPC->IsTracking(Toy)) return false;
	const FVector Eye = Human->GetPawnViewLocation();
	const FRotationMatrix Basis(Human->GetControlRotation());
	const FVector View = Basis.GetUnitAxis(EAxis::X);
	const FVector Right = Basis.GetUnitAxis(EAxis::Y);
	const FVector Up = Basis.GetUnitAxis(EAxis::Z);
	// 視野角と画面比率から、画面の端より少し内側を判定範囲にする。
	const auto& Vision = TB::Rules().Vision;
	// 画面が基準より狭い人は、映っている範囲まで判定を狭める。画面比は本人のPCから届いた値（届く前は基準の画面比）で、
	// 嘘で得をできないよう基準より広げる方向には使わない。
	const auto* HumanController = Cast<ATBController>(Human->GetController());
	const auto Half = TBRuleMath::JudgeHalfAngles(Vision.FieldOfView, Vision.AspectWidth, Vision.AspectHeight,
	                                              Vision.EdgeMargin, HumanController ? HumanController->ViewAspect : 0.f);
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
		FVector MirrorHit;
		if (ATBMirror::FindView(Human,Toy,Eye,Human->GetControlRotation(),SamplePosition,TanH,TanV,MirrorHit)) return true;
	}
	return false;
}

// 誰か1人に見られていれば凍結。全員の視線が外れた次の評価で解除する。
void ATBGameMode::EvaluateGaze()
{
	if (const auto* State = TB::GS(GetWorld()); State && State->IsPreparing()) return;
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
		if (Toy->TBPS()->ToyState == ETBToyState::Boxed)
		{
			const auto* State = TB::GS(GetWorld());
			const auto* Course = State ? ATBBoxEscapeCourse::Find(GetWorld(), State->Box) : nullptr;
			if (Course && Course->Protects(Toy)) { Toy->SetFrozen(false); continue; }
		}
		bool Seen = false;
		for (auto* Human : Humans)
		{
			if (Toy->CanBeFrozenBy(Human) && Watched(Toy, Human))
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
	auto* Pending = GetGameInstance()->GetSubsystem<UTBLobbySettings>();
	if (const auto* State = TB::GS(GetWorld()); State && State->bTravelPending && Pending->bStartOnArrival)
		bObservedPreparationArrival = true;
	TBLobbySettings::TickArrival(this);
	if (const auto* State = TB::GS(GetWorld()); State && bObservedPreparationArrival)
	{
		if (State->Phase == ETBPhase::Playing) bObservedPreparationArrival = false;
		else if (State->Phase == ETBPhase::Lobby && !State->bTravelPending &&
		         !Pending->bStartOnArrival && Pending->PendingMap.IsEmpty())
		{
			bObservedPreparationArrival = false;
			for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
				if (auto* PC = Cast<ATBController>(It->Get())) PC->ClientPreparationTravelCancelled();
		}
	}
	if (!TB::Playing(GetWorld()))
	{
		EventDirector->ResetRound();
		return;
	}
	if (AdvanceHidePreparation()) return;
	EvaluateGaze();
	CheckWin();
	EventDirector->Advance(DeltaSeconds);
}

// Returns true while preparation (or its cancellation) consumes this server tick.
#if WITH_DEV_AUTOMATION_TESTS
void ATBGameMode::AdvancePreparationClockForTest(double SecondsRemaining)
{
	if (auto* State = TB::GS(GetWorld()); State && State->IsPreparing())
	{
		// Advance the test clock boundary, never bypass the authority's release/validation path.
		State->HidePreparation.EndsAt = State->GetServerWorldTimeSeconds() + SecondsRemaining;
		AdvanceHidePreparation();
	}
}
#endif

bool ATBGameMode::AdvanceHidePreparation()
{
	auto* State = TB::GS(GetWorld());
	if (!State || !State->IsPreparing()) return false;
	bool Human = false, Toy = false;
	for (APlayerState* Player : State->PlayerArray)
	{
		auto* Info = Cast<ATBPlayerState>(Player);
		auto* Body = Info ? Cast<ATBCharacter>(Info->GetPawn()) : nullptr;
		if (!IsValid(Body) || !IsValid(Body->GetController())) continue;
		Human |= Info->Team == ETBTeam::Human;
		Toy |= Info->Team == ETBTeam::Toy;
	}
	if (!Human || !Toy || !State->HidePreparation.bValid || State->HidePreparation.Round != PreparationRound)
	{
		Finish(ETBWinner::None, TEXT("隠れる準備に必要な参加者を確認できませんでした。"), true);
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			if (auto* PC = Cast<ATBController>(It->Get())) PC->ClientNotice(State->Reason);
		return true;
	}
	if (State->GetServerWorldTimeSeconds() < State->HidePreparation.EndsAt) return true;
	State->HidePreparation.bHiding = false;
	if (RoundEntrance.IsValid()) RoundEntrance->SetPreparing(false);
	State->EndTime = State->GetServerWorldTimeSeconds() + State->Settings.Duration;
	State->ForceNetUpdate();
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It) It->ApplyState();
	return true;
}

// この呼び出しでは時間切れ → アイテム達成 → 全員収納の順で判定する。
void ATBGameMode::CheckWin()
{
	auto* MatchInfo = TB::GS(GetWorld());
	if (!MatchInfo || MatchInfo->Phase != ETBPhase::Playing || MatchInfo->IsPreparing())
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
	if (RoundEntrance.IsValid()) RoundEntrance->SetPreparing(false);
	MatchInfo->HidePreparation = FTBHidePreparation();
	MatchInfo->Reason = Why;
	MatchInfo->Phase = Abort ? ETBPhase::Aborted : ETBPhase::Results;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (auto* PC = Cast<ATBController>(It->Get())) PC->ClientPreparationCancelled(PreparationRound);
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
	const auto* Leaving = Cast<ATBController>(Exiting);
	const ETBChatNotice LeaveKind = Leaving && Leaving->bKickedByHost ? ETBChatNotice::Kicked : ETBChatNotice::Left;
	const FString LeaverName = TB::Chat::DisplayName(Exiting->PlayerState);
	TB::Chat::BroadcastNotice(GetWorld(), LeaveKind, LeaverName, Exiting);
	auto* MatchInfo = TB::GS(GetWorld());
	const bool WasPlaying = MatchInfo && MatchInfo->Phase == ETBPhase::Playing;
	// 試合中はチャットを出さないので、お知らせの枠にも出す。音はロビーと同じ抜けたときの音（ロビーでは人数の変化で鳴る）。
	if (WasPlaying && !LeaverName.IsEmpty())
	{
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (auto* PC = Cast<ATBController>(It->Get()); PC && PC != Leaving)
			{
				PC->ClientNotice(TB::Chat::NoticeText(LeaveKind, LeaverName), false);
				PC->ClientPlaySoundEvent(ETBSoundEvent::LobbyLeave, FVector::ZeroVector);
			}
		}
	}
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
		if (MatchInfo->IsPreparing())
		{
			Finish(ETBWinner::None, TEXT("隠れる準備中に陣営の参加者がいなくなりました。"), true);
			for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
				if (auto* PC = Cast<ATBController>(It->Get())) PC->ClientNotice(MatchInfo->Reason);
			return;
		}
		Finish(Humans == 0 ? ETBWinner::Toys : ETBWinner::Humans,
		       Humans == 0 ? TEXT("All humans disconnected") : TEXT("All toys disconnected"));
	}
	else
	{
		CheckWin();
	}
}

bool ATBGameMode::SpawnNPCs()
{
	const auto* State = TB::GS(GetWorld());
	if (!State) return false;
#if WITH_DEV_AUTOMATION_TESTS
	TestSuccessfulNPCs = 0;
	bTestNPCFailureReached = false;
#endif
	for (ETBTeam Team : {ETBTeam::Human, ETBTeam::Toy})
	{
		const int32 Count = Team == ETBTeam::Human ? State->Settings.NPCHumans : State->Settings.NPCToys;
		for (int32 Index = 0; Index < Count; ++Index)
		{
#if WITH_DEV_AUTOMATION_TESTS
			if (TestFailNPCAfter != INDEX_NONE && TestSuccessfulNPCs >= TestFailNPCAfter)
			{
				bTestNPCFailureReached = true;
				return false;
			}
#endif
			auto* AI = GetWorld()->SpawnActor<ATBNPCController>();
			if (!AI) return false;
			auto* Info = AI->GetPlayerState<ATBPlayerState>();
			if (!Info) { AI->Destroy(); return false; }
			Info->SetIsABot(true);
			Info->SetPlayerName(FString::Printf(TEXT("NPC %s %d"), Team == ETBTeam::Human ? TEXT("Human") : TEXT("Toy"), Index + 1));
			Info->Team = Team;
			Info->Preference = Team;
			Info->bReady = true;
			// RestartPlayer also provides collision-aware spawning from the normal player starts.
			RestartPlayer(AI);
			auto* Body = Cast<ATBCharacter>(AI->GetPawn());
			if (!Body) return false;
			Body->ApplyState();
			if (Team == ETBTeam::Human && RoundEntrance.IsValid())
			{
				FCollisionQueryParams Query(SCENE_QUERY_STAT(TBNPCEntranceFloor), false, Body);
				const auto* Capsule = Body->GetCapsuleComponent();
				if (!TB::SupportsCapsule(GetWorld(), Body->GetActorLocation(),
				    Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), ECC_Pawn, Query,
				    Body->GetCharacterMovement()->GetWalkableFloorZ())) return false;
			}
			Body->UpdateNavAgent();
			if (auto* Toy = Cast<ATBToyCharacter>(Body)) Toy->ChooseRandomToy();
			Info->ForceNetUpdate();
#if WITH_DEV_AUTOMATION_TESTS
			if (IsValid(AI) && IsValid(Body) && AI->GetPawn() == Body && Body->GetController() == AI) ++TestSuccessfulNPCs;
#endif
		}
	}
	return true;
}

void ATBGameMode::RemoveNPCs()
{
	TArray<ATBNPCController*> Bots;
	for (TActorIterator<ATBNPCController> It(GetWorld()); It; ++It) Bots.Add(*It);
	for (auto* AI : Bots)
	{
		if (auto* Body = Cast<ATBCharacter>(AI->GetPawn()))
		{
			if (IsValid(Body->Carrier)) Body->Carrier->DropToy();
			Body->DropToy();
			AI->UnPossess();
			Body->Destroy();
		}
		AI->Destroy();
	}
}
