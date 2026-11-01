// Copyright (c) 2026 Mamodou. All Rights Reserved.
#include "TBController.h"
#include "TBGameMode.h"
#include "TBGameState.h"
#include "TBPlayerState.h"
#include "TBSession.h"
#include "Character/TBCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr double RecoveryDelaySeconds = 3;
	constexpr double RecoveryCooldownSeconds = 30;
	constexpr double StuckSampleInterval = .25;
	constexpr double StuckInputSeconds = 3;
	constexpr double HintDurationSeconds = 10;
	constexpr float MovementTolerance = 20.f;
} // namespace

void ATBController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBController, bStuckHint);
}

void ATBController::ClientWasKicked_Implementation(const FText& KickReason)
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Kicked();
	}
}

void ATBController::ServerKickPlayer_Implementation(int32 PlayerId)
{
	auto* Mode = GetWorld()->GetAuthGameMode<ATBGameMode>();
	auto* State = GetWorld()->GetGameState<ATBGameState>();
	if (!Mode || !Mode->IsHost(this) || !Mode->GameSession || !State || State->bTravelPending)
	{
		return;
	}
	for (APlayerState* Info : State->PlayerArray)
	{
		if (!Info || Info->GetPlayerId() != PlayerId)
		{
			continue;
		}
		auto* Target = Cast<ATBController>(Info->GetOwner());
		if (!IsValid(Target) || Target == this || Target->IsLocalController() || !Target->GetNetConnection())
		{
			return;
		}
		const FString Name = Info->GetPlayerName();
		// KickPlayer destroys the pawn before Logout: release both sides of carry links first.
		if (auto* Body = Cast<ATBCharacter>(Target->GetPawn()))
		{
			Body->ClearHeldInteraction();
			if (IsValid(Body->Carrier))
			{
				Body->Carrier->DropToy();
			}
			Body->DropToy();
		}
		Target->bKickedByHost = true;
		if (Mode->GameSession->KickPlayer(Target, FText::FromString(TEXT("Removed by host"))))
		{
			ClientNotice(Name + TEXT(" を部屋から退出させました。"), false);
		}
		return;
	}
	ClientNotice(TEXT("そのプレイヤーはすでに退出しています。"));
}

bool ATBController::CanRecover() const
{
	const auto* State = GetWorld()->GetGameState<ATBGameState>();
	auto* Body = Cast<ATBCharacter>(GetPawn());
	const auto* Info = GetPlayerState<ATBPlayerState>();
	if (!State || State->bTravelPending || (State->Phase != ETBPhase::Lobby && State->Phase != ETBPhase::Playing) ||
	    !Body || !Info || Info->bFrozen || Info->ToyState != ETBToyState::Free || Body->Carrier ||
	    Body->HasCarriedToy() || Body->bHoldingInteract || Body->bRescuing || Body->ContactItem ||
	    Body->IsPhysicsLocked())
	{
		return false;
	}
	if (Info->Team == ETBTeam::Toy && State->Phase == ETBPhase::Playing)
	{
		auto* Mode = GetWorld()->GetAuthGameMode<ATBGameMode>();
		if (!Mode)
		{
			return false;
		}
		for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsHuman() && Mode->Watched(Body, *It))
			{
				return false;
			}
		}
	}
	return true;
}

bool ATBController::FindRecoveryLocation(FVector& Destination) const
{
	auto* Body = Cast<ATBCharacter>(GetPawn());
	if (!Body)
	{
		return false;
	}
	const auto* Capsule = Body->GetCapsuleComponent();
	const FVector Origin = Body->GetActorLocation();
	const float Radius = Capsule->GetScaledCapsuleRadius(), Half = Capsule->GetScaledCapsuleHalfHeight();
	const auto Shape = FCollisionShape::MakeCapsule(Radius, Half);
	const auto Channel = Capsule->GetCollisionObjectType();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TBRecovery), false, Body);
	const FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels());
	// Short capsule sweeps prevent passing through walls or other players. A penetrating
	// capsule may leave only along its penetration normal, by the minimum required distance.
	FHitResult Penetration;
	GetWorld()->SweepSingleByChannel(Penetration, Origin, Origin + FVector(0, 0, .1f), FQuat::Identity, Channel, Shape,
	                                 Query, Response);
	for (int32 Index = 0; Index < 33; ++Index)
	{
		FVector Candidate;
		if (Index == 0)
		{
			if (!Penetration.bStartPenetrating || Penetration.PenetrationDepth > 80)
			{
				continue;
			}
			Candidate = Origin + Penetration.Normal * (Penetration.PenetrationDepth + 3);
		}
		else
		{
			if (Penetration.bStartPenetrating)
			{
				continue;
			}
			const float Angle = (Index - 1) % 16 * UE_TWO_PI / 16;
			const float Distance = Index <= 16 ? 50.f : 100.f;
			Candidate = Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Distance;
			FHitResult Path;
			if (GetWorld()->SweepSingleByChannel(Path, Origin, Candidate, FQuat::Identity, Channel, Shape, Query,
			                                     Response))
			{
				continue;
			}
		}
		// No rescue across a floor, onto furniture, or over a ledge.
		FHitResult Floor;
		if (!GetWorld()->SweepSingleByChannel(Floor, Candidate, Candidate - FVector(0, 0, 40), FQuat::Identity, Channel,
		                                      Shape, Query, Response) ||
		    Floor.bStartPenetrating || !Body->GetCharacterMovement()->IsWalkable(Floor))
		{
			continue;
		}
		Candidate = Floor.Location + FVector(0, 0, 2.5f);
		if (FVector::Dist(Origin, Candidate) > 110 ||
		    GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, Channel, Shape, Query, Response))
		{
			continue;
		}
		Destination = Candidate;
		return true;
	}
	return false;
}

