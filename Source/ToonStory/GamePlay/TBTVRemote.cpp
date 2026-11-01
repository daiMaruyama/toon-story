// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBTVRemote.h"
#include "Audio/TBAudioEvents.h"
#include "Audio/TBAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "TBBoxTV.h"
#include "TBTVButton.h"
#include "Character/TBCharacter.h"
#include "Data/TBPresentationData.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
// M_RemoteGlow の Btn 値。
constexpr float GlowPressed = 2.f;      // 光って沈む
constexpr float GlowPreview = 1.25f;    // 選択予告。clientでは操作成功を意味しない。
constexpr float GlowAvailable = 1.f;    // 光る
constexpr float GlowUnavailable = -.6f; // 暗くする
// World centimetres/seconds, independent of the authored remote's scale.
constexpr double DwellSeconds = .20;
constexpr double RearmSeconds = .10;
constexpr double MaxObservationGap = .25;
constexpr float MaxSpeed = 2.f;
constexpr float MaxDrift = .5f;
constexpr float EdgeMargin = .20f;
constexpr float NearestMargin = .25f;
}

ATBTVRemote::ATBTVRemote()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

void ATBTVRemote::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EvaluateContacts(GetWorld()->GetTimeSeconds());
	UpdateGlow();
}

void ATBTVRemote::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Contacts.Empty();
	if (IsValid(TV)) TV->OnPowerChanged.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

void ATBTVRemote::BeginPlay()
{
	Super::BeginPlay();
	// 光り方は全員の画面で変えるので、検知はクライアントでも行う。テレビの操作はサーバーだけ。
	if (GetNetMode() != NM_DedicatedServer)
	{
		GlowMaterial = Mesh->CreateDynamicMaterialInstance(0);
	}
	GetComponents(Buttons);
	for (UTBTVButton* Button : Buttons)
	{
		Button->OnComponentBeginOverlap.AddDynamic(this, &ATBTVRemote::OnButtonOverlap);
		Button->OnComponentEndOverlap.AddDynamic(this, &ATBTVRemote::OnButtonEndOverlap);
		GlowParameters.Add(Button->GetGlowParameterName());
	}
	if (IsValid(TV))
	{
		TV->OnPowerChanged.AddUObject(this, &ATBTVRemote::UpdateGlow);
	}
	UpdateGlow();
}

void ATBTVRemote::UpdateGlow()
{
	ATBCharacter* Viewer = nullptr;
	const UTBTVButton* Preview = PreviewButton(Viewer);
	const FContact* Contact = Contacts.Find(Viewer);
	const bool bConfirmed = HasAuthority() && Contact && Contact->Latched == Preview;
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		UpdateButtonGlow(Index, Preview, bConfirmed);
	}
}

void ATBTVRemote::UpdateButtonGlow(int32 Index, const UTBTVButton* Preview, bool bConfirmed)
{
	if (!GlowMaterial || !Buttons.IsValidIndex(Index))
	{
		return;
	}
	const UTBTVButton* Button = Buttons[Index];
	if (!IsValid(Button)) return;
	// 電源OFF中に押せるのは電源ボタンだけ。押せないボタンは踏んでも沈ませない。
	const bool bUsable = (IsValid(TV) && TV->IsPowerOn()) || Button->CurrentKey == ETBRemoteKey::Power;
	const float State = !bUsable ? GlowUnavailable : Preview == Button ? (bConfirmed ? GlowPressed : GlowPreview) : GlowAvailable;
	GlowMaterial->SetScalarParameterValue(GlowParameters[Index], State);
}

void ATBTVRemote::OnButtonEndOverlap(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32)
{
	UpdateGlow();
}

void ATBTVRemote::OnButtonOverlap(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32, bool,
                                  const FHitResult&)
{
	UpdateGlow();
	// BeginOverlap is only evidence of contact, never permission to operate immediately.
}

