#include "TBCharacter.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBGameMode.h"
#include "Character/TBMovement.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBPickup.h"
#include "GamePlay/TBRuleMath.h"
#include "Core/TBGameHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
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
	GetCapsuleComponent()->InitCapsuleSize(34, 88);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// 運搬カメラの壁よけ判定で、人間自身に当たってアームが縮まないようにする。
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	bUseControllerRotationYaw = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
	Camera->bUsePawnControlRotation = true;
	Camera->FieldOfView = 90.f;
	Camera->AspectRatio = 16.f / 9.f;
	Camera->bConstrainAspectRatio = true;
	Camera->SetRelativeLocation(FVector(0, 0, 64));
	CarryCameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CarryCameraArm"));
	CarryCameraArm->SetupAttachment(GetRootComponent());
	CarryCameraArm->TargetArmLength = 450.f;
	CarryCameraArm->TargetOffset = FVector(0, 0, 80);
	// 位置は人間に追従し、回転は所有者であるおもちゃの視点を使う。
	CarryCameraArm->SetUsingAbsoluteRotation(true);
	CarryCameraArm->bUsePawnControlRotation = true;
	// 人間のメッシュ側のネット補間を使い、注視点だけが遅れて中心からずれるのを防ぐ。
	CarryCameraArm->bEnableCameraLag = false;
	CarryCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CarryCamera"));
	CarryCamera->SetupAttachment(CarryCameraArm, USpringArmComponent::SocketName);
	CarryCamera->FieldOfView = 90.f;
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
	CarryAnchor->SetRelativeLocation(FVector(95, 0, 10));
	GetCharacterMovement()->MaxWalkSpeed = HumanSpeed;
}

void ATBCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		AddTickPrerequisiteActor(GameMode);
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
	GetCapsuleComponent()->SetCapsuleSize(Toy ? 20.f : 34.f, Toy ? 35.f : 88.f);
	BaseEyeHeight = Toy ? 24.f : 64.f;
	Camera->SetRelativeLocation(FVector(0, 0, BaseEyeHeight));
	Body->SetRelativeScale3D(Toy ? FVector(.35, .35, .6) : FVector(.55, .55, 1.6));
	const bool Held = ToyPlayerInfo->ToyState == ETBToyState::Grabbed ||
	                  ToyPlayerInfo->ToyState == ETBToyState::Carried ||
	                  ToyPlayerInfo->ToyState == ETBToyState::Storing;
	GetCapsuleComponent()->SetCollisionEnabled(Held ? ECollisionEnabled::NoCollision
	                                                : ECollisionEnabled::QueryAndPhysics);
	const bool PhysicsLocked = IsPhysicsLocked();
	auto* Movement = GetCharacterMovement();
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
	Movement->MaxWalkSpeed = Toy ? ToySpeed : (CarriedToy ? CarrySpeed : HumanSpeed);
	bUseControllerRotationYaw = !IsMovementLocked();
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
	if (Carrier)
	{
		AttachToComponent(Carrier->CarryAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
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
	const bool bCarried = IsValid(Carrier);
	if (bCarried == bCarryView)
	{
		return;
	}
	bCarryView = bCarried;
	AController* PlayerController = GetController();
	if (bCarried)
	{
		CarryCameraArm->AttachToComponent(Carrier->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		// 掴まれる前に見ていた方角を保ち、以降はマウス左右だけで回り込む。
		if (PlayerController)
		{
			PlayerController->SetControlRotation(FRotator(CarryViewPitch, PlayerController->GetControlRotation().Yaw, 0));
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
	Camera->SetActive(!bCarried);
	CarryCamera->SetActive(bCarried);
	// 抱えられている自分も画面に入れる。
	Body->SetOwnerNoSee(!bCarried);
}

void ATBCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBCharacter, Carrier);
	DOREPLIFETIME(ATBCharacter, CarriedToy);
	DOREPLIFETIME_CONDITION(ATBCharacter, ContactItem, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ATBCharacter, ItemProgress, COND_OwnerOnly);
}

// 進捗と捕獲状態の確定はサーバー側だけで進める。
void ATBCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyState();
	UpdateCarryCamera();
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
	}
	if (ToyPlayerState->ToyState == ETBToyState::Storing)
	{
		auto* MatchInfo = TB::GS(GetWorld());
		auto* Box = MatchInfo ? MatchInfo->Box.Get() : nullptr;
		if (!bHoldingInteract || !Box || !Box->InRange(this))
		{
			DropToy();
			return;
		}
		if (Now - StoreStarted >= Box->StoreSeconds && Box->Store(CarriedToy))
		{
			CarriedToy = nullptr;
			bHoldingInteract = false;
			ApplyState();
		}
	}
}

