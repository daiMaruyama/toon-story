#include "TBContactReactiveComponent.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

namespace
{
	constexpr float StepDegrees = .5f;
	constexpr int32 MaxStepsPerTick = 8;
	bool UniformPositive(FVector Scale)
	{
		return !Scale.ContainsNaN() && Scale.GetMin() > SMALL_NUMBER &&
		    FMath::IsNearlyEqual(Scale.X, Scale.Y, .0001) && FMath::IsNearlyEqual(Scale.X, Scale.Z, .0001);
	}
}

UTBContactReactiveComponent::UTBContactReactiveComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}

bool UTBContactReactiveComponent::ValidateSetup(FString& Error) const
{
	auto* Owner = GetOwner();
	const auto* Box = Cast<UBoxComponent>(MotionTarget.GetComponent(Owner));
	const auto* Sensor = Cast<UPrimitiveComponent>(ContactTrigger.GetComponent(Owner));
	if (!IsValid(Owner) || Owner->IsA<APawn>() || !Owner->GetIsReplicated() || Owner->IsReplicatingMovement() || !GetIsReplicated())
	{ Error = TEXT("Use a replicated prop actor/component, without Replicate Movement or a Pawn owner."); return false; }
	if (!IsValid(Box) || !IsValid(Sensor) || Box == Sensor || Box->GetOwner() != Owner || Sensor->GetOwner() != Owner ||
	    Box == Owner->GetRootComponent() || !Box->GetAttachParent() || Sensor->IsAttachedTo(Box))
	{ Error = TEXT("Choose a child Box proxy and a separate stationary sensor on the same actor."); return false; }
	if (Box->Mobility != EComponentMobility::Movable || Box->IsSimulatingPhysics() || Box->GetCollisionObjectType() != ECC_WorldDynamic || Box->GetUnscaledBoxExtent().ContainsNaN() ||
	    Box->GetUnscaledBoxExtent().GetMin() <= 0 || !Box->IsQueryCollisionEnabled() ||
	    Box->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block || Box->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block)
	{ Error = TEXT("The movable box must have positive extents, no physics, and query-block Human and ToyPawn."); return false; }
	if (Sensor->GetCollisionEnabled() != ECollisionEnabled::QueryOnly || Sensor->GetCollisionObjectType() != ECC_WorldDynamic ||
	    !Sensor->GetGenerateOverlapEvents() || Sensor->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap ||
	    Sensor->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Overlap)
	{ Error = TEXT("Sensor needs QueryOnly/WorldDynamic, overlap events, and Human/ToyPawn Overlap."); return false; }
	for (const USceneComponent* Part = Box; Part; Part = Part->GetAttachParent())
		if (!UniformPositive(Part->GetRelativeScale3D()) || !UniformPositive(Part->GetComponentScale()))
		{ Error = TEXT("Only positive uniform scales throughout the parent chain are supported."); return false; }
	TArray<UPrimitiveComponent*> Primitives; Owner->GetComponents(Primitives);
	for (const auto* Part : Primitives)
		if (Part != Box && Part != Sensor && Part->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{ Error = TEXT("Other primitives on the prop must be NoCollision; keep frames/supports on separate actors."); return false; }
	TArray<UTBContactReactiveComponent*> Drivers; Owner->GetComponents(Drivers);
	for (const auto* Driver : Drivers)
		if (Driver != this && Driver->MotionTarget.GetComponent(Owner) == Box)
		{ Error = TEXT("A box may have only one contact motion driver."); return false; }
	if (LocalAxis.ContainsNaN() || LocalAxis.IsNearlyZero() || !FMath::IsFinite(LocalAxis.SizeSquared()) || LocalPivot.ContainsNaN() ||
	    !FMath::IsFinite(AngleDegrees) || FMath::Abs(AngleDegrees) < 1 || FMath::Abs(AngleDegrees) > 110 ||
	    !FMath::IsFinite(MotionSeconds) || MotionSeconds < .1f || MotionSeconds > 10 ||
	    !FMath::IsFinite(CooldownSeconds) || CooldownSeconds < .05f || CooldownSeconds > 30 ||
	    !FMath::IsFinite(MaxContactGapCm) || MaxContactGapCm < .1f || MaxContactGapCm > 5 ||
	    !FMath::IsFinite(TimeoutSeconds) || TimeoutSeconds < .5f || TimeoutSeconds > 30)
	{ Error = TEXT("Axis/pivot and bounded angle, duration, cooldown and timeout must be finite and in range."); return false; }
	Error.Reset(); return true;
}

bool UTBContactReactiveComponent::BindSetup()
{
	FString Error;
	if (!ValidateSetup(Error)) { Problem = Error; return false; }
	Target = CastChecked<UBoxComponent>(MotionTarget.GetComponent(GetOwner()));
	Trigger = CastChecked<UPrimitiveComponent>(ContactTrigger.GetComponent(GetOwner()));
	FixedParent = Target->GetAttachParent();
	ParentAtRest = FixedParent->GetComponentTransform();
	AxisAtRest = LocalAxis; PivotAtRest = LocalPivot; ExtentAtRest = Target->GetUnscaledBoxExtent();
	LimitAtRest = AngleDegrees; DurationAtRest = MotionSeconds; CooldownAtRest = CooldownSeconds; TimeoutAtRest = TimeoutSeconds;
	GapAtRest = MaxContactGapCm;
	bReturnAtRest = bReturnAfterContact; bOnceAtRest = bOncePerRound;
	bReady = true;
	return true;
}

void UTBContactReactiveComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!BindSetup()) return;
	if (GetOwner()->HasAuthority())
	{
		Snapshot.bInitialized = true;
		Snapshot.Baseline = Snapshot.Pose = Target->GetRelativeTransform();
		Snapshot.Status = ETBContactMotionStatus::Idle;
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &UTBContactReactiveComponent::ContactBegan);
		Trigger->OnComponentEndOverlap.AddDynamic(this, &UTBContactReactiveComponent::ContactEnded);
		SyncRound(GetWorld()->GetTimeSeconds());
		Publish();
	}
	else OnRep_Snapshot();
}

void UTBContactReactiveComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(Trigger))
	{
		Trigger->OnComponentBeginOverlap.RemoveDynamic(this, &UTBContactReactiveComponent::ContactBegan);
		Trigger->OnComponentEndOverlap.RemoveDynamic(this, &UTBContactReactiveComponent::ContactEnded);
	}
	Contacts.Reset(); bReady = false;
	Super::EndPlay(Reason);
}

bool UTBContactReactiveComponent::RuntimeSetupValid() const
{
	FString Error;
	return bReady && ValidateSetup(Error) && IsValid(Target) && IsValid(Trigger) && FixedParent.IsValid() &&
	    MotionTarget.GetComponent(GetOwner()) == Target && ContactTrigger.GetComponent(GetOwner()) == Trigger &&
	    Target->GetAttachParent() == FixedParent.Get() && FixedParent->GetComponentTransform().Equals(ParentAtRest, .001f) &&
	    Target->GetUnscaledBoxExtent().Equals(ExtentAtRest, .001f) && LocalAxis.Equals(AxisAtRest, .001f) && LocalPivot.Equals(PivotAtRest, .001f) &&
	    AngleDegrees == LimitAtRest && MotionSeconds == DurationAtRest && CooldownSeconds == CooldownAtRest && TimeoutSeconds == TimeoutAtRest &&
	    MaxContactGapCm == GapAtRest &&
	    bReturnAfterContact == bReturnAtRest && bOncePerRound == bOnceAtRest &&
	    Target->GetRelativeTransform().Equals(Snapshot.Pose, .01f);
}

void UTBContactReactiveComponent::Publish()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	Snapshot.Problem = Problem;
	GetOwner()->FlushNetDormancy();
	GetOwner()->ForceNetUpdate();
}