bool ATBTVRemote::IsEligible(const ATBCharacter* Viewer) const
{
	const auto* State = TB::GS(GetWorld());
	const auto* Player = IsValid(Viewer) ? Viewer->TBPS() : nullptr;
	const auto* Controller = IsValid(Viewer) ? Viewer->GetController() : nullptr;
	return IsValid(TV) && TV->GetWorld() == GetWorld() && State &&
	       State->Phase == ETBPhase::Playing && !State->IsPreparing() && State->EndTime > GetWorld()->GetTimeSeconds() &&
	       Player && Player->Team == ETBTeam::Toy && Player->ToyState == ETBToyState::Boxed &&
	       IsValid(Controller) && Controller->GetPawn() == Viewer && Controller->GetPlayerState<ATBPlayerState>() == Player &&
	       Viewer->GetWorld() == GetWorld() && !Viewer->IsActorBeingDestroyed() &&
	       Viewer->GetCapsuleComponent()->IsRegistered() && Viewer->GetCapsuleComponent()->GetGenerateOverlapEvents() &&
	       Viewer->GetCapsuleComponent()->IsQueryCollisionEnabled();
}

bool ATBTVRemote::SupportPoint(const ATBCharacter* Viewer, FVector& Point) const
{
	const auto* Movement = Viewer->GetCharacterMovement();
	const auto* Capsule = Viewer->GetCapsuleComponent();
	if (!Movement || !Movement->IsMovingOnGround() || !IsValid(Mesh) ||
	    !Mesh->IsQueryCollisionEnabled() || Mesh->GetCollisionResponseToChannel(Capsule->GetCollisionObjectType()) != ECR_Block)
		return false;
	// Gravity stays world-Z; the support hit and button regions use their actual rotated transforms.
	const FVector Base = Capsule->GetComponentLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TVRemoteSupport), false, Viewer);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Base + FVector(0, 0, 2), Base - FVector(0, 0, 4),
	                                       Capsule->GetCollisionObjectType(), Query) ||
	    Hit.bStartPenetrating || Hit.GetComponent() != Mesh || !Movement->IsWalkable(Hit) ||
	    Base.Z - Hit.ImpactPoint.Z < -.1f || Base.Z - Hit.ImpactPoint.Z > 4.f)
		return false;
	Point = Hit.ImpactPoint;
	return !Point.ContainsNaN();
}

bool ATBTVRemote::InButton(const UTBTVButton* Button, const FVector& Point, float Margin) const
{
	if (!IsValid(Button) || Button->GetOwner() != this || !Button->IsRegistered() || !Button->IsQueryCollisionEnabled()) return false;
	const FTransform Transform = Button->GetComponentTransform();
	const FVector Scale = Transform.GetScale3D();
	if (Transform.ContainsNaN() || Scale.GetMin() <= SMALL_NUMBER) return false;
	const FVector Local = Transform.InverseTransformPosition(Point);
	const FVector Extent = Button->GetUnscaledBoxExtent();
	if (Local.ContainsNaN() || Extent.ContainsNaN() || Extent.X <= 0 || Extent.Y <= 0) return false;
	return Extent.X * Scale.X > Margin && Extent.Y * Scale.Y > Margin &&
	       FMath::Abs(Local.X) * Scale.X < Extent.X * Scale.X - Margin &&
	       FMath::Abs(Local.Y) * Scale.Y < Extent.Y * Scale.Y - Margin;
}

UTBTVButton* ATBTVRemote::SelectButton(const ATBCharacter* Viewer, const FVector& Point) const
{
	UTBTVButton* Best = nullptr;
	float First = TNumericLimits<float>::Max(), Second = First;
	const FTransform Transform = Mesh->GetComponentTransform();
	const FVector X = Transform.GetUnitAxis(EAxis::X), Y = Transform.GetUnitAxis(EAxis::Y);
	for (UTBTVButton* Button : Buttons)
	{
		if (!IsValid(Button) || !Button->GetGenerateOverlapEvents() ||
		    Button->GetCollisionResponseToChannel(Viewer->GetCapsuleComponent()->GetCollisionObjectType()) != ECR_Overlap ||
		    Viewer->GetCapsuleComponent()->GetCollisionResponseToChannel(Button->GetCollisionObjectType()) == ECR_Ignore ||
		    !Button->IsOverlappingComponent(Viewer->GetCapsuleComponent())) continue;
		const FVector Delta = Point - Button->GetComponentLocation();
		const float Distance = FMath::Sqrt(FMath::Square(FVector::DotProduct(Delta, X)) + FMath::Square(FVector::DotProduct(Delta, Y)));
		if (Distance < First) { Second = First; First = Distance; Best = Button; }
		else Second = FMath::Min(Second, Distance);
	}
	// Rank all visible keys, including disabled digits while power is off; never snap to a neighbouring usable key.
	return Best && Second - First > NearestMargin && InButton(Best, Point, EdgeMargin) ? Best : nullptr;
}

