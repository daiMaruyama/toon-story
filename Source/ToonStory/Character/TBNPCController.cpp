// Copyright (c) 2026 Mamodou. All Rights Reserved.
#include "TBNPCController.h"
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

CSV_DEFINE_CATEGORY(TBNPC, true);
static TAutoConsoleVariable<int32> CVarNPCHumanDebug(TEXT("tb.NPCHumanDebug"), 0, TEXT("Log NPC human progress every four seconds."));

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
	if (!Body->CanAct())
	{
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		Body->ClearHeldInteraction();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextThink) return;
	NextThink = Now + FMath::FRandRange(.25f, .4f);
	TRACE_CPUPROFILER_EVENT_SCOPE(TBNPC_Think);
	CSV_SCOPED_TIMING_STAT(TBNPC, Think);
	// Navigation includes low obstacles that fit a normal jump. Recover without teleporting.
	if (Now >= CheckProgressAt)
	{
		if (GetMoveStatus() == EPathFollowingStatus::Moving &&
		    FVector::DistSquared2D(ProgressLocation, Body->GetActorLocation()) < FMath::Square(8.f) &&
		    Body->GetCharacterMovement()->IsMovingOnGround() && Now >= NextJumpAt)
		{
			Body->Jump();
			NextJumpAt = Now + 1.5;
		}
		ProgressLocation = Body->GetActorLocation();
		CheckProgressAt = Now + .6;
	}
	if (Body->IsHuman()) ThinkHuman(Body);
	else ThinkToy(Body);
}

void ATBNPCController::WalkTo(const FVector& Destination, float Acceptance)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextWalk && FVector::DistSquared(WalkGoal, Destination) < FMath::Square(60.f)) return;
	NextWalk = Now + 1;
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
	for (int32 Attempt = 0; Attempt < 6; ++Attempt)
	{
		FNavLocation Point;
		if (!Nav->GetRandomReachablePointInRadius(Origin, 1000.f, Point, Data)) continue;
		float Score = FVector::Dist2D(Origin, Point.Location);
		if (Score < 150.f) continue;
		for (const FVector& Visited : VisitedPlaces) Score = FMath::Min(Score, float(FVector::Dist2D(Visited, Point.Location)));
		if (Score > BestScore) { BestScore = Score; Best = Point.Location; }
	}
	if (BestScore > 0)
	{
		VisitedPlaces.Add(Origin);
		if (VisitedPlaces.Num() > 8) VisitedPlaces.RemoveAt(0);
		NextWalk = 0;
		WalkTo(Best);
		WanderUntil = Now + 30;
	}
}

bool ATBNPCController::CanSee(const AActor* Actor) const
{
	const APawn* Body = GetPawn();
	if (!Body || !IsValid(Actor) || Actor == Body || Actor->IsHidden()) return false;
	const FVector Eye = Body->GetPawnViewLocation();
	const FVector Point = Actor->GetActorLocation() + FVector(0, 0, 15);
	const FVector Delta = Point - Eye;
	// Both teams use the same 90-degree cone and 14-metre sight range.
	if (Delta.SizeSquared() > FMath::Square(1400.f) ||
	    FVector::DotProduct(GetControlRotation().Vector(), Delta.GetSafeNormal()) < .70710678f) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCSight), true, Body);
	Query.AddIgnoredActor(Actor);
	return !GetWorld()->LineTraceTestByChannel(Eye, Point, ECC_Visibility, Query);
}

