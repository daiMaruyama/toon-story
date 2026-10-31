// Copyright (c) 2026 Mamodou. All Rights Reserved.
#include "TBNPCController.h"
#include "GamePlay/Items/TBMirror.h"
#include "TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBPickup.h"
#include "GamePlay/TBDummyToy.h"
#include "GamePlay/TBRoomLighting.h"
#include "GamePlay/Items/TBInteractionTarget.h"
#include "Data/TBRuleData.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

namespace
{
// Distances are centimetres; durations are seconds. Keep tuning separate from decisions.
constexpr float ThinkIntervalMinSeconds = .25f;
constexpr float ThinkIntervalMaxSeconds = .4f;
constexpr float JumpProgressDistanceCm = 8.f;
constexpr double JumpCooldownSeconds = 1.5;
constexpr double ProgressCheckSeconds = .6;
constexpr float MoveGoalChangeCm = 60.f;
constexpr double MoveRequestIntervalSeconds = 1;

constexpr int32 PatrolCandidates = 6;
constexpr float PatrolRadiusCm = 1000.f;
constexpr float PatrolMinDistanceCm = 150.f;
constexpr int32 PatrolHistorySize = 8;
constexpr double PatrolTimeoutSeconds = 30;

constexpr float SightRangeCm = 1400.f;
constexpr float SightTargetHeightCm = 15.f;
constexpr float SightHalfAngleCos = .70710678f; // cos(45 degrees): a 90-degree cone.
constexpr float SightBodySampleRatio = .7f;
float SightHalfAngleTangent()
{
	return FMath::Tan(FMath::Acos(FMath::Clamp(SightHalfAngleCos,.01f,.999f)));
}
constexpr float ScanMinPitchDegrees = -30.f;
constexpr float ScanMaxPitchDegrees = 45.f;
constexpr float ScanRadiansPerSecond = .7f;
constexpr float HideDistanceBonusLimitCm = 1000.f;
constexpr float ThreatRangeCm = 1800.f;
constexpr float ThreatHalfAngleCos = .65f; // Wider cone for cautious cover selection.
constexpr float ExposureSampleSpacingCm = 150.f;

constexpr double BoxReplanSeconds = 1.5;
constexpr int32 BoxApproachGridSize = 3;
constexpr int32 BoxApproachCount = BoxApproachGridSize * BoxApproachGridSize;
constexpr float BoxDepthStartRatio = .5f;
constexpr float BoxDepthStepRatio = .15f;
constexpr float BoxWidthRatio = .65f;
constexpr float BoxFrontClearanceCm = 20.f;
const FVector BoxProjectionExtentCm(40, 40, 150);
constexpr float CloseGoalAcceptanceCm = 5.f;
const FVector DebugBoxFrontOffsetCm(-100, 0, 0);
const FVector DebugBoxProjectionExtentCm(100, 100, 300);

constexpr float HumanProgressDistanceCm = 15.f;
constexpr double HumanProgressCheckSeconds = 4;
constexpr double UnreachableToyCooldownSeconds = 8;
constexpr float OverlookMinSeconds = 4.f;
constexpr float OverlookMaxSeconds = 7.f;
constexpr double LostToyMemorySeconds = 3;
constexpr float ChaseAcceptanceCm = 15.f;

constexpr double BatteryMemorySeconds = 12;
constexpr float BatteryProgressDistanceCm = 10.f;
constexpr double BatteryProgressTimeoutSeconds = 6;
constexpr double BlockedBatteryCooldownSeconds = 15;
constexpr float RouteRecheckMinSeconds = 1.5f;
constexpr float RouteRecheckMaxSeconds = 2.f;
constexpr int32 BatteryPathBudget = 3;
constexpr float ExposedRoutePenaltyCm = 10000.f;
constexpr int32 CoverCandidates = 4;
constexpr float CoverRadiusCm = 650.f;
constexpr float CoverBatteryDistanceWeight = .25f;
constexpr double CoverHoldSeconds = 1.5;
}

CSV_DEFINE_CATEGORY(TBNPC, true);
static TAutoConsoleVariable<int32> CVarNPCHumanDebug(TEXT("tb.NPCHumanDebug"), 0, TEXT("Log NPC human progress at each progress check."));

