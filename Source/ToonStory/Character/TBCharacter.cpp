// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBCharacter.h"
#include "TBHumanPose.h"
#include "TBToyCharacter.h"
#include "TBToyCarVisual.h"
#include "TBToyPresentation.h"
#include "GamePlay/TBRoomLighting.h"
#include "Audio/TBAudioEvents.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Core/TBController.h"
#include "Core/TBGameUserSettings.h"
#include "Input/TBInputSettings.h"
#include "Input/TBKeyLabels.h"
#include "Input/TBKeyRemap.h"
#include "UI/TBHUD.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Character/TBMovement.h"
#include "Character/TBVoiceComponent.h"
#include "Audio/TBCharacterAudioComponent.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBDummyToy.h"
#include "GamePlay/TBPickup.h"
#include "GamePlay/Items/TBInteractable.h"
#include "GamePlay/Items/TBInteractionTarget.h"
#include "GamePlay/Items/TBWalkieTalkie.h"
#include "GamePlay/TBRuleMath.h"
#include "Data/TBCharacterData.h"
#include "Data/TBRuleData.h"
#include "Core/TBGameHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Components/InputComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATBCharacter::ATBCharacter(const FObjectInitializer& Init)
    : Super(Init.SetDefaultSubobjectClass<UTBMovement>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30);
	SetMinNetUpdateFrequency(15);
	GetCapsuleComponent()->InitCapsuleSize(26, 65);
	HumanVisual = CreateDefaultSubobject<UChildActorComponent>(TEXT("HumanVisual"));
	HumanVisual->SetupAttachment(GetMesh());
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// 運搬カメラの壁よけ判定で、人間自身に当たってアームが縮まないようにする。
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	InitializeComfortMovement();
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
	Camera->bUsePawnControlRotation = true;
	Camera->SetRelativeLocation(FVector(0, 0, 64));
	CarryCameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CarryCameraArm"));
	CarryCameraArm->SetupAttachment(GetRootComponent());
	// 位置は人間に追従し、回転は所有者であるおもちゃの視点を使う。
	CarryCameraArm->SetUsingAbsoluteRotation(true);
	CarryCameraArm->SetUsingAbsoluteScale(true);
	CarryCameraArm->bUsePawnControlRotation = true;
	// 人間のメッシュ側のネット補間を使い、注視点だけが遅れて中心からずれるのを防ぐ。
	CarryCameraArm->bEnableCameraLag = false;
	CarryCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CarryCamera"));
	CarryCamera->SetupAttachment(CarryCameraArm, USpringArmComponent::SocketName);
	CarryCamera->SetAutoActivate(false);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	// 表示する身体も運搬カメラと同じ補間に追従させる。
	Body->SetupAttachment(GetMesh());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetOwnerNoSee(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
	}
	Body->SetRelativeScale3D(FVector(.55, .55, 1.6));
	CarryAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("CarryAnchor"));
	// ルートはネット更新で飛び飛びに動くので、補間されるメッシュに付ける。
	CarryAnchor->SetupAttachment(GetMesh());
	CarryAnchor->SetUsingAbsoluteScale(true);
	CarryAnchor->SetUsingAbsoluteRotation(true);
	CarryAnchor->SetRelativeLocation(FVector(95, 0, 10));
	Voice = CreateDefaultSubobject<UTBVoiceComponent>(TEXT("Voice"));
	CreateDefaultSubobject<UTBCharacterAudioComponent>(TEXT("GameplayAudio"));
}

void ATBCharacter::BeginPlay()
{
	Super::BeginPlay();
	LoadedChildMesh = ChildMesh.LoadSynchronous();
	LoadedChildAnimation = ChildAnimation.LoadSynchronous();
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 視界判定と同じ視野角・基準の画面比で映す。運ばれている間も同じにして、切り替えのたびに視野角が伸び縮みしないようにする。
	// 縦の視野は基準の画面比で決まり、狭い画面では横だけ狭く映る（判定も同じ範囲）。
	const auto& Vision = TB::Rules().Vision;
	Camera->FieldOfView = Vision.FieldOfView;
	Camera->AspectRatio = Vision.AspectRatio();
	CarryCamera->FieldOfView = Camera->FieldOfView;
	CarryCamera->AspectRatio = Camera->AspectRatio;
	const auto& CarryView = TB::CharacterData().CarryCamera;
	CarryCameraArm->TargetArmLength = CarryView.ArmLength;
	CarryCameraArm->TargetOffset = FVector(0, 0, CarryView.LookHeight);
	bAppearanceInitialized = false;
	// PlayerState が届くまで ApplyState は何もしないので、その間もエンジンの初期値ではなく人間の速さで動かす。
	ApplyComfortMovement(false);
	ApplyState();
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		AddTickPrerequisiteActor(GameMode);
	}
}