void ATBCharacter::ClearHeldInteraction()
{
	bHoldingInteract = false;
	bRescuing = false;
}

// 壁への埋まり込みを避け、前方に空きがなければ自分の位置へ落とす。
void ATBCharacter::DropToy()
{
	if (!HasAuthority())
	{
		return;
	}
	ATBCharacter* Toy = CarriedToy;
	if (!IsValid(Toy))
	{
		CarriedToy = nullptr;
		return;
	}
	// 人間との衝突は無効なので、自分の位置を落下先の候補にする。
	FVector DropPosition = GetActorLocation();
	if (UWorld* World = GetWorld())
	{
		const FVector DesiredPosition = DropPosition + GetActorForwardVector() * 100;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TBDrop), false, this);
		Params.AddIgnoredActor(Toy);
		if (!World->SweepSingleByChannel(Hit, DropPosition, DesiredPosition, FQuat::Identity, ECC_GameTraceChannel1,
		                                 FCollisionShape::MakeCapsule(20.f, 35.f), Params))
		{
			DropPosition = DesiredPosition;
		}
	}
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
	bHoldingInteract = true;
	auto* MatchInfo = TB::GS(GetWorld());
	ATBBox* Box = MatchInfo ? MatchInfo->Box.Get() : nullptr;
	if (IsHuman())
	{
		if (CarriedToy)
		{
			if (Box && Box->InRange(this) && CarriedToy->TBPS()->ToyState == ETBToyState::Carried)
			{
				StoreStarted = Now;
				CarriedToy->SetToyState(ETBToyState::Storing);
			}
			return;
		}
		ATBCharacter* ClosestToy = nullptr;
		float ClosestDistance = 180.f;
		for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
		{
			auto* Toy = *It;
			auto* TargetState = Toy->TBPS();
			if (!TargetState || TargetState->Team != ETBTeam::Toy || Toy->Carrier)
			{
				continue;
			}
			if (TargetState->ToyState != ETBToyState::Free)
			{
				continue;
			}
			const FVector Delta = Toy->GetActorLocation() - GetActorLocation();
			const float Distance = Delta.Size();
			if (Distance > ClosestDistance ||
			    FVector::DotProduct(GetControlRotation().Vector(), Delta.GetSafeNormal()) < .35f)
			{
				continue;
			}
			if (!TB::ClearPath(this, Toy, GetPawnViewLocation(), Toy->GetActorLocation()))
			{
				continue;
			}
			ClosestToy = Toy;
			ClosestDistance = Distance;
		}
		if (ClosestToy)
		{
			ClosestToy->ClearHeldInteraction();
			ClosestToy->SetFrozen(false);
			CarriedToy = ClosestToy;
			ClosestToy->Carrier = this;
			ClosestToy->SetToyState(ETBToyState::Grabbed);
			ClosestToy->OnRep_Carrier();
			ClosestToy->ForceNetUpdate();
			GrabEnds = Now + .25;
			ApplyState();
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
	if (CarriedToy && CarriedToy->TBPS() && CarriedToy->TBPS()->ToyState == ETBToyState::Storing)
	{
		DropToy();
	}
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

void ATBCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	Input->BindAxis("Forward", this, &ATBCharacter::Forward);
	Input->BindAxis("Right", this, &ATBCharacter::Right);
	Input->BindAxis("Yaw", this, &ATBCharacter::Yaw);
	Input->BindAxis("Pitch", this, &ATBCharacter::Pitch);
	Input->BindAction("Jump", IE_Pressed, this, &ATBCharacter::JumpPressed);
	Input->BindAction("Jump", IE_Released, this, &ATBCharacter::JumpReleased);
	Input->BindAction("Interact", IE_Pressed, this, &ATBCharacter::RequestInteract);
	Input->BindAction("Interact", IE_Released, this, &ATBCharacter::ReleaseInteract);
	Input->BindAction("Drop", IE_Pressed, this, &ATBCharacter::RequestDrop);
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
	ClearHeldInteraction();
	if (CarriedToy && CarriedToy->TBPS() && CarriedToy->TBPS()->ToyState == ETBToyState::Storing)
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
	ItemProgress = TBRuleMath::AdvanceContact(ItemProgress, DeltaSeconds, ContactItem->CollectSeconds, true,
	                                          ToyPlayerInfo->bFrozen, true);
	if (ItemProgress >= 1.f && ContactItem->Take(this))
	{
		ContactItem = nullptr;
		ItemProgress = 0;
	}
}