ATBNPCController::ATBNPCController()
{
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
}

void ATBNPCController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) return;
	auto* Body = Cast<ATBCharacter>(GetPawn());
	if (!Body) return;
	Body->StopJumping();
	if (const auto* State = TB::GS(GetWorld()); State && State->IsPreparing())
	{
		Target.Reset(); PendingTarget.Reset(); BatteryTarget.Reset();
		ClearFocus(EAIFocusPriority::Gameplay);
		Body->ClearHeldInteraction();
		if (Body->IsHuman()) StopMovement();
		else if (State->HidePreparation.bValid && GetWorld()->GetTimeSeconds() >= NextThink)
		{
			NextThink = GetWorld()->GetTimeSeconds() + ThinkIntervalMaxSeconds;
			ThinkHide(Body);
		}
		return;
	}
	if (!Body->CanAct())
	{
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		Body->ClearHeldInteraction();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextThink) return;
	NextThink = Now + FMath::FRandRange(ThinkIntervalMinSeconds, ThinkIntervalMaxSeconds);
	TRACE_CPUPROFILER_EVENT_SCOPE(TBNPC_Think);
	CSV_SCOPED_TIMING_STAT(TBNPC, Think);
	// Navigation includes low obstacles that fit a normal jump. Recover without teleporting.
	if (Now >= CheckProgressAt)
	{
		if (GetMoveStatus() == EPathFollowingStatus::Moving &&
		    FVector::DistSquared2D(ProgressLocation, Body->GetActorLocation()) < FMath::Square(JumpProgressDistanceCm) &&
		    Body->GetCharacterMovement()->IsMovingOnGround() && Now >= NextJumpAt)
		{
			Body->Jump();
			NextJumpAt = Now + JumpCooldownSeconds;
		}
		ProgressLocation = Body->GetActorLocation();
		CheckProgressAt = Now + ProgressCheckSeconds;
	}
	if (Body->IsHuman()) ThinkHuman(Body);
	else ThinkToy(Body);
}

void ATBNPCController::WalkTo(const FVector& Destination, float Acceptance)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextWalk && FVector::DistSquared(WalkGoal, Destination) < FMath::Square(MoveGoalChangeCm)) return;
	NextWalk = Now + MoveRequestIntervalSeconds;
	WalkGoal = Destination;
	MoveToLocation(Destination, Acceptance, false, true, true, false, nullptr, false);
}

void ATBNPCController::Wander()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		if (Now < WanderUntil) return;
		StopMovement();
	}
	auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	const FVector Origin = GetPawn()->GetActorLocation();
	auto* Data = Nav ? Nav->GetNavDataForProps(GetNavAgentPropertiesRef(), Origin) : nullptr;
	if (!Data) return;
	FVector Best = Origin;
	float BestScore = 0;
	for (int32 Attempt = 0; Attempt < PatrolCandidates; ++Attempt)
	{
		FNavLocation Point;
		if (!Nav->GetRandomReachablePointInRadius(Origin, PatrolRadiusCm, Point, Data)) continue;
		float Score = FVector::Dist2D(Origin, Point.Location);
		if (Score < PatrolMinDistanceCm) continue;
		for (const FVector& Visited : VisitedPlaces) Score = FMath::Min(Score, float(FVector::Dist2D(Visited, Point.Location)));
		if (Score > BestScore) { BestScore = Score; Best = Point.Location; }
	}
	if (BestScore > 0)
	{
		VisitedPlaces.Add(Origin);
		if (VisitedPlaces.Num() > PatrolHistorySize) VisitedPlaces.RemoveAt(0);
		NextWalk = 0;
		WalkTo(Best);
		WanderUntil = Now + PatrolTimeoutSeconds;
	}
}

bool ATBNPCController::CanSee(const AActor* Actor) const
{
	FVector Aim;
	return FindVisiblePoint(Actor,GetControlRotation(),Aim);
}