UTBTVButton* ATBTVRemote::PreviewButton(ATBCharacter*& Viewer) const
{
	Viewer = nullptr;
	TSet<ATBCharacter*> Riders;
	for (UTBTVButton* Button : Buttons)
	{
		if (!IsValid(Button)) continue;
		TArray<UPrimitiveComponent*> Overlaps;
		Button->GetOverlappingComponents(Overlaps);
		for (auto* Part : Overlaps)
		{
			auto* Body = Cast<ATBCharacter>(Part->GetOwner());
			if (IsValid(Body) && !Body->IsActorBeingDestroyed() && Part == Body->GetCapsuleComponent()) Riders.Add(Body);
		}
	}
	FVector Point;
	for (ATBCharacter* Body : Riders)
	{
		FVector Hit;
		if (!SupportPoint(Body, Hit)) continue;
		if (Viewer) { Viewer = nullptr; return nullptr; }
		Viewer = Body; Point = Hit;
	}
	if (!IsEligible(Viewer) || Viewer->GetVelocity().ContainsNaN() || Viewer->GetVelocity().Size2D() > MaxSpeed) return nullptr;
	auto* Button = SelectButton(Viewer, Point);
	return Button && (TV->IsPowerOn() || Button->CurrentKey == ETBRemoteKey::Power) ? Button : nullptr;
}