void ATBCharacter::UpdateAppearance(bool bToy)
{
	if (bAppearanceInitialized && bShowingToy == bToy)
	{
		return;
	}
	// Possession can apply appearance before BeginPlay; retain assets on that path too.
	if (!bToy)
	{
		if (!LoadedChildMesh) LoadedChildMesh = ChildMesh.LoadSynchronous();
		if (!LoadedChildAnimation) LoadedChildAnimation = ChildAnimation.LoadSynchronous();
	}
	bAppearanceInitialized = true;
	bShowingToy = bToy;
	GetMesh()->SetVisibility(false);
	Body->SetVisibility(true);
	if (!bToy && LoadedChildMesh)
	{
		Body->SetVisibility(false);
		auto* Visual = GetMesh();
		Visual->SetSkeletalMesh(LoadedChildMesh);
		const FBoxSphereBounds Bounds = LoadedChildMesh->GetBounds();
		const float Scale = 125.f / FMath::Max(1.f, float(Bounds.BoxExtent.Z * 2));
		Visual->SetRelativeScale3D(FVector(Scale));
		// 速度の切り替えはABPに任せ、待機も含めた一律の早回しを避ける。
		Visual->GlobalAnimRateScale = 1.f;
		const float FloorClearance =
		    (UCharacterMovementComponent::MIN_FLOOR_DIST + UCharacterMovementComponent::MAX_FLOOR_DIST) * .5f;
		const float MeshBottom = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
		Visual->SetRelativeLocation(FVector(0, 0, -65 - FloorClearance - MeshBottom));
		// リターゲット後の子どもの姿勢はメッシュの+Yが正面。Pawnの+X（視点のYaw）へ合わせる。
		Visual->SetRelativeRotation(FRotator(0, -90, 0));
		// 他プレイヤー側の移動補間は記録済みのメッシュ位置へ毎フレーム戻すため、記録も更新しないと足が浮く。
		StandingMeshOffset = Visual->GetRelativeLocation();
		CacheInitialMeshOffset(StandingMeshOffset, Visual->GetRelativeRotation());
		if (LoadedChildAnimation)
		{
			Visual->SetAnimInstanceClass(LoadedChildAnimation);
			Visual->SetOverridePostProcessAnimBP(LoadClass<UAnimInstance>(
			    nullptr, TEXT("/Game/Characters/ChildAnimation/ABP_TBHumanPose.ABP_TBHumanPose_C")));
		}
		Visual->SetVisibility(true);
	}
	else
	{
		Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Body->SetRelativeLocation(FVector::ZeroVector);
		Body->SetRelativeScale3D(bToy ? FVector(.18, .18, .25) : FVector(.45, .45, 1.2));
	}
	// メッシュの縮尺・正面補正に影響されず、従来のPawn基準の運搬位置を保つ。
	const FTransform MeshTransform = GetMesh()->GetRelativeTransform();
	CarryAnchor->SetRelativeLocation(MeshTransform.InverseTransformPosition(FVector(95, 0, 10)));
	CarryAnchor->SetRelativeRotation(MeshTransform.GetRotation().Inverse());
	if (!bToy && GetMesh()->DoesSocketExist(TEXT("hand_r")))
	{
		CarryAnchor->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		                               TEXT("hand_r"));
		CarryAnchor->SetRelativeLocation(FVector::ZeroVector);
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

ATBPlayerState* ATBCharacter::TBPS() const
{
	return GetPlayerState<ATBPlayerState>();
}

bool ATBCharacter::IsHuman() const
{
	return TBPS() && TBPS()->Team == ETBTeam::Human;
}

bool ATBCharacter::GetHumanLook(FTBHumanLookSnapshot& Out) const
{
	Out = FTBHumanLookSnapshot();
	const auto* State = TB::GS(GetWorld());
	auto* Player = TBPS();
	const double ServerNow = State ? State->GetServerWorldTimeSeconds() : 0;
	if (!IsValid(Player) || Player->Team != ETBTeam::Human || !State || State->Phase != ETBPhase::Playing ||
	    State->IsPreparing() || !FMath::IsFinite(ServerNow) || !FMath::IsFinite(State->EndTime) ||
	    State->EndTime <= 0 || State->EndTime <= ServerNow) return false;
	const auto* OwnerController = GetController();
	if (HasAuthority())
	{
		if (!IsValid(OwnerController) || OwnerController->GetPawn() != this ||
		    OwnerController->GetPlayerState<ATBPlayerState>() != Player || HumanLookPossession == 0) return false;
		Out.bValid = true; Out.Player = Player; Out.Round = State->HidePreparation.Round; Out.Possession = HumanLookPossession;
		Out.View = OwnerController->GetControlRotation();
	}
	else
	{
		if (bHumanLookRejected || !HumanLook.bValid || !IsValid(HumanLook.Player) || HumanLook.Player != Player ||
		    HumanLook.Possession != ObservedHumanLookPossession ||
		    HumanLook.Round != State->HidePreparation.Round || HumanLook.Possession == 0) return false;
		Out = HumanLook;
		if (IsLocallyControlled())
		{
			if (!IsValid(OwnerController) || OwnerController->GetPawn() != this ||
			    OwnerController->GetPlayerState<ATBPlayerState>() != Player) { Out = FTBHumanLookSnapshot(); return false; }
			Out.View = OwnerController->GetControlRotation();
		}
	}
	if (Out.View.ContainsNaN()) { Out = FTBHumanLookSnapshot(); return false; }
	Out.View = FRotator(FRotator::NormalizeAxis(Out.View.Pitch), FRotator::NormalizeAxis(Out.View.Yaw), 0);
	return true;
}

void ATBCharacter::UpdateHumanLook()
{
	if (!HasAuthority()) return;
	FTBHumanLookSnapshot Next;
	GetHumanLook(Next);
	if (HumanLook.bValid && (!Next.bValid || HumanLook.Player != Next.Player || HumanLook.Round != Next.Round))
		InvalidateHumanLook();
	// A normal replicated property at the existing actor update frequency; no per-frame RPC.
	Next.Possession = HumanLookPossession;
	HumanLook = Next;
}

void ATBCharacter::OnRep_HumanLook()
{
	const uint32 Incoming = HumanLook.Possession;
	// Serial comparison also handles wrap-around. An old generation cannot revive an invalidated pose.
	bHumanLookRejected = Incoming == 0 || (ObservedHumanLookPossession != 0 && Incoming != ObservedHumanLookPossession &&
	    uint32(Incoming - ObservedHumanLookPossession) > uint32(MAX_int32));
	if (!bHumanLookRejected) ObservedHumanLookPossession = Incoming;
}

void ATBCharacter::InvalidateHumanLook()
{
	if (!HasAuthority()) return;
	CancelEmote();
	if (++HumanLookPossession == 0) ++HumanLookPossession;
	HumanLook = FTBHumanLookSnapshot();
	HumanLook.Possession = HumanLookPossession;
	ForceNetUpdate();
}

namespace
{
	bool NewerEmoteRevision(uint32 A, uint32 B)
	{
		return A != 0 && (B == 0 || (A != B && uint32(A - B) <= uint32(MAX_int32)));
	}
}

double ATBCharacter::EmoteDuration(ETBEmoteKind Kind)
{
	switch (Kind)
	{
	case ETBEmoteKind::Wave: return 1.6;
	case ETBEmoteKind::Cheer: return 2.;
	case ETBEmoteKind::Dance: return 3.;
	case ETBEmoteKind::WildDance: return 6.;
	default: return 0;
	}
}

int32 ATBCharacter::EmoteVisualIndex() const
{
	const auto* Toy = Cast<ATBToyCharacter>(this);
	return TBPS() && TBPS()->Team == ETBTeam::Toy && Toy ? Toy->SelectedToyIndex : INDEX_NONE;
}

bool ATBCharacter::SupportsEmotes() const
{
	if (TBPS() && TBPS()->Team != ETBTeam::Toy) return TBToyPresentation::SupportsEmoteMesh(GetMesh()->GetSkeletalMeshAsset());
	const auto* Toy = Cast<ATBToyCharacter>(this);
	if (!Toy || !Toy->ToyCatalogue.IsValidIndex(Toy->SelectedToyIndex) ||
	    !TBToyPresentation::SupportsEmoteClass(Toy->ToyCatalogue[Toy->SelectedToyIndex].Get())) return false;
	if (GetNetMode() == NM_DedicatedServer) return true;
	auto* Visual = Toy->ToyVisual->GetChildActor();
	if (!IsValid(Visual)) return false;
	if (Visual->IsA<ATBToyCarVisual>()) return true;
	TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual);
	for (auto* Part : Parts)
		if (!Part->LeaderPoseComponent.IsValid() && TBToyPresentation::SupportsEmoteMesh(Part->GetSkeletalMeshAsset())) return true;
	return false;
}

bool ATBCharacter::EmoteGameplayEligible() const
{
	const auto* State = TB::GS(GetWorld());
	const auto* Movement = GetCharacterMovement();
	const auto* Player = TBPS();
	const auto* OwnerController = GetController();
	const double Now = State ? State->GetServerWorldTimeSeconds() : 0;
	const bool Lobby = State && State->Phase == ETBPhase::Lobby && !State->bTravelPending;
	const bool Playing = State && CanAct() && FMath::IsFinite(State->EndTime) && State->EndTime > Now;
	if (!State || !IsValid(Player) || (!Lobby && !Playing) || IsMovementLocked() || IsActorBeingDestroyed() ||
	    !State->PlayerArray.Contains(Player) || !FMath::IsFinite(Now) ||
	    !Movement || bIsCrouched || Movement->bWantsToCrouch || !Movement->IsMovingOnGround() || GetVelocity().ContainsNaN() || GetVelocity().Size2D() > 2.f ||
	    Carrier || HasCarriedToy() || bHoldingInteract || bRescuing || StoreStarted >= 0) return false;
	if ((HasAuthority() || IsLocallyControlled()) && (!IsValid(OwnerController) || OwnerController->GetPawn() != this ||
	    OwnerController->GetPlayerState<ATBPlayerState>() != Player)) return false;
	return true;
}

bool ATBCharacter::EmoteEligible() const
{
	return EmoteGameplayEligible() && SupportsEmotes();
}