bool ATBNPCController::FindVisiblePoint(const AActor* Actor, FRotator View, FVector& Aim) const
{
	const APawn* Body = GetPawn();
	if (!Body || !IsValid(Actor) || Actor == Body || Actor->IsHidden()) return false;
	const FVector Eye = Body->GetPawnViewLocation();
	const auto* Observer = Cast<ATBCharacter>(Body);
	const float Range = Observer && Observer->IsHuman() ? HumanSightRangeCm : SightRangeCm;
	FVector Origin, Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	if (Extent.IsNearlyZero()) Origin = Actor->GetActorLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCSight), true, Body);
	Query.AddIgnoredActor(Actor);
	for (float Height : {0.f, float(Extent.Z * SightBodySampleRatio), float(-Extent.Z * SightBodySampleRatio)})
	{
		const FVector Point = Origin + FVector(0, 0, Height);
		const FVector Delta = Point - Eye;
		if (Delta.SizeSquared() <= FMath::Square(Range) &&
		    FVector::DotProduct(View.Vector(), Delta.GetSafeNormal()) >= SightHalfAngleCos &&
		    !GetWorld()->LineTraceTestByChannel(Eye, Point, ECC_Visibility, Query)) { Aim=Point; return true; }
		FVector MirrorHit;
		const float Tan=SightHalfAngleTangent();
		if (Observer && Observer->IsHuman() && ATBMirror::FindView(Body,Actor,Eye,View,Point,Tan,Tan,MirrorHit,Range,SightHalfAngleCos))
		{ Aim=MirrorHit; return true; }
	}
	return false;
}

void ATBNPCController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	const FRotator PreviousView=GetControlRotation();
	Super::UpdateControlRotation(DeltaTime, bUpdatePawn);
	const auto* Body = Cast<ATBCharacter>(GetPawn());
	const auto* Focus = GetFocusActor();
	// AAIController normally flattens pitch for non-pawns, including floor-level decoys.
	if (Body && Body->IsHuman() && Focus && (Focus->IsA<ATBDummyToy>() || Focus->IsA<ATBCharacter>()))
	{
		FVector Aim=Focus->GetActorLocation();
		const FVector Eye=Body->GetPawnViewLocation();
		// 視認時と同じ体の3点・視野・遮蔽条件で、見えた反射位置に視線を合わせる。
		FindVisiblePoint(Focus,PreviousView,Aim);
		SetControlRotation((Aim-Eye).Rotation());
	}
	else if (Body && Body->IsHuman() && !Focus && !Body->HasCarriedToy() &&
	         !Target.IsValid() && !PendingTarget.IsValid() && Body->CanAct())
	{
		// Scan by time, never by an undiscovered actor's position.
		FRotator Rotation = GetControlRotation();
		Rotation.Pitch = FMath::Lerp(ScanMinPitchDegrees, ScanMaxPitchDegrees,
		    .5f + .5f * FMath::Sin(float(GetWorld()->GetTimeSeconds() * ScanRadiansPerSecond)));
		SetControlRotation(Rotation);
	}
}

void ATBNPCController::CarryToBox(ATBCharacter* Body, ATBBox* Box)
{
	Target.Reset();
	PendingTarget.Reset();
	ClearFocus(EAIFocusPriority::Gameplay);
	if (!Box) { StopMovement(); return; }
	if (Box->CanStoreFrom(Body))
	{
		StopMovement();
		SetFocalPoint(Box->GetActorLocation());
		if (Body->StoreStarted < 0) Body->RequestInteract();
		return;
	}
	Body->ClearHeldInteraction();
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextBoxPlan) return;
	NextBoxPlan = Now + BoxReplanSeconds;
	auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	const auto* Data = Nav ? Nav->GetNavDataForProps(GetNavAgentPropertiesRef(), Body->GetActorLocation()) : nullptr;
	if (!Data) return;
	// Try different positions across the box front instead of retrying one blocked point forever.
	const FVector Reach = TB::Rules().Box.StoreReach;
	for (int32 Attempt = 0; Attempt < BoxApproachCount; ++Attempt)
	{
		const int32 Index = (BoxApproach + Attempt) % BoxApproachCount;
		const FVector Local(-Reach.X * (BoxDepthStartRatio + BoxDepthStepRatio * (Index / BoxApproachGridSize)),
		                    Reach.Y * BoxWidthRatio * ((Index % BoxApproachGridSize) - BoxApproachGridSize / 2), 0);
		FNavLocation Goal;
		if (!Nav->ProjectPointToNavigation(Box->GetActorTransform().TransformPosition(Local), Goal, BoxProjectionExtentCm, Data)) continue;
		const FVector Projected = Box->GetActorTransform().InverseTransformPosition(Goal.Location);
		if (Projected.X >= -BoxFrontClearanceCm || Projected.X < -Reach.X || FMath::Abs(Projected.Y) > Reach.Y) continue;
		auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Body->GetActorLocation(), Goal.Location, this);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		BoxApproach = Index;
		WalkTo(Goal.Location, CloseGoalAcceptanceCm);
		return;
	}
	StopMovement();
}