void UTBContactReactiveComponent::Disable(const FString& Why)
{
	if (Snapshot.Status == ETBContactMotionStatus::Disabled && Problem == Why) return;
	Snapshot.Status = ETBContactMotionStatus::Disabled;
	Problem = Why; bResetting = false;
	Publish();
}

void UTBContactReactiveComponent::OnRep_Snapshot()
{
	// No local timeline, contact processing, or extrapolation beyond this server-approved snapshot.
	if (!GetOwner() || GetOwner()->HasAuthority() || !Snapshot.bInitialized || !bReady || !IsValid(Target) || !FixedParent.IsValid()) return;
	if (Snapshot.Pose.ContainsNaN() || !FixedParent->GetComponentTransform().Equals(ParentAtRest, .001f)) return;
	Target->SetRelativeTransform(Snapshot.Pose, false, nullptr, ETeleportType::None);
}

void UTBContactReactiveComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTBContactReactiveComponent, Snapshot);
}

void UTBContactReactiveComponent::RebuildContacts(ATBCharacter* Entering)
{
	Contacts.Reset();
	TArray<UPrimitiveComponent*> Overlaps; Trigger->GetOverlappingComponents(Overlaps);
	for (auto* Part : Overlaps)
		if (auto* Body = Cast<ATBCharacter>(Part->GetOwner()); IsValid(Body) && Body != Entering && Part == Body->GetCapsuleComponent()) Contacts.Add(Body);
	bArmed = Contacts.IsEmpty();
}

bool UTBContactReactiveComponent::SyncRound(double Now, ATBCharacter* Entering)
{
	auto* State = TB::GS(GetWorld());
	if (!State) return false;
	if (ObservedState.Get() == State && ObservedRound == State->HidePreparation.Round && ObservedPhase == uint8(State->Phase)) return true;
	ObservedState = State; ObservedRound = State->HidePreparation.Round; ObservedPhase = uint8(State->Phase);
	Snapshot.Round = ObservedRound; ++Snapshot.Generation;
	bReactedThisRound = false; bReturning = false; GoalAngle = 0; NextContactTime = Now;
	RebuildContacts(Entering);
	// Reset uses precisely the same swept-envelope checks as contact motion.
	bResetting = !FMath::IsNearlyZero(Snapshot.Angle);
	Deadline = Now + TimeoutAtRest;
	Snapshot.Status = bResetting ? ETBContactMotionStatus::ResetPending : ETBContactMotionStatus::Idle;
	Problem.Reset(); Publish();
	return true;
}

void UTBContactReactiveComponent::ContactBegan(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
    int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bReady || Overlapped != Trigger || OtherActor == GetOwner()) return;
	auto* Body = Cast<ATBCharacter>(OtherActor);
	if (!IsValid(Body) || Body->IsActorBeingDestroyed() || OtherComponent != Body->GetCapsuleComponent() ||
	    !Trigger->IsOverlappingComponent(OtherComponent)) return;
	if (!RuntimeSetupValid()) { Disable(TEXT("Prop configuration, parent, or external motion changed.")); return; }
	if (!NearSurface(Body)) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (!SyncRound(Now, Body)) return;
	const bool bNewActor = !Contacts.Contains(Body); Contacts.Add(Body);
	const auto* State = TB::GS(GetWorld());
	if (!bNewActor || !bArmed || Contacts.Num() != 1 || Now < NextContactTime ||
	    Snapshot.Status != ETBContactMotionStatus::Idle || (bOnceAtRest && bReactedThisRound) ||
	    !State || State->Phase != ETBPhase::Playing || State->IsPreparing() || !Body->CanAct() ||
	    !IsValid(Body->GetController()) || Body->GetController()->GetPawn() != Body ||
	    Body->GetController()->GetPlayerState<ATBPlayerState>() != Body->TBPS()) return;
	bArmed = false; bReactedThisRound = true; bReturning = false;
	NextContactTime = Now + CooldownAtRest; Deadline = Now + TimeoutAtRest;
	GoalAngle = LimitAtRest; Snapshot.Status = ETBContactMotionStatus::Moving;
	++Snapshot.Generation; Problem.Reset(); Publish();
}