bool ATBCharacter::CanRequestEmote() const
{
	if (!IsLocallyControlled() || !EmoteEligible()) return false;
	const auto* State = TB::GS(GetWorld());
	return !Emote.bActive && FMath::IsFinite(Emote.NextAllowedAt) && State->GetServerWorldTimeSeconds() >= Emote.NextAllowedAt &&
	    (HasAuthority() || (Emote.Player == TBPS() && Emote.Round == State->HidePreparation.Round &&
	      Emote.Possession != 0 && Emote.Possession == HumanLook.Possession && !bEmoteRejected));
}

bool ATBCharacter::RequestEmote(ETBEmoteKind Kind)
{
	if (EmoteDuration(Kind) <= 0 || !CanRequestEmote()) return false;
	if (HasAuthority()) return TryEmote(Kind);
	ServerEmote(Kind, Emote.Round, Emote.Possession);
	return true; // Request delivery is not a claim of server acceptance.
}

void ATBCharacter::ServerEmote_Implementation(ETBEmoteKind Kind, uint32 ExpectedRound, uint32 ExpectedPossession)
{
	const auto* State = TB::GS(GetWorld());
	if (!State || ExpectedRound != State->HidePreparation.Round || ExpectedPossession != HumanLookPossession || !TryEmote(Kind))
		if (auto* PC = Cast<ATBController>(GetController())) PC->ClientNotice(TEXT("今はエモートできません。安全な場所で止まってから選んでください。"));
}

bool ATBCharacter::TryEmote(ETBEmoteKind Kind)
{
	if (!HasAuthority()) return false;
	UpdateEmote(); // Cancellation always precedes a new request in the same frame.
	const auto* State = TB::GS(GetWorld());
	const double Duration = EmoteDuration(Kind);
	if (Duration <= 0 || !EmoteEligible() || Emote.bActive || LastEmoteCancelFrame == GFrameCounter ||
	    State->GetServerWorldTimeSeconds() < Emote.NextAllowedAt) return false;
	Emote.bActive = true; Emote.Kind = Kind;
	if (++Emote.Revision == 0) ++Emote.Revision;
	Emote.Player = TBPS(); Emote.Team = TBPS()->Team; Emote.Round = State->HidePreparation.Round;
	Emote.Phase = State->Phase;
	Emote.Possession = HumanLookPossession; Emote.Visual = EmoteVisualIndex();
	Emote.StartsAt = State->GetServerWorldTimeSeconds(); Emote.EndsAt = Emote.StartsAt + Duration;
	Emote.NextAllowedAt = Emote.StartsAt + 4.;
	ForceNetUpdate(); return true;
}

void ATBCharacter::CancelEmote()
{
	if (!HasAuthority() || !Emote.bActive) return;
	Emote.bActive = false; Emote.Kind = ETBEmoteKind::None;
	if (++Emote.Revision == 0) ++Emote.Revision;
	LastEmoteCancelFrame = GFrameCounter;
	ForceNetUpdate();
}

void ATBCharacter::UpdateEmote()
{
	if (!HasAuthority())
	{
		// GT safety observation is independent of visibility, animation evaluation and bPauseAnims.
		FTBEmoteSample Ignored;
		GetEmoteSample(Ignored);
		return;
	}
	const auto* State = TB::GS(GetWorld());
	const uint32 Round = State ? State->HidePreparation.Round : 0;
	const ETBTeam Team = TBPS() ? TBPS()->Team : ETBTeam::None;
	const ETBPhase Phase = State ? State->Phase : ETBPhase::Lobby;
	const bool Changed = Emote.Player != TBPS() || Emote.Team != Team || Emote.Round != Round ||
	    Emote.Phase != Phase || Emote.Possession != HumanLookPossession || Emote.Visual != EmoteVisualIndex();
	if (Emote.bActive && (Changed || !EmoteEligible() || !State || State->GetServerWorldTimeSeconds() >= Emote.EndsAt)) CancelEmote();
	if (Changed)
	{
		if (++Emote.Revision == 0) ++Emote.Revision;
		Emote.Player = TBPS(); Emote.Team = Team; Emote.Round = Round; Emote.Possession = HumanLookPossession;
		Emote.Phase = Phase;
		Emote.Visual = EmoteVisualIndex(); Emote.NextAllowedAt = 0;
		ForceNetUpdate();
	}
}

void ATBCharacter::OnRep_Emote()
{
	bEmoteRejected = Emote.Revision == 0 || (Emote.Revision != ObservedEmoteRevision && !NewerEmoteRevision(Emote.Revision, ObservedEmoteRevision));
	if (!bEmoteRejected) ObservedEmoteRevision = Emote.Revision;
	if (!Emote.bActive && NewerEmoteRevision(Emote.Revision, BlockedEmoteRevision)) BlockedEmoteRevision = Emote.Revision;
}

bool ATBCharacter::GetEmoteSample(FTBEmoteSample& Out) const
{
	Out = FTBEmoteSample();
	if (!Emote.bActive) return false;
	const auto* State = TB::GS(GetWorld());
	const double Now = State ? State->GetServerWorldTimeSeconds() : 0;
	if (bEmoteRejected || (!HasAuthority() && Emote.Revision != ObservedEmoteRevision) || !NewerEmoteRevision(Emote.Revision, BlockedEmoteRevision)) return false;
	const bool Ready = State && IsValid(TBPS()) && State->PlayerArray.Contains(TBPS()) && IsValid(Emote.Player) && Emote.Player == TBPS() &&
	    Emote.Team == TBPS()->Team && Emote.Round == State->HidePreparation.Round &&
	    Emote.Possession != 0 && (HasAuthority() ? Emote.Possession == HumanLookPossession : Emote.Possession == HumanLook.Possession) &&
	    Emote.Phase == State->Phase && !State->IsPreparing() &&
	    (State->Phase == ETBPhase::Playing || (State->Phase == ETBPhase::Lobby && !State->bTravelPending));
	if (!Ready)
	{
		// Actor/PS/round properties may arrive in either order. Wait before first presentation,
		// but an already presented revision must never return after a local interruption.
		if (PresentedEmoteRevision == Emote.Revision) BlockedEmoteRevision = Emote.Revision;
		return false;
	}
	if (!FMath::IsFinite(Now) || !FMath::IsFinite(Emote.StartsAt) || !FMath::IsFinite(Emote.EndsAt) ||
	    Emote.EndsAt <= Emote.StartsAt || EmoteDuration(Emote.Kind) <= 0 || Now >= Emote.EndsAt)
	{
		if (NewerEmoteRevision(Emote.Revision, BlockedEmoteRevision)) BlockedEmoteRevision = Emote.Revision;
		return false;
	}
	if (Now < Emote.StartsAt) return false; // Clock smoothing is not a cancellation.
	if (!EmoteGameplayEligible())
	{
		// Movement/role replication may trail the first emote payload. Once presented,
		// a local interruption remains final even if eligibility later recovers.
		if (PresentedEmoteRevision == Emote.Revision) BlockedEmoteRevision = Emote.Revision;
		return false;
	}
	if (Emote.Visual != EmoteVisualIndex() || !SupportsEmotes())
	{
		// A first relevance snapshot can precede child-actor/mesh creation. Do not poison it.
		if (PresentedEmoteRevision == Emote.Revision) BlockedEmoteRevision = Emote.Revision;
		return false;
	}
	PresentedEmoteRevision = Emote.Revision;
	Out.Kind = Emote.Kind; Out.Progress = float((Now - Emote.StartsAt) / (Emote.EndsAt - Emote.StartsAt));
	Out.Weight = FMath::SmoothStep(0.f, .15f, Out.Progress) * (1.f - FMath::SmoothStep(.85f, 1.f, Out.Progress));
	return true;
}