void ATBNPCController::ThinkHuman(ATBCharacter* Body)
{
	auto* State = TB::GS(GetWorld());
	auto* Box = State ? State->Box.Get() : nullptr;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= HumanProgressAt)
	{
		if (CVarNPCHumanDebug.GetValueOnGameThread())
		{
			int32 Visible = 0;
			for (TActorIterator<ATBDummyToy> It(GetWorld()); It; ++It) Visible += !It->Holder && !It->StoredBox && CanSee(*It);
			for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It) Visible += !It->IsHuman() && CanSee(*It);
			UE_LOG(LogTemp, Display, TEXT("[NPCHuman] %s pos=%s move=%d velocity=%.1f look=%s target=%s pending=%s focus=%s carrying=%d storing=%.1f visible=%d interaction=%s"),
			 *GetName(), *Body->GetActorLocation().ToCompactString(), int32(GetMoveStatus()), Body->GetVelocity().Size(), *GetControlRotation().ToCompactString(),
			 *GetNameSafe(Target.Get()), *GetNameSafe(PendingTarget.Get()), *GetNameSafe(GetFocusActor()), Body->HasCarriedToy(), Body->StoreStarted, Visible,
			 *GetNameSafe(TB::FindInteractionTarget(Body).Actor));
			if (Box && Body->HasCarriedToy())
			{
				auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
				const auto& Props = GetNavAgentPropertiesRef();
				const auto* Data = Nav ? Nav->GetNavDataForProps(Props, Body->GetActorLocation()) : nullptr;
				const FVector Front = Box->GetActorTransform().TransformPosition(DebugBoxFrontOffsetCm);
				FNavLocation Projected;
				const bool OnNav = Data && Nav->ProjectPointToNavigation(Front, Projected, DebugBoxProjectionExtentCm, Data);
				auto* Path = OnNav ? UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Body->GetActorLocation(), Projected.Location, this) : nullptr;
				UE_LOG(LogTemp, Display, TEXT("[NPCCarry] data=%s agent=%.1f/%.1f box=%s projected=%d %s path=%d partial=%d points=%d"),
				 *GetNameSafe(Data), Props.AgentRadius, Props.AgentHeight, *Front.ToCompactString(), OnNav, *Projected.Location.ToCompactString(),
				 Path && Path->IsValid(), Path && Path->IsPartial(), Path ? Path->PathPoints.Num() : 0);
				if (Path) for (const FVector& Point : Path->PathPoints) UE_LOG(LogTemp, Display, TEXT("[NPCPathPoint] %s"), *Point.ToCompactString());

			}
		}

		if (HumanProgressAt > 0 && FVector::DistSquared2D(HumanProgressLocation, Body->GetActorLocation()) < FMath::Square(HumanProgressDistanceCm))
		{
			++BoxApproach;
			NextWalk = 0;
			NextBoxPlan = 0;
			if (!Body->HasCarriedToy() && Target.IsValid())
			{
				OverlookUntil.Add(Target, Now + UnreachableToyCooldownSeconds);
				Target.Reset();
				StopMovement();
			}
		}
		HumanProgressLocation = Body->GetActorLocation();
		HumanProgressAt = Now + HumanProgressCheckSeconds;
	}
	if (Body->HasCarriedToy())
	{
		CarryToBox(Body, Box);
		return;
	}
	NextBoxPlan = 0;
	Body->ClearHeldInteraction();
	for (auto It = OverlookUntil.CreateIterator(); It; ++It)
		if (!It.Key().IsValid() || It.Value() <= Now) It.RemoveCurrent();
	AActor* Seen = nullptr;
	AActor* Interactable = TB::FindInteractionTarget(Body).Actor;
	float Nearest = TNumericLimits<float>::Max();
	auto Consider = [&](AActor* Candidate)
	{
		if (OverlookUntil.Contains(TWeakObjectPtr<AActor>(Candidate))) return;
		const float Distance = FVector::DistSquared(Body->GetActorLocation(), Candidate->GetActorLocation());
		if ((Distance < Nearest || Candidate == Interactable) && CanSee(Candidate))
		{
			Seen = Candidate;
			Nearest = Candidate == Interactable ? -1.f : Distance;
		}
	};
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		auto* Info = It->TBPS();
		if (Info && Info->Team == ETBTeam::Toy && Info->ToyState == ETBToyState::Free) Consider(*It);
	}
	// Availability differs, but perception and priority never depend on whether a toy is real.
	for (TActorIterator<ATBDummyToy> It(GetWorld()); It; ++It)
		if (!It->Holder && !It->StoredBox) Consider(*It);
	// Keep a visible target through recognition; nearby moving toys must not restart the delay.
	auto* Stable = Target.IsValid() ? Target.Get() : PendingTarget.Get();
	if (Stable && (Seen != Interactable || Seen == Stable) && !OverlookUntil.Contains(TWeakObjectPtr<AActor>(Stable)) && CanSee(Stable))
	{
		if (auto* Toy = Cast<ATBCharacter>(Stable); Toy && Toy->TBPS() && Toy->TBPS()->ToyState == ETBToyState::Free)
			Seen = Stable;
		else if (auto* Dummy = Cast<ATBDummyToy>(Stable); Dummy && !Dummy->Holder && !Dummy->StoredBox)
			Seen = Stable;
	}
	if (Seen) Nearest = FVector::DistSquared(Body->GetActorLocation(), Seen->GetActorLocation());
	if (Seen && Target.Get() != Seen)
	{
		if (PendingTarget.Get() != Seen)
		{
			PendingTarget = Seen;
			RecognizeAt = Now + RecognitionSeconds;
			if (FMath::FRand() < OverlookChance)
			{
				OverlookUntil.Add(TWeakObjectPtr<AActor>(Seen), Now + FMath::FRandRange(OverlookMinSeconds, OverlookMaxSeconds));
				PendingTarget.Reset();
				Seen = nullptr;
			}
		}
		if (Seen && Now < RecognizeAt)
		{
			ClearFocus(EAIFocusPriority::Gameplay);
			return;
		}
	}
	if (Seen)
	{
		PendingTarget.Reset();
		Target = Seen;
		LastSeen = Seen->GetActorLocation();
		ForgetAt = Now + LostToyMemorySeconds;
		SetFocus(Seen);
		if (Nearest <= FMath::Square(TB::Rules().Aim.CaptureDistance) && TB::FindInteractionTarget(Body).Actor == Seen)
		{
			StopMovement();
			Body->RequestInteract();
		}
		else WalkTo(LastSeen, ChaseAcceptanceCm);
	}
	else
	{
		PendingTarget.Reset();
		ClearFocus(EAIFocusPriority::Gameplay);
		if (Target.IsValid() && Now < ForgetAt) WalkTo(LastSeen);
		else { Target.Reset(); Wander(); }
	}
}