void UTBContactReactiveComponent::ContactEnded(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 BodyIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Overlapped != Trigger) return;
	auto* Body = Cast<ATBCharacter>(OtherActor);
	if (Body && OtherComponent == Body->GetCapsuleComponent()) Contacts.Remove(Body);
	if (Contacts.IsEmpty()) bArmed = true;
}

bool UTBContactReactiveComponent::NearSurface(const ATBCharacter* Body) const
{
	const auto* Capsule = Body->GetCapsuleComponent();
	const FVector Center = Capsule->GetComponentLocation();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Axis = Capsule->GetUpVector() * FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Radius);
	const FTransform BoxTransform = Target->GetComponentTransform();
	const FVector A = BoxTransform.InverseTransformPosition(Center - Axis), B = BoxTransform.InverseTransformPosition(Center + Axis);
	const FVector Extent = Target->GetUnscaledBoxExtent();
	auto DistanceSquared = [&](double Alpha)
	{
		const FVector P = FMath::Lerp(A, B, Alpha);
		return (P - P.BoundToBox(-Extent, Extent)).SizeSquared();
	};
	// Distance to a convex box along a segment is convex. A finite search errs toward refusing contact.
	double Low = 0, High = 1;
	for (int32 Step = 0; Step < 24; ++Step)
	{
		const double Left = (2 * Low + High) / 3, Right = (Low + 2 * High) / 3;
		if (DistanceSquared(Left) < DistanceSquared(Right)) High = Right; else Low = Left;
	}
	const double Distance = FMath::Min3(DistanceSquared(0), DistanceSquared(1), DistanceSquared((Low + High) * .5));
	return Distance <= FMath::Square((Radius + GapAtRest) / Target->GetComponentScale().X);
}

FTransform UTBContactReactiveComponent::PoseAt(float Angle) const
{
	const FQuat Rotation(AxisAtRest.GetSafeNormal(), FMath::DegreesToRadians(Angle));
	return FTransform(Rotation * Snapshot.Baseline.GetRotation(),
	    PivotAtRest + Rotation.RotateVector(Snapshot.Baseline.GetLocation() - PivotAtRest), Snapshot.Baseline.GetScale3D());
}

bool UTBContactReactiveComponent::SegmentClear(float From, float To) const
{
	if (!FMath::IsFinite(From) || !FMath::IsFinite(To) || FMath::Abs(To - From) > StepDegrees + .001f) return false;
	const FTransform Middle = PoseAt((From + To) * .5f) * ParentAtRest;
	const FTransform RestWorld = Snapshot.Baseline * ParentAtRest;
	const FVector PivotWorld = ParentAtRest.TransformPosition(PivotAtRest);
	float Radius = 0;
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector P = ExtentAtRest * FVector(Corner & 1 ? 1 : -1, Corner & 2 ? 1 : -1, Corner & 4 ? 1 : -1);
		Radius = FMath::Max(Radius, float(FVector::Distance(RestWorld.TransformPosition(P), PivotWorld)));
	}
	const float Padding = 2 * Radius * FMath::Sin(FMath::DegreesToRadians(FMath::Abs(To - From)) * .25f) + .05f;
	const FVector Extent = ExtentAtRest * Middle.GetScale3D() + FVector(Padding);
	if (!FMath::IsFinite(Radius) || !FMath::IsFinite(Padding) || Extent.ContainsNaN() || Middle.ContainsNaN()) return false;
	const FTransform BoxSpace(Middle.GetRotation(), Middle.GetLocation());
	const FBox QueryBox(-Extent, Extent);
	// Real character capsules are protected even if their collision response (or held-state collision) ignores this prop.
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		const auto* Capsule = It->GetCapsuleComponent();
		const FVector C = Capsule->GetComponentLocation();
		const FVector E = FVector(Capsule->GetScaledCapsuleRadius()) + Capsule->GetUpVector().GetAbs() *
		    FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
		if (QueryBox.Intersect(FBox(C - E, C + E).TransformBy(BoxSpace.Inverse()))) return false;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ContactPropClearance), false, GetOwner());
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Middle.GetLocation(), Middle.GetRotation(),
	    FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects), FCollisionShape::MakeBox(Extent), Query);
	for (const auto& Overlap : Overlaps)
	{
		const auto* Part = Overlap.GetComponent();
		if (!IsValid(Part) || !IsValid(Part->GetOwner()) || Part->GetOwner()->IsActorBeingDestroyed()) continue;
		if (Part->GetCollisionResponseToChannel(Target->GetCollisionObjectType()) == ECR_Block &&
		    Target->GetCollisionResponseToChannel(Part->GetCollisionObjectType()) == ECR_Block) return false;
	}
	return true;
}