void ATBController::ServerUnstuck_Implementation()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (RecoveryEnds > 0)
	{
		return;
	}
	if (!CanRecover())
	{
		ClientNotice(TEXT("今の状態では解除できません。運搬・収納・停止・操作中は使えません。"));
		return;
	}
	if (Now < RecoveryCooldown)
	{
		ClientNotice(TEXT("スタック解除は30秒に一度使えます。"));
		return;
	}
	auto* Body = Cast<ATBCharacter>(GetPawn());
	const auto* Capsule = Body->GetCapsuleComponent();
	const bool Embedded = GetWorld()->OverlapBlockingTestByChannel(
	    Body->GetActorLocation(), FQuat::Identity, Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(),
	    FCollisionQueryParams(SCENE_QUERY_STAT(TBStuck), false, Body),
	    FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
	if (!bStuckHint && !Embedded)
	{
		ClientNotice(TEXT("移動を3秒ほど試してください。動けない状態を確認してから解除できます。"));
		return;
	}
	RecoveryPawn = Body;
	RecoveryWorld = GetWorld();
	RecoveryOrigin = Body->GetActorLocation();
	RecoveryPhase = GetWorld()->GetGameState<ATBGameState>()->Phase;
	RecoveryEnds = Now + RecoveryDelaySeconds;
	ClientNotice(TEXT("3秒後に近くの安全な場所へ移動します。動くとキャンセルします。"), false);
}

// Remote controllers have no PlayerInput, so PlayerTick does not run on the server.
void ATBController::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	if (IsValid(this))
	{
		TickRecovery();
	}
}

void ATBController::TickRecovery()
{
	if (!HasAuthority())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	auto* Body = Cast<ATBCharacter>(GetPawn());
	if (RecoveryPawn.Get() != Body || RecoveryWorld.Get() != GetWorld())
	{
		if (RecoveryWorld.Get() != GetWorld())
		{
			RecoveryCooldown = 0;
		}
		RecoveryPawn = Body;
		RecoveryWorld = GetWorld();
		RecoveryEnds = HintUntil = BlockedSeconds = 0;
		bStuckHint = false;
		RecoverySampleAt = Now;
		SampleOrigin = Body ? Body->GetActorLocation() : FVector::ZeroVector;
	}
	// 待機中以外の衝突・視線判定はサンプリング時だけ行う。
	if (RecoveryEnds <= 0 && Now - RecoverySampleAt < StuckSampleInterval)
	{
		return;
	}
	const bool Eligible = CanRecover();
	if (Body && Now - RecoverySampleAt >= StuckSampleInterval)
	{
		const auto* Move = Body->GetCharacterMovement();
		const FVector Direction = Move->GetCurrentAcceleration().GetSafeNormal2D();
		const auto* Capsule = Body->GetCapsuleComponent();
		FHitResult Hit;
		const bool Blocked = Eligible && !Direction.IsNearlyZero() &&
		                     FVector::DistSquared2D(SampleOrigin, Body->GetActorLocation()) < 4 &&
		                     GetWorld()->SweepSingleByChannel(
		                         Hit, Body->GetActorLocation(), Body->GetActorLocation() + Direction * 10,
		                         FQuat::Identity, Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(),
		                         FCollisionQueryParams(SCENE_QUERY_STAT(TBStuck), false, Body),
		                         FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
		BlockedSeconds = Blocked ? BlockedSeconds + FMath::Min(Now - RecoverySampleAt, 2 * StuckSampleInterval) : 0;
		if (BlockedSeconds >= StuckInputSeconds)
		{
			if (HintUntil <= Now)
			{
				StuckOrigin = Body->GetActorLocation();
			}
			HintUntil = Now + HintDurationSeconds;
		}
		if (HintUntil > Now &&
		    FVector::DistSquared(StuckOrigin, Body->GetActorLocation()) > FMath::Square(MovementTolerance))
		{
			HintUntil = BlockedSeconds = 0;
		}
		SampleOrigin = Body->GetActorLocation();
		RecoverySampleAt = Now;
	}
	if (!Eligible)
	{
		HintUntil = BlockedSeconds = 0;
	}
	bStuckHint = Eligible && Now < HintUntil;
	if (RecoveryEnds <= 0)
	{
		return;
	}
	if (!Eligible || GetWorld()->GetGameState<ATBGameState>()->Phase != RecoveryPhase ||
	    FVector::DistSquared(RecoveryOrigin, Body->GetActorLocation()) > FMath::Square(MovementTolerance))
	{
		RecoveryEnds = 0;
		ClientNotice(TEXT("状態が変わったため、スタック解除をキャンセルしました。"));
		return;
	}
	if (Now < RecoveryEnds)
	{
		return;
	}
	RecoveryEnds = 0;
	FVector Destination;
	if (!FindRecoveryLocation(Destination) || !Body->TeleportTo(Destination, Body->GetActorRotation(), false, true))
	{
		ClientNotice(TEXT("近くに安全な移動先が見つかりませんでした。"));
		return;
	}
	Body->GetCharacterMovement()->StopMovementImmediately();
	Body->ForceNetUpdate();
	RecoveryCooldown = Now + RecoveryCooldownSeconds;
	HintUntil = BlockedSeconds = 0;
	bStuckHint = false;
	ClientNotice(TEXT("近くの安全な場所へ移動しました。"), false);
}