bool ATBNPCController::Exposed(const FVector& Position) const
{
	for (const auto& Human : VisibleHumans)
	{
		auto* It = Human.Get();
		if (!It || ATBRoomLighting::ProtectsFromGaze(GetWorld(), Position, It->GetActorLocation())) continue;
		const FVector Eye = It->GetPawnViewLocation();
		const FVector Delta = Position + FVector(0, 0, SightTargetHeightCm) - Eye;
		if (Delta.SizeSquared() > FMath::Square(ThreatRangeCm) ||
		    FVector::DotProduct(It->GetControlRotation().Vector(), Delta.GetSafeNormal()) < ThreatHalfAngleCos) continue;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCCover), true, It);
		Query.AddIgnoredActor(GetPawn());
		if (!GetWorld()->LineTraceTestByChannel(Eye, Position + FVector(0, 0, SightTargetHeightCm), ECC_Visibility, Query))
			return true;
	}
	return false;
}

bool ATBNPCController::PathExposed(const TArray<FVector>& Points) const
{
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const int32 Samples = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Points[Index - 1], Points[Index]) / ExposureSampleSpacingCm));
		for (int32 Sample = 1; Sample <= Samples; ++Sample)
			if (Exposed(FMath::Lerp(Points[Index - 1], Points[Index], float(Sample) / Samples))) return true;
	}
	return false;
}