void ATBNPCController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	Super::UpdateControlRotation(DeltaTime, bUpdatePawn);
	const auto* Body = Cast<ATBCharacter>(GetPawn());
	const auto* Focus = GetFocusActor();
	// AAIController normally flattens pitch for non-pawns, including floor-level decoys.
	if (Body && Body->IsHuman() && Focus && (Focus->IsA<ATBDummyToy>() || Focus->IsA<ATBCharacter>()))
		SetControlRotation((Focus->GetActorLocation() - Body->GetPawnViewLocation()).Rotation());
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
	NextBoxPlan = Now + 1.5;
	auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	const auto* Data = Nav ? Nav->GetNavDataForProps(GetNavAgentPropertiesRef(), Body->GetActorLocation()) : nullptr;
	if (!Data) return;
	// Try different positions across the box front instead of retrying one blocked point forever.
	const FVector Reach = TB::Rules().Box.StoreReach;
	for (int32 Attempt = 0; Attempt < 9; ++Attempt)
	{
		const int32 Index = (BoxApproach + Attempt) % 9;
		const FVector Local(-Reach.X * (.5f + .15f * (Index / 3)),
		                    Reach.Y * .65f * ((Index % 3) - 1), 0);
		FNavLocation Goal;
		if (!Nav->ProjectPointToNavigation(Box->GetActorTransform().TransformPosition(Local), Goal, FVector(40,40,150), Data)) continue;
		const FVector Projected = Box->GetActorTransform().InverseTransformPosition(Goal.Location);
		if (Projected.X >= -20 || Projected.X < -Reach.X || FMath::Abs(Projected.Y) > Reach.Y) continue;
		auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Body->GetActorLocation(), Goal.Location, this);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		BoxApproach = Index;
		WalkTo(Goal.Location, 5.f);
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
				const FVector Front = Box->GetActorTransform().TransformPosition(FVector(-100, 0, 0));
				FNavLocation Projected;
				const bool OnNav = Data && Nav->ProjectPointToNavigation(Front, Projected, FVector(100,100,300), Data);
				auto* Path = OnNav ? UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Body->GetActorLocation(), Projected.Location, this) : nullptr;
				UE_LOG(LogTemp, Display, TEXT("[NPCCarry] data=%s agent=%.1f/%.1f box=%s projected=%d %s path=%d partial=%d points=%d"),
				 *GetNameSafe(Data), Props.AgentRadius, Props.AgentHeight, *Front.ToCompactString(), OnNav, *Projected.Location.ToCompactString(),
				 Path && Path->IsValid(), Path && Path->IsPartial(), Path ? Path->PathPoints.Num() : 0);
				if (Path) for (const FVector& Point : Path->PathPoints) UE_LOG(LogTemp, Display, TEXT("[NPCPathPoint] %s"), *Point.ToCompactString());

			}
		}

		if (HumanProgressAt > 0 && FVector::DistSquared2D(HumanProgressLocation, Body->GetActorLocation()) < FMath::Square(15.f))
		{
			++BoxApproach;
			NextWalk = 0;
			NextBoxPlan = 0;
			if (!Body->HasCarriedToy() && Target.IsValid())
			{
				OverlookUntil.Add(Target, Now + 8);
				Target.Reset();
				StopMovement();
			}
		}
		HumanProgressLocation = Body->GetActorLocation();
		HumanProgressAt = Now + 4;
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
				OverlookUntil.Add(TWeakObjectPtr<AActor>(Seen), Now + FMath::FRandRange(4.f, 7.f));
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
		ForgetAt = Now + 3;
		SetFocus(Seen);
		if (Nearest <= FMath::Square(TB::Rules().Aim.CaptureDistance) && TB::FindInteractionTarget(Body).Actor == Seen)
		{
			StopMovement();
			Body->RequestInteract();
		}
		else WalkTo(LastSeen, 15.f);
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
		const FVector Delta = Position + FVector(0, 0, 15) - Eye;
		if (Delta.SizeSquared() > FMath::Square(1800.f) ||
		    FVector::DotProduct(It->GetControlRotation().Vector(), Delta.GetSafeNormal()) < .65f) continue;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(NPCCover), true, It);
		Query.AddIgnoredActor(GetPawn());
		if (!GetWorld()->LineTraceTestByChannel(Eye, Position + FVector(0, 0, 15), ECC_Visibility, Query))
			return true;
	}
	return false;
}

bool ATBNPCController::PathExposed(const TArray<FVector>& Points) const
{
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const int32 Samples = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Points[Index - 1], Points[Index]) / 150.f));
		for (int32 Sample = 1; Sample <= Samples; ++Sample)
			if (Exposed(FMath::Lerp(Points[Index - 1], Points[Index], float(Sample) / Samples))) return true;
	}
	return false;
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
		WalkTo(CoverGoal, 5.f);
		return;
	}
	// Remember a discovered stationary battery briefly while turning around furniture.
	if (BatteryTarget.IsValid() && CanSee(BatteryTarget.Get())) BatteryVisibleUntil = Now + 12;
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
		if (Distance < ClosestBatteryDistance - 10.f)
		{
			ClosestBatteryDistance = Distance;
			BatteryProgressAt = Now + 6;
		}
		else if (Now >= BatteryProgressAt)
		{
			BlockedBattery = BatteryTarget;
			RetryBatteryAt = Now + 15;
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
			ReplanAt = Now + FMath::FRandRange(1.5f, 2.f);
		}
		if (Safe)
		{
			WalkTo(BatteryTarget->GetActorLocation(), 5.f);
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
	const int32 Budget = FMath::Min(3, Batteries.Num());
	for (int32 Index = 0; Index < Budget; ++Index)
	{
		auto* It = Batteries[(BatteryCursor + Index) % Batteries.Num()];
		auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Origin, It->GetActorLocation(), this);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		const bool Unsafe = PathExposed(Path->PathPoints);
		const float Score = Path->GetPathLength() + (Unsafe ? 10000.f : 0.f);
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
		BatteryVisibleUntil = Now + 12;
		ClosestBatteryDistance = FVector::Dist2D(Origin, BatteryTarget->GetActorLocation());
		BatteryProgressAt = Now + 6;
	}
	BatteryCursor = Batteries.Num() ? (BatteryCursor + Budget) % Batteries.Num() : 0;
	ReplanAt = Now + FMath::FRandRange(1.5f, 2.f);
	// When threatened, prefer nearby cover over crossing an exposed route to a battery.
	if (Danger || UnsafeRoute)
	{
		BatteryTarget.Reset();
		const FVector BatteryGoal = Best;
		Found = false;
		BestScore = TNumericLimits<float>::Max();
		for (int32 Attempt = 0; Attempt < 4; ++Attempt)
		{
			FNavLocation Cover;
			if (!Nav->GetRandomReachablePointInRadius(Origin, 650.f, Cover, Data) || Exposed(Cover.Location)) continue;
			auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Origin, Cover.Location, this);
			if (!Path || !Path->IsValid() || Path->IsPartial() || PathExposed(Path->PathPoints)) continue;
			const float Score = Path->GetPathLength() + FVector::Dist(BatteryGoal, Cover.Location) * .25f;
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
			CoverUntil = GetWorld()->GetTimeSeconds() + 1.5;
		}
	}
	if (Found) WalkTo(Best, 5.f);
	else if (Danger) StopMovement();
	else Wander();
}
