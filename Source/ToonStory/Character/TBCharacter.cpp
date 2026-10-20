// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBCharacter.h"
#include "GamePlay/TBRoomLighting.h"
#include "Audio/TBAudioEvents.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Core/TBController.h"
#include "Character/TBMovement.h"
#include "Character/TBVoiceComponent.h"
#include "Audio/TBCharacterAudioComponent.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBDummyToy.h"
#include "GamePlay/TBPickup.h"
#include "GamePlay/Items/TBInteractable.h"
#include "GamePlay/Items/TBInteractionTarget.h"
#include "GamePlay/TBRuleMath.h"
#include "Data/TBCharacterData.h"
#include "Data/TBRuleData.h"
#include "Core/TBGameHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
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
	ChildMesh.LoadSynchronous();
	ChildAnimation.LoadSynchronous();
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
	bAppearanceInitialized = true;
	bShowingToy = bToy;
	GetMesh()->SetVisibility(false);
	Body->SetVisibility(true);
	if (!bToy && ChildMesh.Get())
	{
		Body->SetVisibility(false);
		auto* Visual = GetMesh();
		Visual->SetSkeletalMesh(ChildMesh.Get());
		const FBoxSphereBounds Bounds = ChildMesh->GetBounds();
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
		if (ChildAnimation.Get())
		{
			Visual->SetAnimInstanceClass(ChildAnimation.Get());
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

bool ATBCharacter::CanAct() const
{
	const auto* ToyPlayerInfo = TBPS();
	return TB::Playing(GetWorld()) && ToyPlayerInfo && !ToyPlayerInfo->bFrozen &&
	       (ToyPlayerInfo->Team == ETBTeam::Human ||
	        (ToyPlayerInfo->Team == ETBTeam::Toy && ToyPlayerInfo->ToyState == ETBToyState::Free));
}

bool ATBCharacter::IsGazeFrozen() const
{
	return TBPS() && TBPS()->bFrozen;
}

bool ATBCharacter::IsPhysicsLocked() const
{
	const auto* MatchInfo = TB::GS(GetWorld());
	const auto* ToyPlayerInfo = TBPS();
	if (MatchInfo && (MatchInfo->Phase == ETBPhase::Results || MatchInfo->Phase == ETBPhase::Aborted))
	{
		return true;
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
	Super::PossessedBy(PlayerController);
	ApplyState();
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
	DOREPLIFETIME_CONDITION(ATBCharacter, ContactItem, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATBCharacter, ItemProgress, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATBCharacter, StoreStarted, COND_OwnerOnly);
}

// 進捗と捕獲状態の確定はサーバー側だけで進める。
void ATBCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyState();
	UpdateCarryCamera();
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
			TB::Notice(this, TEXT("Items collect automatically while touching. E is for rescue."));
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

void ATBCharacter::ServerTaunt_Implementation()
{
	TryTaunt();
}

bool ATBCharacter::TryTaunt()
{
	if (!HasAuthority() || !CanAct() || TBPS()->Team != ETBTeam::Toy) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	const float Cooldown = FMath::Max(.1f, TB::Rules().Toys.TauntCooldownSeconds);
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
		AddControllerYawInput(InputValue);
	}
}

// 運ばれている間の見下ろし角は掴まれた時点の値で固定する。
void ATBCharacter::Pitch(float InputValue)
{
	if (!IsMovementLocked())
	{
		AddControllerPitchInput(InputValue);
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

void ATBCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAxis("Forward", this, &ATBCharacter::Forward);
	Input->BindAxis("Right", this, &ATBCharacter::Right);
	Input->BindAxis("Yaw", this, &ATBCharacter::Yaw);
	Input->BindAxis("Pitch", this, &ATBCharacter::Pitch);
	Input->BindAction("Jump", IE_Pressed, this, &ATBCharacter::JumpPressed);
	Input->BindAction("Jump", IE_Released, this, &ATBCharacter::JumpReleased);
	Input->BindKey(EKeys::LeftControl, IE_Pressed, this, &ATBCharacter::CrouchPressed);
	Input->BindKey(EKeys::LeftControl, IE_Released, this, &ATBCharacter::CrouchReleased);
	Input->BindKey(EKeys::LeftShift, IE_Pressed, this, &ATBCharacter::SprintPressed);
	Input->BindKey(EKeys::LeftShift, IE_Released, this, &ATBCharacter::SprintReleased);
	Input->BindAction("Interact", IE_Pressed, this, &ATBCharacter::RequestInteract);
	Input->BindAction("Interact", IE_Released, this, &ATBCharacter::ReleaseInteract);
	Input->BindAction("Drop", IE_Pressed, this, &ATBCharacter::RequestDrop);
	Input->BindAction("Taunt", IE_Pressed, this, &ATBCharacter::RequestTaunt);
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
	const bool Eligible = TB::Playing(GetWorld()) && ToyPlayerInfo && ToyPlayerInfo->Team == ETBTeam::Toy &&
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