void ATBNPCController::ThinkHide(ATBCharacter* Body)
{
	auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	const FVector Origin = Body->GetActorLocation();
	auto* Data = Nav ? Nav->GetNavDataForProps(GetNavAgentPropertiesRef(), Origin) : nullptr;
	if (!Data) { StopMovement(); return; }
	VisibleHumans.Reset();
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
		if (It->IsHuman() && CanSee(*It)) VisibleHumans.Add(*It);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < CoverUntil && !Exposed(CoverGoal))
	{
		WalkTo(CoverGoal, CloseGoalAcceptanceCm);
		return;
	}
	FVector Best = Origin;
	float BestScore = -1.f;
	for (int32 Attempt = 0; Attempt < CoverCandidates; ++Attempt)
	{
		FNavLocation Point;
		if (!Nav->GetRandomReachablePointInRadius(Origin, CoverRadiusCm, Point, Data)) continue;
		auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Origin, Point.Location, this);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		const float Distance = FVector::Dist2D(Origin, Point.Location);
		if (Distance < PatrolMinDistanceCm) continue;
		// Cover at the endpoint wins. Crossing exposed ground is allowed during the head start.
		float Score = Distance + (Exposed(Point.Location) ? 0.f : ExposedRoutePenaltyCm);
		for (const auto& Human : VisibleHumans)
			if (Human.IsValid()) Score += FMath::Min(HideDistanceBonusLimitCm, float(FVector::Dist2D(Point.Location, Human->GetActorLocation())));
		if (Score > BestScore) { BestScore = Score; Best = Point.Location; }
	}
	if (BestScore < 0) { StopMovement(); return; }
	CoverGoal = Best;
	CoverUntil = Now + CoverHoldSeconds;
	WalkTo(Best, CloseGoalAcceptanceCm);
}