void ATBTVRemote::EvaluateContacts(double Now)
{
	if (!HasAuthority()) return;
	auto* State = TB::GS(GetWorld());
	if (!State || State->Phase != ETBPhase::Playing || State->IsPreparing() || !IsValid(TV) ||
	    State->EndTime <= GetWorld()->GetTimeSeconds() || !FMath::IsFinite(Now))
	{
		Contacts.Empty(); LastEvaluation = -1; LastPressTime = -1; return;
	}
	const bool bNewContext = ContactState != State || ContactTV != TV || ContactRound != State->HidePreparation.Round ||
	                         !ContactTransform.Equals(Mesh->GetComponentTransform());
	const bool bGap = LastEvaluation < 0 || Now < LastEvaluation || Now - LastEvaluation > MaxObservationGap;
	if (bNewContext) { Contacts.Empty(); LastPressTime = -1; }
	ContactState = State; ContactTV = TV; ContactRound = State->HidePreparation.Round;
	ContactTransform = Mesh->GetComponentTransform(); LastEvaluation = Now;
	TSet<TWeakObjectPtr<ATBCharacter>> Riders;
	for (UTBTVButton* Button : Buttons)
	{
		if (!IsValid(Button)) continue;
		TArray<UPrimitiveComponent*> Overlaps;
		Button->GetOverlappingComponents(Overlaps);
		for (auto* Part : Overlaps)
		{
			auto* Body = Cast<ATBCharacter>(Part->GetOwner());
			if (IsValid(Body) && !Body->IsActorBeingDestroyed() && Part == Body->GetCapsuleComponent()) Riders.Add(Body);
		}
	}
	// One record per possessed character, never one per overlapping button/body component.
	for (auto It = Contacts.CreateIterator(); It; ++It)
	{
		if (!IsEligible(It.Key().Get()) || It.Value().Player != It.Key()->TBPS()) It.RemoveCurrent();
	}
	int32 SupportedCount = 0;
	for (const auto& Rider : Riders) { FVector Point; SupportedCount += SupportPoint(Rider.Get(), Point) ? 1 : 0; }
	// Keep latched records after overlap ends until a clear departure has been observed for .10s.
	for (const auto& Rider : Riders)
		if (IsEligible(Rider.Get())) Contacts.FindOrAdd(Rider).Player = Rider->TBPS();
	for (auto& Pair : Contacts)
	{
		auto* Body = Pair.Key.Get(); FContact& Contact = Pair.Value;
		FVector Point;
		const bool bSupported = Riders.Contains(Body) && SupportPoint(Body, Point);
		if (Contact.Latched.IsValid())
		{
			const FVector Base = Body->GetCapsuleComponent()->GetComponentLocation() - FVector(0, 0, Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			auto* OtherKey = bSupported && SupportedCount == 1 ? SelectButton(Body, Point) : nullptr;
			// An ambiguous gap/tie is not proof of departure. Moving clearly onto another key or fully leaving contact is.
			const bool bLeft = !Contact.Latched->IsOverlappingComponent(Body->GetCapsuleComponent()) ||
			    (OtherKey && OtherKey != Contact.Latched.Get() && !InButton(Contact.Latched.Get(), bSupported ? Point : Base, -EdgeMargin));
			if (!bLeft || bGap) Contact.LeftSince = -1;
			else if (Contact.LeftSince < 0) Contact.LeftSince = Now;
			else if (Now - Contact.LeftSince >= RearmSeconds) { Contact.Latched.Reset(); Contact.LeftSince = -1; }
		}
		auto* Button = bSupported && SupportedCount == 1 ? SelectButton(Body, Point) : nullptr;
		if (bGap || Contact.Latched.IsValid() || !Button || Body->GetVelocity().ContainsNaN() || Body->GetVelocity().Size2D() > MaxSpeed ||
		    (Button->CurrentKey != ETBRemoteKey::Power && !TV->IsPowerOn()))
		{
			Contact.Candidate.Reset(); Contact.Since = -1; continue;
		}
		if (Contact.Candidate != Button || Contact.Since < 0 || FVector::Distance(Contact.Anchor, Point) > MaxDrift ||
		    Contact.Key != uint8(Button->CurrentKey) || Contact.Digit != Button->Digit || !Contact.ButtonTransform.Equals(Button->GetComponentTransform()))
		{
			Contact.Candidate = Button; Contact.Anchor = Point; Contact.Since = Now;
			Contact.Key = uint8(Button->CurrentKey); Contact.Digit = Button->Digit; Contact.ButtonTransform = Button->GetComponentTransform();
			continue;
		}
		if (Now - Contact.Since < DwellSeconds ||
		    (LastPressTime >= 0 && Now - LastPressTime < TB::PresentationData().Remote.PressCooldown)) continue;
		// Revalidate at dispatch; no queued timer can operate using an old pawn/button/role.
		if (IsEligible(Body) && Button->IsOverlappingComponent(Body->GetCapsuleComponent()) && Dispatch(Body, Button, Now))
		{
			Contact.Latched = Button; Contact.Candidate.Reset(); Contact.Since = -1;
		}
	}
}

bool ATBTVRemote::Dispatch(ATBCharacter* Viewer, const UTBTVButton* Button, double Now)
{
	if (!HasAuthority() || !IsEligible(Viewer) || !IsValid(Button)) return false;
	bool bHandled = false;
	switch (Button->CurrentKey)
	{
		case ETBRemoteKey::Digit:
			bHandled = TV->SelectChannel(Viewer, Button->Digit);
			break;
		case ETBRemoteKey::Next:
			bHandled = TV->StepChannel(Viewer, 1);
			break;
		case ETBRemoteKey::Prev:
			bHandled = TV->StepChannel(Viewer, -1);
			break;
		case ETBRemoteKey::Power:
			bHandled = TV->TogglePower(Viewer);
			break;
	}
	// 断られた操作（電源OFF中の番号など）では連打防止を始めない。
	if (bHandled)
	{
		LastPressTime = Now;
		UTBAudioEvents::Emit(this, ETBSoundEvent::TVButton, nullptr, FVector::ZeroVector);
	}
	return bHandled;
}