void ATBCharacter::OnEmoteVisualEndPlay(AActor* Visual, EEndPlayReason::Type Reason)
{
	TBToyPresentation::RestoreEmoteVisual(Visual);
}

void ATBCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelEmote();
	TBToyPresentation::RestoreEmoteOwner(this);
	Super::EndPlay(EndPlayReason);
}

bool ATBCharacter::CanAct() const
{
	const auto* ToyPlayerInfo = TBPS();
	const auto* State = TB::GS(GetWorld());
	return TB::Playing(GetWorld()) && State && !State->IsPreparing() && ToyPlayerInfo && !ToyPlayerInfo->bFrozen &&
	       (ToyPlayerInfo->Team == ETBTeam::Human ||
	        (ToyPlayerInfo->Team == ETBTeam::Toy && ToyPlayerInfo->ToyState == ETBToyState::Free));
}

bool ATBCharacter::IsCelebrating() const
{
	const auto* State = TB::GS(GetWorld());
	const auto* Info = TBPS();
	return State && Info && State->Phase == ETBPhase::Results &&
	    ((State->Winner == ETBWinner::Humans && Info->Team == ETBTeam::Human) ||
	     (State->Winner == ETBWinner::Toys && Info->Team == ETBTeam::Toy));
}

bool ATBCharacter::IsGazeFrozen() const
{
	return !IsCelebrating() && TBPS() && TBPS()->bFrozen;
}

bool ATBCharacter::IsPhysicsLocked() const
{
	const auto* MatchInfo = TB::GS(GetWorld());
	const auto* ToyPlayerInfo = TBPS();
	if (MatchInfo && MatchInfo->Phase == ETBPhase::Lobby && MatchInfo->bTravelPending) return true;
	if (MatchInfo && MatchInfo->IsPreparing() && (!ToyPlayerInfo || ToyPlayerInfo->Team != ETBTeam::Toy)) return true;
	if (MatchInfo && (MatchInfo->Phase == ETBPhase::Results || MatchInfo->Phase == ETBPhase::Aborted))
	{
		return !IsCelebrating();
	}
	return ToyPlayerInfo && ToyPlayerInfo->Team == ETBTeam::Toy &&
	       (ToyPlayerInfo->ToyState == ETBToyState::Grabbed || ToyPlayerInfo->ToyState == ETBToyState::Carried ||
	        ToyPlayerInfo->ToyState == ETBToyState::Storing);
}

bool ATBCharacter::IsMovementLocked() const
{
	return IsPhysicsLocked() || IsGazeFrozen();
}

// 同期された状態から、体格・衝突・移動モードをそろえる。
void ATBCharacter::ApplyState()
{
	const auto* ToyPlayerInfo = TBPS();
	if (!ToyPlayerInfo)
	{
		return;
	}
	const bool Toy = ToyPlayerInfo->Team == ETBTeam::Toy;
	// ToyPawnはConfig/DefaultEngine.iniで定義する専用オブジェクトチャンネル。
	GetCapsuleComponent()->SetCollisionObjectType(Toy ? ECC_GameTraceChannel1 : ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, Toy ? ECR_Block : ECR_Ignore);
	ApplyComfortBody(Toy);
	UpdateAppearance(Toy);
	UClass* Look = !Toy && GetNetMode() != NM_DedicatedServer ? ToyPlayerInfo->HumanAppearance.LoadSynchronous() : nullptr;
	if (HumanVisual->GetChildActorClass() != Look)
	{
		HumanVisual->SetChildActorClass(Look);
		if (AActor* Assembly = HumanVisual->GetChildActor())
		{
			Assembly->SetOwner(this);
			Assembly->SetActorEnableCollision(false);
			TInlineComponentArray<UPrimitiveComponent*> Parts(Assembly);
			for (auto* Part : Parts)
			{
				// MetaHuman's Hair component inherits the face-only bounds by default.
				// Use its own groom bounds so hair outside the face is not culled at view edges.
				if (Part->GetFName() == TEXT("Hair"))
				{
					Part->bUseAttachParentBound = false;
					Part->SetBoundsScale(1.2f);
					Part->UpdateBounds();
				}
				auto* Skinned = Cast<USkeletalMeshComponent>(Part);
				if (Skinned && Skinned->GetSkeletalMeshAsset() && LoadedChildMesh && Skinned->GetName().Equals(TEXT("Body"), ESearchCase::IgnoreCase))
				{
					// Clothing follows this hidden/occluded body. Its bones must update even
					// when only the garment (or a TV capture) is visible.
					Skinned->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					Skinned->AddTickPrerequisiteComponent(GetMesh());
					Skinned->SetAnimInstanceClass(UTBHumanBodyPose::StaticClass());
					// Body meshes are modular and may omit the head. Size the entire assembly by its skeleton.
					const auto& Ref = Skinned->GetSkeletalMeshAsset()->GetRefSkeleton();
					auto RestPosition = [&Ref](int32 Bone)
					{
						FTransform Rest = FTransform::Identity;
						for (; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone))
							Rest *= Ref.GetRefBonePose()[Bone];
						return Rest.GetLocation();
					};
					const int32 Head = Ref.FindBoneIndex(TEXT("head")), Foot = Ref.FindBoneIndex(TEXT("foot_l"));
					if (Head != INDEX_NONE && Foot != INDEX_NONE)
					{
						HumanVisual->SetRelativeScale3D(FVector::OneVector);
						const FVector Span = Skinned->GetComponentTransform().TransformVector(RestPosition(Head) - RestPosition(Foot));
						if (Span.Z > 1.) HumanVisual->SetRelativeScale3D(FVector(106. / Span.Z));
					}
				}
			}
		}
	}
	if (auto* Assembly = HumanVisual->GetChildActor())
		for (auto* Part : TInlineComponentArray<UPrimitiveComponent*>(Assembly))
			Part->SetOwnerNoSee(IsHuman() && !IsCelebrating());
	if (!Toy && LoadedChildMesh) GetMesh()->SetVisibility(!HumanVisual->GetChildActor());
	const bool Held = ToyPlayerInfo->ToyState == ETBToyState::Grabbed ||
	                  ToyPlayerInfo->ToyState == ETBToyState::Carried ||
	                  ToyPlayerInfo->ToyState == ETBToyState::Storing;
	GetCapsuleComponent()->SetCollisionEnabled(Held ? ECollisionEnabled::NoCollision
	                                                : ECollisionEnabled::QueryAndPhysics);
	const bool PhysicsLocked = IsPhysicsLocked();
	auto* Movement = GetCharacterMovement();

	const auto& Feel = TB::CharacterData();
	Movement->MaxStepHeight = Toy ? Feel.Toy.MaxStepHeight : Feel.Human.MaxStepHeight;
	Movement->JumpZVelocity = Toy ? Feel.Toy.JumpVelocity : Feel.Human.JumpVelocity;
	if (PhysicsLocked)
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		StopJumping();
	}
	else if (IsGazeFrozen())
	{
		StopJumping();
		Movement->Velocity.X = 0;
		Movement->Velocity.Y = 0;
		Movement->Velocity.Z = TBRuleMath::FrozenVerticalSpeed(Movement->Velocity.Z);
		if (Movement->MovementMode != MOVE_Custom || Movement->CustomMovementMode != 1)
		{
			Movement->SetMovementMode(MOVE_Custom, 1);
		}
	}
	else if (bWasPhysicsLocked || Movement->MovementMode == MOVE_None ||
	         (Movement->MovementMode == MOVE_Custom && Movement->CustomMovementMode == 1))
	{
		Movement->SetMovementMode(MOVE_Falling);
	}
	ApplyComfortMovement(Toy);
	bWasPhysicsLocked = PhysicsLocked;
}

void ATBCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyState();
}

void ATBCharacter::PossessedBy(AController* PlayerController)
{
	InvalidateHumanLook();
	Super::PossessedBy(PlayerController);
	ApplyState();
}

void ATBCharacter::UnPossessed()
{
	InvalidateHumanLook();
	Super::UnPossessed();
}

bool ATBCharacter::CanBeFrozenBy(const ATBCharacter* Observer) const
{
	return Observer && !ATBRoomLighting::ProtectsFromGaze(GetWorld(), GetActorLocation(), Observer->GetActorLocation());
}

void ATBCharacter::SetFrozen(bool bFrozen)
{
	if (!HasAuthority() || !TBPS() || TBPS()->bFrozen == bFrozen)
	{
		return;
	}
	TBPS()->bFrozen = bFrozen;
	TBPS()->ForceNetUpdate();
	ApplyState();
	StateVisualChanged();
	if (bFrozen)
	{
		ClearHeldInteraction();
	}
	if (TBPS()->ToyState == ETBToyState::Free && !Carrier)
	{
		UTBAudioEvents::Emit(this, bFrozen ? ETBSoundEvent::Frozen : ETBSoundEvent::Thawed,
		                    TBPS(), GetActorLocation());
	}
}

void ATBCharacter::SetToyState(ETBToyState NewState)
{
	if (!HasAuthority() || !TBPS())
	{
		return;
	}
	TBPS()->ToyState = NewState;
	TBPS()->ForceNetUpdate();
	ApplyState();
	StateVisualChanged();
	ForceNetUpdate();
}