void ATBNPCController::ThinkToy(ATBCharacter* Body)
{
	ClearFocus(EAIFocusPriority::Gameplay);
	Body->ClearHeldInteraction();
	// Remain in contact until the normal collection timer completes.
	if (IsValid(Body->ContactItem) && Body->ContactItem->Touches(Body))
	{
		StopMovement();
		return;
	}
	auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!Nav) return;
	VisibleHumans.Reset();
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
		if (It->IsHuman() && CanSee(*It)) VisibleHumans.Add(*It);
	const FVector Origin = Body->GetActorLocation();
	auto* Data = Nav->GetNavDataForProps(GetNavAgentPropertiesRef(), Origin);
	if (!Data) return;
	const double Now = GetWorld()->GetTimeSeconds();
	const bool Danger = Exposed(Origin);
	if (GetWorld()->GetTimeSeconds() < CoverUntil && !Exposed(CoverGoal))
	{
		WalkTo(CoverGoal, CloseGoalAcceptanceCm);
		return;
	}
	// Remember a discovered stationary battery briefly while turning around furniture.
	if (BatteryTarget.IsValid() && CanSee(BatteryTarget.Get())) BatteryVisibleUntil = Now + BatteryMemorySeconds;
	if (BatteryTarget.IsValid() && Now >= BatteryVisibleUntil)
	{
		BatteryTarget.Reset();
		StopMovement();
	}
	// A valid nav path can still be blocked by toy-only collisions or other moving actors.
	// Give that item a cooldown instead of retrying the same unreachable target forever.
	if (BatteryTarget.IsValid() && !BatteryTarget->bTaken)
	{
		const float Distance = FVector::Dist2D(Origin, BatteryTarget->GetActorLocation());
		if (Distance < ClosestBatteryDistance - BatteryProgressDistanceCm)
		{
			ClosestBatteryDistance = Distance;
			BatteryProgressAt = Now + BatteryProgressTimeoutSeconds;
		}
		else if (Now >= BatteryProgressAt)
		{
			BlockedBattery = BatteryTarget;
			RetryBatteryAt = Now + BlockedBatteryCooldownSeconds;
			BatteryTarget.Reset();
			StopMovement();
		}
	}
	// Keep travelling to the same battery; periodically validate its route instead of switching goals.
	if (!Danger && BatteryTarget.IsValid() && !BatteryTarget->bTaken &&
	    GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		bool Safe = !Exposed(BatteryTarget->GetActorLocation());
		if (Safe && Now >= ReplanAt)
		{
			auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Origin, BatteryTarget->GetActorLocation(), this);
			Safe = Path && Path->IsValid() && !Path->IsPartial() && !PathExposed(Path->PathPoints);
			ReplanAt = Now + FMath::FRandRange(RouteRecheckMinSeconds, RouteRecheckMaxSeconds);
		}
		if (Safe)
		{
			WalkTo(BatteryTarget->GetActorLocation(), CloseGoalAcceptanceCm);
			return;
		}
	}
	const auto PreviousBattery = BatteryTarget;
	BatteryTarget.Reset();
	TArray<ATBPickup*> Batteries;
	for (TActorIterator<ATBPickup> It(GetWorld()); It; ++It)
		if (!It->bTaken && (BlockedBattery.Get() != *It || Now >= RetryBatteryAt) && (CanSee(*It) || (PreviousBattery.Get() == *It && Now < BatteryVisibleUntil))) Batteries.Add(*It);
	Batteries.Sort([Origin](const ATBPickup& A, const ATBPickup& B)
	{
		return FVector::DistSquared(Origin, A.GetActorLocation()) < FVector::DistSquared(Origin, B.GetActorLocation());
	});
	FVector Best = FVector::ZeroVector;
	float BestScore = TNumericLimits<float>::Max();
	bool Found = false;
	bool UnsafeRoute = false;
	// Bound synchronous work per decision; rotate candidates so unreachable nearby items cannot starve others.
	const int32 Budget = FMath::Min(BatteryPathBudget, Batteries.Num());
	for (int32 Index = 0; Index < Budget; ++Index)
	{
		auto* It = Batteries[(BatteryCursor + Index) % Batteries.Num()];
		auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Origin, It->GetActorLocation(), this);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		const bool Unsafe = PathExposed(Path->PathPoints);
		const float Score = Path->GetPathLength() + (Unsafe ? ExposedRoutePenaltyCm : 0.f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = It->GetActorLocation();
			UnsafeRoute = Unsafe;
			BatteryTarget = It;
			Found = true;
		}
	}
	if (BatteryTarget.IsValid() && BatteryTarget != PreviousBattery)
	{
		BatteryVisibleUntil = Now + BatteryMemorySeconds;
		ClosestBatteryDistance = FVector::Dist2D(Origin, BatteryTarget->GetActorLocation());
		BatteryProgressAt = Now + BatteryProgressTimeoutSeconds;
	}
	BatteryCursor = Batteries.Num() ? (BatteryCursor + Budget) % Batteries.Num() : 0;
	ReplanAt = Now + FMath::FRandRange(RouteRecheckMinSeconds, RouteRecheckMaxSeconds);
	// When threatened, prefer nearby cover over crossing an exposed route to a battery.
	if (Danger || UnsafeRoute)
	{
		BatteryTarget.Reset();
		const FVector BatteryGoal = Best;
		Found = false;
		BestScore = TNumericLimits<float>::Max();
		for (int32 Attempt = 0; Attempt < CoverCandidates; ++Attempt)
		{
			FNavLocation Cover;
			if (!Nav->GetRandomReachablePointInRadius(Origin, CoverRadiusCm, Cover, Data) || Exposed(Cover.Location)) continue;
			auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Origin, Cover.Location, this);
			if (!Path || !Path->IsValid() || Path->IsPartial() || PathExposed(Path->PathPoints)) continue;
			const float Score = Path->GetPathLength() + FVector::Dist(BatteryGoal, Cover.Location) * CoverBatteryDistanceWeight;
			if (Score < BestScore)
			{
				BestScore = Score;
				Best = Cover.Location;
				Found = true;
			}
		}
		if (Found)
		{
			CoverGoal = Best;
			CoverUntil = GetWorld()->GetTimeSeconds() + CoverHoldSeconds;
		}
	}
	if (Found) WalkTo(Best, CloseGoalAcceptanceCm);
	else if (Danger) StopMovement();
	else Wander();
}