bool UTBContactReactiveComponent::ApplyApproved(float Angle)
{
	const FTransform Pose = PoseAt(Angle);
	Target->SetRelativeTransform(Pose, false, nullptr, ETeleportType::None);
	if (!IsValid(Target) || !Target->GetRelativeTransform().Equals(Pose, .001f))
	{ Disable(TEXT("Approved pose could not be applied; motion stopped.")); return false; }
	Snapshot.Angle = Angle; Snapshot.Pose = Pose; return true;
}

void UTBContactReactiveComponent::AdvanceServer(float DeltaTime, double Now)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !bReady) return;
	if (!RuntimeSetupValid()) { Disable(TEXT("Prop configuration, parent, or external driver changed.")); return; }
	if (!SyncRound(Now)) { Disable(TEXT("Match state is unavailable.")); return; }
	for (auto It = Contacts.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
	if (Contacts.IsEmpty()) bArmed = true;
	if (Snapshot.Status == ETBContactMotionStatus::Disabled || Snapshot.Status == ETBContactMotionStatus::Idle || Snapshot.Status == ETBContactMotionStatus::Held) return;
	if (Now >= Deadline) { Disable(bResetting ? TEXT("Reset timed out; safe pose retained, new contacts disabled.") : TEXT("Motion timed out; safe pose retained.")); return; }
	float Budget = FMath::Abs(LimitAtRest) / DurationAtRest * FMath::Clamp(DeltaTime, 0.f, .25f);
	for (int32 Step = 0; Step < MaxStepsPerTick && Budget > SMALL_NUMBER; ++Step)
	{
		const float Delta = GoalAngle - Snapshot.Angle;
		if (FMath::IsNearlyZero(Delta, .001f)) break;
		const float Amount = FMath::Min3(FMath::Abs(Delta), Budget, StepDegrees);
		const float Next = Snapshot.Angle + FMath::Sign(Delta) * Amount;
		if (!SegmentClear(Snapshot.Angle, Next))
		{
			Snapshot.Status = bResetting ? ETBContactMotionStatus::ResetPending : ETBContactMotionStatus::Blocked;
			Problem = bResetting ? TEXT("Reset clearance occupied; waiting at the approved pose.") : TEXT("Motion clearance occupied; waiting at the approved pose.");
			Publish(); return;
		}
		if (!ApplyApproved(Next)) return;
		Budget -= Amount;
	}
	if (FMath::IsNearlyEqual(Snapshot.Angle, GoalAngle, .001f))
	{
		if (!bResetting && bReturnAtRest && !bReturning)
		{ bReturning = true; GoalAngle = 0; Snapshot.Status = ETBContactMotionStatus::Moving; }
		else
		{
			Snapshot.Status = bResetting || bReturning ? ETBContactMotionStatus::Idle : ETBContactMotionStatus::Held;
			bResetting = false;
		}
	}
	else Snapshot.Status = bResetting ? ETBContactMotionStatus::ResetPending : ETBContactMotionStatus::Moving;
	Problem.Reset(); Publish();
}

void UTBContactReactiveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (GetOwner() && GetOwner()->HasAuthority()) AdvanceServer(DeltaTime, GetWorld()->GetTimeSeconds());
}