void ATBCharacter::OnRep_Carrier()
{
	if (GripTickCarrier.IsValid())
	{
		RemoveTickPrerequisiteComponent(GripTickCarrier->GetMesh());
	}
	GripTickCarrier = Carrier;
	if (Carrier)
	{
		AddTickPrerequisiteComponent(Carrier->GetMesh());
	}
	if (Carrier)
	{
		AttachToComponent(Carrier->CarryAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(FVector(0, 0, -10));
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	ApplyState();
}

// 降ろす・収納はサーバーでCarrierを直接書くため、OnRepではなく毎フレーム差分を見る。
void ATBCharacter::UpdateCarryCamera()
{
	if (!IsLocallyControlled())
	{
		return;
	}
	ATBCharacter* NewCarrier = IsValid(Carrier) ? Carrier.Get() : nullptr;
	const bool bCarried = NewCarrier != nullptr;
	if (bCarried == bCarryView && NewCarrier == CarryViewCarrier.Get())
	{
		return;
	}
	// 掴まれた・降ろされた・運ぶ人が替わった、のいずれも直前の視点からつなぐ。
	const auto& CarryView = TB::CharacterData().CarryCamera;
	CarryBlendDuration = bCarried ? CarryView.BlendTime : CarryView.ReleaseBlendTime;
	CarryBlendFrom = LastView;
	CarryBlendStarted = bHasLastView && CarryBlendDuration > 0 ? GetWorld()->GetTimeSeconds() : -1;
	AController* PlayerController = GetController();
	if (bCarried)
	{
		// 運ぶ人が入れ替わった場合も付け直す。視点はそのまま引き継ぐ。
		CarryCameraArm->AttachToComponent(NewCarrier->GetMesh(),
		                                  FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		// 掴まれる前に見ていた方角を保ち、以降はマウス左右だけで回り込む。
		if (!bCarryView && PlayerController)
		{
			PlayerController->SetControlRotation(
			    FRotator(CarryView.ViewPitch, PlayerController->GetControlRotation().Yaw, 0));
		}
	}
	else
	{
		// 見ていた方向のまま、水平に戻して一人称へ。
		if (PlayerController)
		{
			PlayerController->SetControlRotation(FRotator(0, PlayerController->GetControlRotation().Yaw, 0));
		}
		CarryCameraArm->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	CarryViewCarrier = NewCarrier;
	if (bCarried == bCarryView)
	{
		return;
	}
	bCarryView = bCarried;
	Camera->SetActive(!bCarried);
	CarryCamera->SetActive(bCarried);
	// 抱えられている自分も画面に入れる。
	Body->SetOwnerNoSee(!bCarried);
	GetMesh()->SetOwnerNoSee(!bCarried);
	UpdateAppearance(TBPS() && TBPS()->Team == ETBTeam::Toy);
}

// カメラは先に切り替える。寄せる先は毎フレームの最新値なので、運ぶ人の移動や視点操作があっても最後に飛ばない。
void ATBCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	if (IsCelebrating())
	{
		const FVector Focus = GetActorLocation() + FVector(0, 0, BaseEyeHeight * .5f);
		const FRotator View = GetBaseAimRotation();
		const FVector Desired = Focus - View.Vector() * (IsHuman() ? 240.f : 130.f);
		FCollisionQueryParams Query(SCENE_QUERY_STAT(TBCelebrationCamera), false, this);
		if (HumanVisual->GetChildActor()) Query.AddIgnoredActor(HumanVisual->GetChildActor());
		FHitResult Hit;
		GetWorld()->SweepSingleByChannel(Hit, Focus, Desired, FQuat::Identity, ECC_Camera,
		    FCollisionShape::MakeSphere(8.f), Query);
		OutResult.Location = Hit.bBlockingHit ? Hit.Location : Desired;
		OutResult.Rotation = View;
		return;
	}
	// カメラの画面比（BeginPlay で基準の画面比にしてある）より横長の画面は、左右に黒帯を出して見えすぎを防ぐ。
	// 1366×768 など、ほぼ同じ比率の画面に細い黒帯が出ないよう、1% までの差は許す。
	const auto* Player = Cast<ATBController>(GetController());
	if (Player && Player->ViewAspect > OutResult.AspectRatio * 1.01f)
	{
		OutResult.bConstrainAspectRatio = true;
	}
	// 箱への収納など瞬間移動を伴う切替はつながず、壁越しに視点が流れるのを防ぐ。降ろす位置は1m程度。
	const FVector ActorLocation = GetActorLocation();
	const bool bTeleported =
	    bHasLastView && FVector::DistSquared(ActorLocation, LastActorLocation) > FMath::Square(300.f);
	if (bTeleported)
	{
		CarryBlendStarted = -1;
	}
	// 段差の上り下りで目線が一気に跳ねて酔わないよう、速度では説明できない高さの変化（段差でのワープ）だけ少し遅らせる。
	// 高い段差は一瞬空中扱いを挟むので、地面にいるかでは判定しない。ジャンプや落下は速度どおりなので遅らせない。
	// 運ばれている間と降ろされた直後（手元から落下位置へのワープ）は比べない。しゃがみの切替で体が上下した分も数えない。
	const bool bCompareStep = bHasLastView && !bTeleported && !Carrier && !bLastViewCarried;
	const float StepZ = bCompareStep && bIsCrouched == bLastViewCrouched
	                        ? ActorLocation.Z - LastActorLocation.Z - GetVelocity().Z * DeltaTime
	                        : 0.f;
	const auto& Feel = TB::CharacterData();
	StepOffset = bCompareStep ? TBRuleMath::SmoothStepOffset(StepOffset, StepZ, DeltaTime, Feel.View.StepSmoothSpeed,
	                                                         Feel.View.StepSmoothMax)
	                          : 0.f;
	OutResult.Location.Z += StepOffset;
	LastActorLocation = ActorLocation;
	bLastViewCarried = Carrier != nullptr;
	bLastViewCrouched = bIsCrouched;
	const double Elapsed = GetWorld()->GetTimeSeconds() - CarryBlendStarted;
	if (CarryBlendStarted >= 0 && Elapsed < CarryBlendDuration)
	{
		const float Alpha = UKismetMathLibrary::Ease(0.f, 1.f, Elapsed / CarryBlendDuration, Feel.CarryCamera.BlendEase,
		                                             Feel.CarryCamera.BlendExp);
		OutResult.Location = FMath::Lerp(CarryBlendFrom.Location, OutResult.Location, Alpha);
		OutResult.Rotation =
		    FQuat::Slerp(CarryBlendFrom.Rotation.Quaternion(), OutResult.Rotation.Quaternion(), Alpha).Rotator();
		OutResult.FOV = FMath::Lerp(CarryBlendFrom.FOV, OutResult.FOV, Alpha);
	}
	LastView = OutResult;
	bHasLastView = true;
}

void ATBCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBCharacter, Carrier);
	DOREPLIFETIME(ATBCharacter, CarriedToy);
	DOREPLIFETIME(ATBCharacter, CarriedDummy);
	DOREPLIFETIME(ATBCharacter, HumanLook);
	DOREPLIFETIME(ATBCharacter, Emote);
	DOREPLIFETIME_CONDITION(ATBCharacter, ContactItem, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATBCharacter, ItemProgress, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATBCharacter, StoreStarted, COND_OwnerOnly);
}

// 進捗と捕獲状態の確定はサーバー側だけで進める。
void ATBCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHumanLook();
	UpdateEmote();
	ApplyState();
	UpdateCarryCamera();
	if (IsCelebrating())
	{
		Body->SetOwnerNoSee(false);
		GetMesh()->SetOwnerNoSee(false);
	}
	if (IsHuman())
	{
		CarryAnchor->SetWorldRotation(GetActorRotation());
	}

	if (!HasAuthority())
	{
		return;
	}
	auto* ToyPlayerInfo = TBPS();
	if (!ToyPlayerInfo)
	{
		return;
	}
	UpdateItemContact(DeltaSeconds);
	if (!TB::Playing(GetWorld()))
	{
		ClearHeldInteraction();
		return;
	}
	if (!IsValid(CarriedToy))
	{
		UpdateCarriedDummy();
		return;
	}
	auto* ToyPlayerState = CarriedToy->TBPS();
	if (!ToyPlayerState)
	{
		DropToy();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (ToyPlayerState->ToyState == ETBToyState::Grabbed && Now >= GrabEnds)
	{
		CarriedToy->SetToyState(ETBToyState::Carried);
		UTBAudioEvents::Emit(this, ETBSoundEvent::CarryLift, TBPS(), FVector::ZeroVector);
		UTBAudioEvents::Emit(this, ETBSoundEvent::CarryLift, CarriedToy->TBPS(), FVector::ZeroVector);
	}
	if (ToyPlayerState->ToyState == ETBToyState::Storing)
	{
		auto* MatchInfo = TB::GS(GetWorld());
		auto* Box = MatchInfo ? MatchInfo->Box.Get() : nullptr;
		if (!bHoldingInteract || !Box || !Box->CanStoreFrom(this))
		{
			ClearHeldInteraction();
			return;
		}
		if (Now - StoreStarted >= TB::Rules().Box.StoreSeconds && Box->Store(CarriedToy))
		{
			CarriedToy = nullptr;
			ClearHeldInteraction();
			ApplyState();
		}
	}
}

void ATBCharacter::ClearHeldInteraction()
{
	if (HasAuthority())
		for (TActorIterator<ATBWalkieTalkie> It(GetWorld()); It; ++It) It->StopTransmitting(this);
	if (HasAuthority() && CarriedDummy && StoreStarted >= 0)
	{
		UTBAudioEvents::Emit(this, ETBSoundEvent::StoreCancel, TBPS(), FVector::ZeroVector);
	}
	bHoldingInteract = false;
	bRescuing = false;
	StoreStarted = -1;
	if (HasAuthority() && IsValid(CarriedToy) && CarriedToy->TBPS() &&
	    CarriedToy->TBPS()->ToyState == ETBToyState::Storing)
	{
		CarriedToy->SetToyState(ETBToyState::Carried);
		UTBAudioEvents::Emit(this, ETBSoundEvent::StoreCancel, TBPS(), FVector::ZeroVector);
	}
}

// 壁への埋まり込みを避け、前方に空きがなければ自分の位置へ落とす。
void ATBCharacter::DropToy()
{
	if (!HasAuthority())
	{
		return;
	}
	ATBCharacter* Toy = CarriedToy;
	if (CarriedDummy)
	{
		CarriedDummy->PutDown();
	}
	if (!IsValid(Toy))
	{
		CarriedToy = nullptr;
		return;
	}
	// 人間との衝突は無効なので、自分の位置を落下先の候補にする。
	FVector DropPosition = GetActorLocation();
	if (UWorld* World = GetWorld())
	{
		const FVector DesiredPosition =
		    DropPosition + FRotator(0, GetControlRotation().Yaw, 0).Vector() * TB::Rules().Aim.DropDistance;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TBDrop), false, this);
		Params.AddIgnoredActor(Toy);
		if (!World->SweepSingleByChannel(
		        Hit, DropPosition, DesiredPosition, FQuat::Identity, ECC_GameTraceChannel1,
		        FCollisionShape::MakeCapsule(Toy->GetCapsuleComponent()->GetScaledCapsuleRadius(),
		                                     Toy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
		        Params))
		{
			DropPosition = DesiredPosition;
		}
	}
	UTBAudioEvents::Emit(this, ETBSoundEvent::CarryDrop, TBPS(), FVector::ZeroVector);
	UTBAudioEvents::Emit(this, ETBSoundEvent::CarryDrop, Toy->TBPS(), FVector::ZeroVector);
	Toy->Carrier = nullptr;
	Toy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Toy->SetActorLocation(DropPosition, false, nullptr, ETeleportType::TeleportPhysics);
	Toy->SetToyState(ETBToyState::Free);
	CarriedToy = nullptr;
	ClearHeldInteraction();
	ApplyState();
	Toy->ForceNetUpdate();
}

// Eキーの要求を検証し、人間は収納・捕獲、おもちゃは救助を開始する。
void ATBCharacter::ServerInteract_Implementation()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastRequest < .12 || !CanAct())
	{
		return;
	}
	LastRequest = Now;
	const FTBInteractionTarget Target = TB::FindInteractionTarget(this);
	if (auto* Item = Cast<ITBInteractable>(Target.Actor))
	{
		FTBInteractionContext Context;
		Context.InstigatorCharacter = this;
		Context.ActionId = Target.ActionId;
		Item->TryInteract(Context);
		return;
	}
	bHoldingInteract = true;
	auto* MatchInfo = TB::GS(GetWorld());
	ATBBox* Box = MatchInfo ? MatchInfo->Box.Get() : nullptr;
	if (IsHuman())
	{
		if (CarriedDummy)
		{
			BeginDummyStorage();
			return;
		}
		if (CarriedToy)
		{
			if (Box && Box->CanStoreFrom(this) && CarriedToy->TBPS() &&
			    CarriedToy->TBPS()->ToyState == ETBToyState::Carried)
			{
				StoreStarted = Now;
				CarriedToy->SetToyState(ETBToyState::Storing);
			}
			return;
		}
		if (auto* Dummy = Cast<ATBDummyToy>(Target.Actor))
		{
			Dummy->PickUp(this);
			return;
		}
		auto* ClosestToy = Cast<ATBCharacter>(Target.Actor);
		if (ClosestToy)
		{
			ClosestToy->ClearHeldInteraction();
			CarriedToy = ClosestToy;
			ClosestToy->Carrier = this;
			ClosestToy->SetFrozen(false);
			ClosestToy->SetToyState(ETBToyState::Grabbed);
			ClosestToy->OnRep_Carrier();
			ClosestToy->ForceNetUpdate();
			GrabEnds = Now + TB::Rules().Aim.GrabSeconds;
			ApplyState();
			UTBAudioEvents::Emit(this, ETBSoundEvent::Capture, ClosestToy->TBPS(), ClosestToy->GetActorLocation());
			UTBAudioEvents::Emit(this, ETBSoundEvent::TeamNotice, ClosestToy->TBPS(), FVector::ZeroVector);
		}
		else
		{
			TB::Notice(this, TEXT("No toy in reach."));
		}
	}
	else
	{
		if (Box && Box->InRange(this) && Box->HasPrisoners())
		{
			bRescuing = true;
		}
		else
		{
			TB::Notice(this, TB::Notices::ItemsAutoCollect);
		}
	}
}

void ATBCharacter::ServerReleaseInteract_Implementation()
{
	ClearHeldInteraction();
}

void ATBCharacter::ServerDrop_Implementation()
{
	if (TB::Playing(GetWorld()))
	{
		DropToy();
	}
}

void ATBCharacter::RequestInteract()
{
	if (CanAct())
	{
		ServerInteract();
	}
}

void ATBCharacter::ReleaseInteract()
{
	ServerReleaseInteract();
}

void ATBCharacter::RequestDrop()
{
	ServerDrop();
}

void ATBCharacter::RequestTaunt()
{
	if (IsLocallyControlled() && CanAct() && TBPS()->Team == ETBTeam::Toy)
		ServerTaunt();
}

void ATBCharacter::RequestPush()
{
	if (IsLocallyControlled() && CanAct() && TBPS()->Team == ETBTeam::Toy)
		ServerPush();
}

void ATBCharacter::ServerPush_Implementation()
{
	TryPush();
}

bool ATBCharacter::TryPush()
{
	if (!HasAuthority() || !CanAct() || TBPS()->Team != ETBTeam::Toy) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastPushTime < .45) return false;
	LastPushTime = Now;

	const FVector Start = GetActorLocation();
	const FVector Forward = FRotator(0, GetControlRotation().Yaw, 0).Vector();
	constexpr float Reach = 42.f;
	FCollisionObjectQueryParams Types(ECC_PhysicsBody);
	Types.AddObjectTypesToQuery(ECC_WorldDynamic);
	Types.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	TArray<FHitResult> Contacts;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TBToyPush), false, this);
	GetWorld()->SweepMultiByObjectType(Contacts, Start, Start + Forward * Reach, FQuat::Identity,
	    Types, FCollisionShape::MakeSphere(12.f), Query);
	for (const FHitResult& Hit : Contacts)
	{
		auto* Part = Hit.GetComponent();
		auto* Ally = Cast<ATBCharacter>(Hit.GetActor());
		if (!Part) continue;
		if (Ally)
		{
			if (Part != Ally->GetCapsuleComponent() || !Ally->CanAct() || Ally->TBPS()->Team != ETBTeam::Toy) continue;
		}
		else if (!Part->ComponentHasTag(TEXT("TBPushable")) || !Part->IsSimulatingPhysics()) continue;

		if (FVector::DotProduct(Forward, Part->Bounds.Origin - Start) <= 0.f) continue;
		const FVector Contact = Hit.bStartPenetrating ? Part->Bounds.Origin : FVector(Hit.ImpactPoint);
		if (!TB::ClearPath(this, Hit.GetActor(), Start, Contact)) continue;

		if (Ally)
			Ally->LaunchCharacter(Forward * 145.f + FVector::UpVector * 65.f, true, false);
		else
			Part->AddImpulseAtLocation((Forward * 190.f + FVector::UpVector * 22.f) * Part->GetMass(), Contact);
		UTBAudioEvents::Emit(this, ETBSoundEvent::ToyPush, TBPS(), Contact);
		return true;
	}
	return false;
}

void ATBCharacter::ServerTaunt_Implementation()
{
	TryTaunt();
}

bool ATBCharacter::TryTaunt()
{
	if (!HasAuthority() || !CanAct() || TBPS()->Team != ETBTeam::Toy) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	const float Cooldown = TB::Rules().Toys.TauntCooldownSeconds;
	if (Now - LastTauntTime < Cooldown) return false;
	LastTauntTime = Now;
	UTBAudioEvents::Emit(this, ETBSoundEvent::ToyTaunt, TBPS(), GetActorLocation());
	return true;
}

void ATBCharacter::Forward(float InputValue)
{
	if (!IsMovementLocked())
	{
		AddMovementInput(FRotator(0, GetControlRotation().Yaw, 0).Vector(), InputValue);
	}
}

void ATBCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	Forward(Axis.Y);
	Right(Axis.X);
}

void ATBCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	Yaw(Axis.X);
	Pitch(Axis.Y);
}

void ATBCharacter::Right(float InputValue)
{
	if (!IsMovementLocked())
	{
		AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), InputValue);
	}
}

// 運ばれている間は移動できないが、人間の周りを左右に回り込める。
void ATBCharacter::Yaw(float InputValue)
{
	if (!IsMovementLocked() || Carrier)
	{
		AddControllerYawInput(InputValue * LookScale().X);
	}
}

// オプションで選んだ感度と上下反転。設定がないとき（専用サーバーなど）はそのまま。
FVector2D ATBCharacter::LookScale()
{
	const auto* Settings = UTBGameUserSettings::Get();
	return Settings ? Settings->GetLookScale() : FVector2D(1.f);
}

// 運ばれている間の見下ろし角は掴まれた時点の値で固定する。
void ATBCharacter::Pitch(float InputValue)
{
	if (!IsMovementLocked())
	{
		AddControllerPitchInput(InputValue * LookScale().Y);
	}
}

void ATBCharacter::JumpPressed()
{
	if (!IsMovementLocked())
	{
		Jump();
	}
}

void ATBCharacter::JumpReleased()
{
	StopJumping();
}

void ATBCharacter::SetSprintRequested(bool bRequested)
{
	CastChecked<UTBMovement>(GetCharacterMovement())->bWantsToSprint = bRequested;
}

void ATBCharacter::SprintPressed()
{
	SetSprintRequested(true);
}

void ATBCharacter::SprintReleased()
{
	SetSprintRequested(false);
}

void ATBCharacter::EmotePressed(ETBEmoteKind Kind)
{
	auto* PC = Cast<ATBController>(GetController());
	if (!PC || !PC->IsLocalController() || PC->GetPawn() != this || PC->IsChatOpen() ||
	    PC->IsLeaveMenuOpen() || !CanRequestEmote()) return;
	if (const auto* HUD = Cast<ATBHUD>(PC->GetHUD()); HUD && (HUD->IsChatBlocked() || HUD->IsStagePickerOpen())) return;
	if (Kind == ETBEmoteKind::WildDance)
	{
		// Secret fixed shortcut. Respect any existing player binding on zero.
		for (const FName Name : TB::Keys::All())
		{
			TB::FKeyboardKey Existing;
			if (TB::FindKeyboardKey(PC, Name, Existing) && Existing.Key == EKeys::Zero) return;
		}
		RequestEmote(Kind);
		return;
	}
	const FName Mapping = Kind == ETBEmoteKind::Wave ? TB::Keys::EmoteWave :
	    Kind == ETBEmoteKind::Cheer ? TB::Keys::EmoteCheer :
	    Kind == ETBEmoteKind::Dance ? TB::Keys::EmoteDance : NAME_None;
	TB::FKeyboardKey Row;
	// Newly registered defaults must not silently compete with an existing saved profile.
	// Read only: the user resolves conflicts through the existing controls page.
	if (Mapping.IsNone() || !TB::FindKeyboardKey(PC, Mapping, Row) ||
	    TB::CheckRemap(PC, Mapping, Row.Key).Result != TB::ERemapResult::OK) return;
	RequestEmote(Kind);
}

void ATBCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	FInputKeyBinding SecretDance(FInputChord(EKeys::Zero), IE_Pressed);
	SecretDance.bConsumeInput = false;
	SecretDance.KeyDelegate.GetDelegateForManualSet().BindUObject(this, &ATBCharacter::EmotePressed, ETBEmoteKind::WildDance);
	Input->KeyBindings.Add(MoveTemp(SecretDance));
	if (auto* Enhanced = Cast<UEnhancedInputComponent>(Input))
	{
		const auto* Settings = GetDefault<UTBInputSettings>();
		if (const UInputAction* LookAction = Settings->LookAction.LoadSynchronous())
		{
			Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATBCharacter::Look);
		}
		if (const UInputAction* MoveAction = Settings->MoveAction.LoadSynchronous())
		{
			Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATBCharacter::Move);
		}
		if (const UInputAction* JumpAction = Settings->JumpAction.LoadSynchronous())
		{
			Enhanced->BindAction(JumpAction, ETriggerEvent::Started, this, &ATBCharacter::JumpPressed);
			// 離したときやIMCが外れたときはCompletedが来る。Canceledは今はほぼ来ないが、Holdなどのトリガーを足したときの予防。
			Enhanced->BindAction(JumpAction, ETriggerEvent::Completed, this, &ATBCharacter::JumpReleased);
			Enhanced->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ATBCharacter::JumpReleased);
		}
		// しゃがみは人間、ダッシュは人形のIMCにだけある。どちらが効くかは付いているIMCで決まる。
		if (const UInputAction* CrouchAction = Settings->CrouchAction.LoadSynchronous())
		{
			Enhanced->BindAction(CrouchAction, ETriggerEvent::Started, this, &ATBCharacter::CrouchPressed);
			Enhanced->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ATBCharacter::CrouchReleased);
			Enhanced->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &ATBCharacter::CrouchReleased);
		}
		if (const UInputAction* SprintAction = Settings->SprintAction.LoadSynchronous())
		{
			Enhanced->BindAction(SprintAction, ETriggerEvent::Started, this, &ATBCharacter::SprintPressed);
			Enhanced->BindAction(SprintAction, ETriggerEvent::Completed, this, &ATBCharacter::SprintReleased);
			Enhanced->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ATBCharacter::SprintReleased);
		}
		// インタラクトは両方の陣営のIMCにある。置くは人間、挑発は人形のIMCにだけある。
		if (const UInputAction* InteractAction = Settings->InteractAction.LoadSynchronous())
		{
			Enhanced->BindAction(InteractAction, ETriggerEvent::Started, this, &ATBCharacter::RequestInteract);
			Enhanced->BindAction(InteractAction, ETriggerEvent::Completed, this, &ATBCharacter::ReleaseInteract);
			Enhanced->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ATBCharacter::ReleaseInteract);
		}
		if (const UInputAction* DropAction = Settings->DropAction.LoadSynchronous())
		{
			Enhanced->BindAction(DropAction, ETriggerEvent::Started, this, &ATBCharacter::RequestDrop);
		}
		if (const UInputAction* TauntAction = Settings->TauntAction.LoadSynchronous())
		{
			Enhanced->BindAction(TauntAction, ETriggerEvent::Started, this, &ATBCharacter::RequestTaunt);
		}
		if (const UInputAction* PushAction = Settings->PushAction.LoadSynchronous())
		{
			Enhanced->BindAction(PushAction, ETriggerEvent::Started, this, &ATBCharacter::RequestPush);
		}
		if (const UInputAction* Action = Settings->EmoteWaveAction.LoadSynchronous())
			Enhanced->BindAction(Action, ETriggerEvent::Started, this, &ATBCharacter::EmotePressed, ETBEmoteKind::Wave);
		if (const UInputAction* Action = Settings->EmoteCheerAction.LoadSynchronous())
			Enhanced->BindAction(Action, ETriggerEvent::Started, this, &ATBCharacter::EmotePressed, ETBEmoteKind::Cheer);
		if (const UInputAction* Action = Settings->EmoteDanceAction.LoadSynchronous())
			Enhanced->BindAction(Action, ETriggerEvent::Started, this, &ATBCharacter::EmotePressed, ETBEmoteKind::Dance);
	}
}

void ATBCharacter::ApplyInterruption()
{
	if (!HasAuthority())
	{
		return;
	}
	auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>();
	if (!GameMode || !GameMode->bAllowInterference || !TB::Playing(GetWorld()))
	{
		return;
	}
	const bool bWasStoring = HasCarriedToy() && StoreStarted >= 0;
	ClearHeldInteraction();
	if (bWasStoring)
	{
		DropToy();
	}
}

// 接触中の対象を維持する。凍結中は進捗を保持し、接触が切れたらリセットする。
void ATBCharacter::UpdateItemContact(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	auto* ToyPlayerInfo = TBPS();
	const auto* State = TB::GS(GetWorld());
	const bool Eligible = TB::Playing(GetWorld()) && State && !State->IsPreparing() && ToyPlayerInfo && ToyPlayerInfo->Team == ETBTeam::Toy &&
	                      ToyPlayerInfo->ToyState == ETBToyState::Free;
	if (!Eligible)
	{
		ContactItem = nullptr;
		ItemProgress = 0;
		return;
	}
	if (!IsValid(ContactItem) || !ContactItem->Touches(this))
	{
		ContactItem = nullptr;
		ItemProgress = 0;
		float ClosestDistanceSquared = MAX_flt;
		for (TActorIterator<ATBPickup> It(GetWorld()); It; ++It)
		{
			const float DistanceSquared = FVector::DistSquared(GetActorLocation(), It->GetActorLocation());
			if (It->Touches(this) && DistanceSquared < ClosestDistanceSquared)
			{
				ContactItem = *It;
				ClosestDistanceSquared = DistanceSquared;
			}
		}
	}
	if (!ContactItem)
	{
		return;
	}
	ItemProgress = TBRuleMath::AdvanceContact(ItemProgress, DeltaSeconds, TB::Rules().Pickup.CollectSeconds, true,
	                                          ToyPlayerInfo->bFrozen, true);
	if (ItemProgress >= 1.f && ContactItem->Take(this))
	{
		ContactItem = nullptr;
		ItemProgress = 0;
	}
}
