#include "TBGame.h"
#include "TBRuleMath.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "TBSession.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameInstance.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace TB
{
	ATBGameState* GS(const UWorld* World)
	{
		return World ? World->GetGameState<ATBGameState>() : nullptr;
	}

	bool Playing(const UWorld* World)
	{
		const auto* MatchInfo = GS(World);
		return MatchInfo && MatchInfo->Phase == ETBPhase::Playing;
	}

	// 判定元と判定先自身を除外し、間に壁などがあるか調べる。
	bool ClearPath(const AActor* From, const AActor* To, const FVector& Start, const FVector& End)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TBInteract), false, From);
		QueryParams.AddIgnoredActor(To);
		return !From->GetWorld()->LineTraceTestByChannel(Start, End, ECC_Visibility, QueryParams);
	}

	void Notice(ATBCharacter* ToyCharacter, const FString& Text)
	{
		if (auto* PlayerController = Cast<ATBController>(ToyCharacter->GetController()))
		{
			PlayerController->ClientNotice(Text);
		}
	}
} // namespace TB

void ATBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBPlayerState, Team);
	DOREPLIFETIME(ATBPlayerState, ToyState);
	DOREPLIFETIME(ATBPlayerState, bFrozen);
	DOREPLIFETIME(ATBPlayerState, bReady);
	DOREPLIFETIME(ATBPlayerState, Preference);
	DOREPLIFETIME(ATBPlayerState, Items);
}

void ATBPlayerState::OnRep_State()
{
	if (auto* ToyCharacter = Cast<ATBCharacter>(GetPawn()))
	{
		ToyCharacter->ApplyState();
		ToyCharacter->StateVisualChanged();
	}
}

void ATBPlayerState::CopyProperties(APlayerState* NewState)
{
	Super::CopyProperties(NewState);
	if (auto* CopiedState = Cast<ATBPlayerState>(NewState))
	{
		CopiedState->Team = Team;
		CopiedState->Preference = Preference;
	}
}

void ATBGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBGameState, Settings);
	DOREPLIFETIME(ATBGameState, Phase);
	DOREPLIFETIME(ATBGameState, Winner);
	DOREPLIFETIME(ATBGameState, Reason);
	DOREPLIFETIME(ATBGameState, EndTime);
	DOREPLIFETIME(ATBGameState, Collected);
	DOREPLIFETIME(ATBGameState, BoxedCount);
	DOREPLIFETIME(ATBGameState, Box);
}

float ATBGameState::Remaining() const
{
	return Phase == ETBPhase::Playing ? FMath::Max(0., EndTime - GetServerWorldTimeSeconds()) : 0.f;
}

void UTBMovement::PerformMovement(float DeltaSeconds)
{
	const auto* ToyCharacter = Cast<ATBCharacter>(CharacterOwner);
	if (ToyCharacter && ToyCharacter->IsPhysicsLocked())
	{
		StopMovementImmediately();
		return;
	}
	if (ToyCharacter && ToyCharacter->IsGazeFrozen())
	{
		// Keep CharacterMovement's prediction/correction path, but use vertical-only physics.
		if (MovementMode != MOVE_Custom || CustomMovementMode != 1)
		{
			SetMovementMode(MOVE_Custom, 1);
		}
		Acceleration = FVector::ZeroVector;
		CharacterOwner->StopJumping();
	}
	else if (MovementMode == MOVE_Custom && CustomMovementMode == 1)
	{
		SetMovementMode(MOVE_Falling);
	}
	Super::PerformMovement(DeltaSeconds);
}

// 凍結中の専用物理。横速度と上昇を止め、落下と床の衝突を処理する。
void UTBMovement::PhysCustom(float DeltaSeconds, int32 Iterations)
{
	if (CustomMovementMode != 1)
	{
		Super::PhysCustom(DeltaSeconds, Iterations);
		return;
	}
	if (!UpdatedComponent || DeltaSeconds < MIN_TICK_TIME)
	{
		return;
	}
	// Static-level v1: no horizontal motion, jump boost, slopes sliding or moving bases.
	Velocity.X = 0;
	Velocity.Y = 0;
	Velocity.Z = TBRuleMath::FrozenVerticalSpeed(Velocity.Z);
	const float OldZ = Velocity.Z;
	const auto* Volume = GetPhysicsVolume();
	Velocity.Z = FMath::Max(Velocity.Z + GetGravityZ() * DeltaSeconds, -(Volume ? Volume->TerminalVelocity : 4000.f));
	const FVector Delta(0, 0, (OldZ + Velocity.Z) * .5f * DeltaSeconds);
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit() && Hit.Normal.Z > 0)
	{
		Velocity.Z = 0;
	}
}

// サーバーが受け取った移動入力にも、凍結・運搬中の制限を適用する。
void UTBMovement::MoveAutonomous(float ClientTimeStamp, float DeltaSeconds, uint8 Flags, const FVector& NewAcceleration)
{
	const auto* ToyCharacter = Cast<ATBCharacter>(CharacterOwner);
	if (ToyCharacter && ToyCharacter->IsPhysicsLocked())
	{
		StopMovementImmediately();
		DisableMovement();
		return;
	}
	if (ToyCharacter && ToyCharacter->IsGazeFrozen())
	{
		Super::MoveAutonomous(ClientTimeStamp, DeltaSeconds, 0, FVector::ZeroVector);
		return;
	}
	Super::MoveAutonomous(ClientTimeStamp, DeltaSeconds, Flags, NewAcceleration);
}

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
	GetCharacterMovement()->bOrientRotationToMovement = false;
	bUseControllerRotationYaw = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
	Camera->bUsePawnControlRotation = true;
	Camera->FieldOfView = 90.f;
	Camera->AspectRatio = 16.f / 9.f;
	Camera->bConstrainAspectRatio = true;
	Camera->SetRelativeLocation(FVector(0, 0, 64));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetRootComponent());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetOwnerNoSee(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
	}
	Body->SetRelativeScale3D(FVector(.55, .55, 1.6));
	CarryAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("CarryAnchor"));
	CarryAnchor->SetupAttachment(GetRootComponent());
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

void ATBCharacter::Yaw(float InputValue)
{
	if (!IsMovementLocked())
	{
		AddControllerYawInput(InputValue);
	}
}

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

ATBBox::ATBBox()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(10);
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	auto* Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorLight"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(400, 0, 250));
	Light->SetIntensity(5000.f);
	Light->SetAttenuationRadius(900.f);
	InteractionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("Interaction"));
	InteractionPoint->SetupAttachment(Root);
	InteractionPoint->SetRelativeLocation(FVector(-120, 220, 60));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto* Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InteractMarker"));
	Marker->SetupAttachment(Root);
	if (Cube.Succeeded())
	{
		Marker->SetStaticMesh(Cube.Object);
	}
	Marker->SetRelativeLocation(FVector(-120, 220, 20));
	Marker->SetRelativeScale3D(FVector(.4, .4, .4));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const FVector Pos[] = {{400, 0, -10}, {0, -210, 160},   {0, 210, 160},   {0, 0, 280},
	                       {800, 0, 160}, {400, -300, 160}, {400, 300, 160}, {400, 0, 330}};
	const FVector Scale[] = {{8, 6, .2},   {.2, 1.8, 3.2}, {.2, 1.8, 3.2}, {.2, 2.4, .8},
	                         {.2, 6, 3.2}, {8, .2, 3.2},   {8, .2, 3.2},   {8, 6, .2}};
	for (int32 N = 0; N < 8; ++N)
	{
		auto* M = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wall%d"), N));
		M->SetupAttachment(Root);
		if (Cube.Succeeded())
		{
			M->SetStaticMesh(Cube.Object);
		}
		M->SetRelativeLocation(Pos[N]);
		M->SetRelativeScale3D(Scale[N]);
		M->SetCollisionProfileName(TEXT("BlockAll"));
	}
	// 前面は固定壁。救助は開閉を行わず、箱外への移動で成立する。
	auto* FrontWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontWall"));
	FrontWall->SetupAttachment(Root);
	if (Cube.Succeeded())
	{
		FrontWall->SetStaticMesh(Cube.Object);
	}
	FrontWall->SetRelativeLocation(FVector(0, 0, 120));
	FrontWall->SetRelativeScale3D(FVector(.2, 2.4, 2.4));
	FrontWall->SetCollisionProfileName(TEXT("BlockAll"));
}

void ATBBox::BeginPlay()
{
	Super::BeginPlay();
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		AddTickPrerequisiteActor(GameMode);
	}
}

void ATBBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBBox, RescueProgress);
	DOREPLIFETIME(ATBBox, Rescuers);
	DOREPLIFETIME(ATBBox, AlarmUntil);
}

bool ATBBox::InRange(const ATBCharacter* ToyCharacter) const
{
	if (!ToyCharacter ||
	    FVector::Dist(ToyCharacter->GetActorLocation(), InteractionPoint->GetComponentLocation()) > 180.f)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TBBoxInteraction), false, ToyCharacter);
	// Do not ignore this box: its walls must block access to the outside console.
	return !GetWorld()->LineTraceTestByChannel(ToyCharacter->GetPawnViewLocation(),
	                                           InteractionPoint->GetComponentLocation(), ECC_Visibility, Params);
}

bool ATBBox::HasPrisoners() const
{
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		if (It->TBPS() && It->TBPS()->ToyState == ETBToyState::Boxed)
		{
			return true;
		}
	}
	return false;
}

// 収納の最終確認を行い、箱内へ移してから勝敗を確認する。
bool ATBBox::Store(ATBCharacter* Toy)
{
	if (!HasAuthority() || !Toy || !Toy->TBPS() || !TB::Playing(GetWorld()))
	{
		return false;
	}
	// 収納済みのおもちゃと重ならない空き位置を選ぶ。
	FVector StoragePosition;
	bool bFoundPosition = false;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TBStorage), false, Toy);
	for (int32 Slot = 0; Slot < 9; ++Slot)
	{
		const FVector Candidate =
		    GetActorTransform().TransformPosition(FVector(200 + (Slot % 3) * 180, -170 + (Slot / 3) * 170, 60));
		if (!GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_GameTraceChannel1,
		                                              FCollisionShape::MakeCapsule(20.f, 35.f), QueryParams))
		{
			StoragePosition = Candidate;
			bFoundPosition = true;
			break;
		}
	}
	if (!bFoundPosition)
	{
		return false;
	}
	Toy->Carrier = nullptr;
	Toy->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Toy->SetActorLocation(StoragePosition, false, nullptr, ETeleportType::TeleportPhysics);
	Toy->SetFrozen(false);
	Toy->SetToyState(ETBToyState::Boxed);
	Toy->ForceNetUpdate();
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->CheckWin();
	}
	return true;
}

// 救助人数と進捗をサーバーで更新する。
void ATBBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (!TB::Playing(GetWorld()))
	{
		RescueProgress = 0;
		Rescuers = 0;
		return;
	}
	int32 RescueCount = 0;
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		auto* ToyCharacter = *It;
		if (!ToyCharacter->bRescuing)
		{
			continue;
		}
		if (!ToyCharacter->bHoldingInteract || !ToyCharacter->CanAct() || ToyCharacter->IsHuman() ||
		    !InRange(ToyCharacter))
		{
			ToyCharacter->bRescuing = false;
			continue;
		}
		++RescueCount;
	}
	if (!HasPrisoners())
	{
		RescueCount = 0;
	}
	if (RescueCount == 0)
	{
		Rescuers = 0;
		RescueProgress = 0;
		return;
	}
	if (Rescuers == 0)
	{
		AlarmUntil = GetWorld()->GetTimeSeconds() + 3;
		MulticastAlarm();
	}
	Rescuers = RescueCount;
	RescueProgress =
	    FMath::Min(1.f, RescueProgress + DeltaSeconds * TBRuleMath::RescueRate(RescueCount, BaseRescueSeconds));
	if (RescueProgress >= 1.f)
	{
		ReleasePrisoners();
	}
}

// 空いている箱外の位置へ救助する。塞がっている場合は次のTickで再試行する。
void ATBBox::ReleasePrisoners()
{
	if (!HasAuthority() || !TB::Playing(GetWorld()))
	{
		return;
	}
	int32 Slot = 0;
	for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
	{
		auto* Toy = *It;
		if (!Toy->TBPS() || Toy->TBPS()->ToyState != ETBToyState::Boxed)
		{
			continue;
		}
		const FVector Exit =
		    GetActorTransform().TransformPosition(FVector(-240 - (Slot / 5) * 100, -240 + (Slot % 5) * 100, 60));
		++Slot;
		if (Toy->TeleportTo(Exit, GetActorRotation()))
		{
			Toy->SetFrozen(false);
			Toy->SetToyState(ETBToyState::Free);
		}
	}
	if (!HasPrisoners())
	{
		RescueProgress = 0;
		Rescuers = 0;
		for (TActorIterator<ATBCharacter> It(GetWorld()); It; ++It)
		{
			It->bRescuing = false;
		}
	}
	ForceNetUpdate();
}

void ATBBox::MulticastAlarm_Implementation()
{
	// A global 2D cue must not disappear due to distance attenuation.
	if (GetNetMode() != NM_DedicatedServer && AlarmSound)
	{
		UGameplayStatics::PlaySound2D(this, AlarmSound);
	}
	AlarmVisual();
}

ATBPickup::ATBPickup()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	Contact = CreateDefaultSubobject<USphereComponent>(TEXT("Contact"));
	SetRootComponent(Contact);
	Contact->SetSphereRadius(30.f);
	Contact->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Contact->SetCollisionResponseToAllChannels(ECR_Ignore);
	Contact->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Contact->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	Contact->SetGenerateOverlapEvents(true);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Contact);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
	Mesh->SetRelativeScale3D(FVector(.35));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ATBPickup::Touches(const ATBCharacter* ToyCharacter) const
{
	return !bTaken && ToyCharacter && Contact->IsOverlappingActor(ToyCharacter) &&
	       TB::ClearPath(ToyCharacter, this, ToyCharacter->GetPawnViewLocation(), GetActorLocation());
}

void ATBPickup::OnRep_Taken()
{
	SetActorHiddenInGame(bTaken);
	Contact->SetCollisionEnabled(bTaken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

// 接触と進捗を再確認する。取得済みフラグを先に立てて二重取得を防ぐ。
bool ATBPickup::Take(ATBCharacter* ToyCharacter)
{
	if (!HasAuthority() || bTaken || !ToyCharacter || !ToyCharacter->CanAct() || ToyCharacter->IsHuman())
	{
		return false;
	}
	if (ToyCharacter->ContactItem != this || ToyCharacter->ItemProgress < 1.f || !Touches(ToyCharacter))
	{
		return false;
	}
	auto* MatchInfo = TB::GS(GetWorld());
	if (!MatchInfo || !ToyCharacter->TBPS())
	{
		return false;
	}
	bTaken = true;
	OnRep_Taken();
	ForceNetUpdate();
	++MatchInfo->Collected;
	++ToyCharacter->TBPS()->Items;
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->CheckWin();
	}
	return true;
}

void ATBPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBPickup, bTaken);
}

ATBBlock::ATBBlock()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetMobility(EComponentMobility::Movable);
}

ATBGameMode::ATBGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PlayerStateClass = ATBPlayerState::StaticClass();
	GameStateClass = ATBGameState::StaticClass();
	PlayerControllerClass = ATBController::StaticClass();
	DefaultPawnClass = ATBCharacter::StaticClass();
	HUDClass = ATBHUD::StaticClass();
	BoxClass = ATBBox::StaticClass();
	// A single arena doubles as the lobby; v1 deliberately requires no travel persistence.
}

void ATBGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (bBuildTestArena && SpawnPoints.IsEmpty())
	{
		BuildArena();
	}
	if (auto* MatchInfo = TB::GS(GetWorld()))
	{
		for (TActorIterator<ATBBox> It(GetWorld()); It; ++It)
		{
			MatchInfo->Box = *It;
			break;
		}
	}
}

// 検証用の床・壁・箱・開始位置・アイテムをサーバーで一度だけ生成する。
void ATBGameMode::BuildArena()
{
	if (!SpawnPoints.IsEmpty())
	{
		return;
	}
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
	for (int32 Index = 0; Index < 8; ++Index)
	{
		auto* Position = GetWorld()->SpawnActor<APlayerStart>(
		    FVector(-2000 + (Index % 4) * 300, -1800 + (Index / 4) * 350, 120), FRotator::ZeroRotator);
		if (Position)
		{
			SpawnPoints.Add(Position);
		}
		GetWorld()->SpawnActor<ATBPickup>(FVector(-1900 + (Index % 4) * 950, -2200 + (Index / 4) * 4400, 40),
		                                  FRotator::ZeroRotator);
	}
}
AActor* ATBGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (bBuildTestArena && SpawnPoints.IsEmpty())
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
	if (MatchInfo && MatchInfo->Phase != ETBPhase::Lobby)
	{
		Error = TEXT("Match already started. Join the next lobby.");
	}
	if (GetNumPlayers() >= 8)
	{
		Error = TEXT("Lobby is full.");
	}
}

void ATBGameMode::PostLogin(APlayerController* PlayerController)
{
	Super::PostLogin(PlayerController);
	if (auto* ToyCharacter = Cast<ATBCharacter>(PlayerController->GetPawn()))
	{
		ToyCharacter->ApplyState();
	}
}

bool ATBGameMode::IsHost(const APlayerController* PlayerController) const
{
	return PlayerController && PlayerController->IsLocalController() && PlayerController->HasAuthority();
}

// ホストからの設定変更を検証し、変更後は全員のReadyを解除する。
void ATBGameMode::SetRules(APlayerController* PlayerController, const FTBSettings& Rules)
{
	auto* MatchInfo = TB::GS(GetWorld());
	if (!IsHost(PlayerController) || !MatchInfo || MatchInfo->Phase != ETBPhase::Lobby)
	{
		return;
	}
	if (Rules.Humans < 1 || Rules.Humans > 7 || Rules.Toys < 1 || Rules.Toys > 7 || Rules.Humans + Rules.Toys > 8 ||
	    !FMath::IsFinite(Rules.Duration) || Rules.Duration < 10 || Rules.Duration > 3600 || Rules.RequiredItems < 1)
	{
		return;
	}
	int32 Items = 0;
	for (TActorIterator<ATBPickup> It(GetWorld()); It; ++It)
	{
		if (!It->bTaken)
		{
			++Items;
		}
	}
	if (Rules.RequiredItems > Items)
	{
		return;
	}
	MatchInfo->Settings = Rules;
	MatchInfo->ForceNetUpdate();
	for (APlayerState* ToyPlayerInfo : MatchInfo->PlayerArray)
	{
		if (auto* Toy = Cast<ATBPlayerState>(ToyPlayerInfo))
		{
			Toy->bReady = false;
		}
	}
}

// 人数・Ready・マップ構成を確認してから陣営を割り当て、試合を開始する。
void ATBGameMode::StartRound(APlayerController* PlayerController)
{
	auto* MatchInfo = TB::GS(GetWorld());
	auto* HostController = Cast<ATBController>(PlayerController);
	if (!IsHost(PlayerController) || !MatchInfo || MatchInfo->Phase != ETBPhase::Lobby)
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
	if (Players.Num() != MatchInfo->Settings.Humans + MatchInfo->Settings.Toys)
	{
		if (HostController)
		{
			HostController->ClientNotice(TEXT("Player count must match rules (TBRules H T Seconds Items)."));
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
		if (HostController)
		{
			HostController->ClientNotice(TEXT("Map requires exactly one box and enough items."));
		}
		return;
	}
	// Shuffle first, then prioritize Human / no preference / Toy for human slots.
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
	const float TanH = FMath::Tan(FMath::DegreesToRadians(GazeHalfAngle));
	// Fixed 90-degree horizontal FOV and 16:9 camera; inset both screen axes.
	const float HalfV =
	    FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(45.f)) / (16.f / 9.f))) - 5.f;
	const float TanV = FMath::Tan(FMath::DegreesToRadians(HalfV));
	const float SampleHeight = Toy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * .85f;
	for (float HeightOffset : {SampleHeight, 0.f, -SampleHeight})
	{
		const FVector SamplePosition = Toy->GetActorLocation() + FVector(0, 0, HeightOffset);
		const FVector Delta = SamplePosition - Eye;
		const float Depth = FVector::DotProduct(View, Delta);
		// Rectangular view frustum avoids freezing a toy above/below the visible screen.
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
		Toy->SetFrozen(Seen); // No release delay; applies on the next server evaluation.
	}
}

void ATBGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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

// 切断時は運搬・操作状態を解除し、結果を通知してから部屋を解散する。
void ATBGameMode::Logout(AController* Exiting)
{
	auto* MatchInfo = TB::GS(GetWorld());
	const bool WasPlaying = MatchInfo && MatchInfo->Phase == ETBPhase::Playing;
	int32 RemainingHumans = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PlayerController = It->Get();
		if (!PlayerController || PlayerController == Exiting)
		{
			continue;
		}
		const auto* ToyPlayerInfo = PlayerController->GetPlayerState<ATBPlayerState>();
		if (ToyPlayerInfo && ToyPlayerInfo->Team == ETBTeam::Human)
		{
			++RemainingHumans;
		}
	}
	if (auto* ToyCharacter = Cast<ATBCharacter>(Exiting->GetPawn()))
	{
		ToyCharacter->ClearHeldInteraction();
		if (ToyCharacter->Carrier)
		{
			ToyCharacter->Carrier->DropToy();
		}
		ToyCharacter->DropToy();
	}
	Super::Logout(Exiting);
	if (!WasPlaying)
	{
		return;
	}
	Finish(RemainingHumans == 0 ? ETBWinner::Toys : ETBWinner::None,
	       RemainingHumans == 0 ? TEXT("All humans disconnected") : TEXT("Player disconnected; match disbanded"),
	       RemainingHumans != 0);
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(
	    Handle,
	    FTimerDelegate::CreateWeakLambda(
	        this,
	        [this]()
	        {
		        // Tell remote peers first; allow the reliable RPC to leave before host closes.
		        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		        {
			        if (auto* PlayerController = Cast<ATBController>(It->Get()))
			        {
				        if (!PlayerController->IsLocalController())
				        {
					        PlayerController->ClientDisband(TEXT("Match closed after disconnect"));
				        }
			        }
		        }
		        FTimerHandle HostHandle;
		        GetWorldTimerManager().SetTimer(
		            HostHandle,
		            FTimerDelegate::CreateWeakLambda(this,
		                                             [this]()
		                                             {
			                                             if (auto* GI = GetGameInstance())
			                                             {
				                                             if (auto* Session = GI->GetSubsystem<UTBSession>())
				                                             {
					                                             Session->Leave();
				                                             }
			                                             }
		                                             }),
		            1.f, false);
	        }),
	    5.f, false);
}

void ATBController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
		{
			Session->Activate();
		}
	}
}

void ATBController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction("Ready", IE_Pressed, this, &ATBController::TBReady);
	InputComponent->BindAction("Start", IE_Pressed, this, &ATBController::TBStart);
}

void ATBController::TBHost()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Host();
	}
}

void ATBController::TBFind()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Find();
	}
}

void ATBController::TBJoin(int32 Index)
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Join(Index);
	}
}

void ATBController::TBLeave()
{
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Leave();
	}
}

void ATBController::TBReady()
{
	ServerReady();
}

void ATBController::TBStart()
{
	ServerStart();
}

void ATBController::TBRules(int32 HumanCount, int32 ToyCount, float Seconds, int32 Items)
{
	FTBSettings Settings;
	Settings.Humans = HumanCount;
	Settings.Toys = ToyCount;
	Settings.Duration = Seconds;
	Settings.RequiredItems = Items;
	ServerRules(Settings);
}

void ATBController::TBPrefer(int32 Team)
{
	ServerPreference(Team == 1 ? ETBTeam::Human : Team == 2 ? ETBTeam::Toy : ETBTeam::None);
}

void ATBController::ServerReady_Implementation()
{
	auto* MatchInfo = TB::GS(GetWorld());
	auto* ToyPlayerInfo = GetPlayerState<ATBPlayerState>();
	if (MatchInfo && MatchInfo->Phase == ETBPhase::Lobby && ToyPlayerInfo)
	{
		ToyPlayerInfo->bReady = !ToyPlayerInfo->bReady;
		ToyPlayerInfo->ForceNetUpdate();
	}
}

void ATBController::ServerStart_Implementation()
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->StartRound(this);
	}
}

void ATBController::ServerRules_Implementation(FTBSettings Rules)
{
	if (auto* GameMode = GetWorld()->GetAuthGameMode<ATBGameMode>())
	{
		GameMode->SetRules(this, Rules);
	}
}

void ATBController::ServerPreference_Implementation(ETBTeam Team)
{
	auto* MatchInfo = TB::GS(GetWorld());
	auto* ToyPlayerInfo = GetPlayerState<ATBPlayerState>();
	if (MatchInfo && MatchInfo->Phase == ETBPhase::Lobby && ToyPlayerInfo &&
	    (Team == ETBTeam::None || Team == ETBTeam::Human || Team == ETBTeam::Toy))
	{
		ToyPlayerInfo->Preference = Team;
		ToyPlayerInfo->bReady = false;
	}
}

void ATBController::ClientNotice_Implementation(const FString& Message)
{
	Notice = Message;
}

void ATBController::ClientDisband_Implementation(const FString& Message)
{
	Notice = Message;
	if (auto* Session = GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Session->Leave();
	}
}

// HUDは状態の表示だけを行う。勝敗や取得の判定はサーバー側で行う。
void ATBHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	float Y = 20;
	auto Line = [this, &Y](const FString& Text, FLinearColor Color = FLinearColor::White)
	{
		DrawText(Text, Color, 20, Y, nullptr, 1.f);
		Y += 22;
	};
	Line(TEXT("TOYBOX | WASD Mouse Space | E grab/hold rescue/store | Q drop | R ready | F5 start | F10 console"));
	Line(TEXT("Console: TBHost / TBFind / TBJoin 0 / TBLeave / TBRules 1 1 600 5 / TBPrefer 1(human),2(toy),0(any)"));
	auto* PlayerController = Cast<ATBController>(GetOwningPlayerController());
	if (PlayerController)
	{
		if (auto* Session = PlayerController->GetGameInstance()->GetSubsystem<UTBSession>())
		{
			Line(Session->Status, FLinearColor::Yellow);
			for (int32 Index = 0; Index < Session->Rooms.Num(); ++Index)
			{
				Line(FString::Printf(TEXT("[%d] %s"), Index, *Session->Rooms[Index]));
			}
		}
		Line(PlayerController->Notice, FLinearColor::Yellow);
	}
	if (auto* MatchInfo = TB::GS(GetWorld()))
	{
		Line(FString::Printf(TEXT("Phase %d | Humans %d / Toys %d | Time %.0f | Items %d/%d | Boxed %d"),
		                     int32(MatchInfo->Phase), MatchInfo->Settings.Humans, MatchInfo->Settings.Toys,
		                     MatchInfo->Remaining(), MatchInfo->Collected, MatchInfo->Settings.RequiredItems,
		                     MatchInfo->BoxedCount));
		if (MatchInfo->Phase == ETBPhase::Results || MatchInfo->Phase == ETBPhase::Aborted)
		{
			Line(FString::Printf(TEXT("Winner: %s | %s | TBLeave to return"),
			                     MatchInfo->Winner == ETBWinner::Humans ? TEXT("HUMANS")
			                     : MatchInfo->Winner == ETBWinner::Toys ? TEXT("TOYS")
			                                                            : TEXT("NONE"),
			                     *MatchInfo->Reason),
			     FLinearColor::Green);
		}
		for (APlayerState* LocalPlayerState : MatchInfo->PlayerArray)
		{
			if (auto* ToyPlayerInfo = Cast<ATBPlayerState>(LocalPlayerState))
			{
				Line(FString::Printf(TEXT("%s | Team:%d Pref:%d Ready:%d State:%d Frozen:%d"),
				                     *ToyPlayerInfo->GetPlayerName(), int32(ToyPlayerInfo->Team),
				                     int32(ToyPlayerInfo->Preference), ToyPlayerInfo->bReady,
				                     int32(ToyPlayerInfo->ToyState), ToyPlayerInfo->bFrozen));
			}
		}
		if (MatchInfo->Box)
		{
			Line(FString::Printf(TEXT("Rescue:%.0f%% (%d rescuers)"), MatchInfo->Box->RescueProgress * 100,
			                     MatchInfo->Box->Rescuers));
			if (MatchInfo->GetServerWorldTimeSeconds() < MatchInfo->Box->AlarmUntil)
			{
				Line(TEXT("ALARM: RESCUE STARTED AT THE BOX!"), FLinearColor::Red);
			}
		}
	}
	if (PlayerController)
	{
		if (auto* LocalPlayerState = PlayerController->GetPlayerState<ATBPlayerState>())
		{
			if (LocalPlayerState->bFrozen)
			{
				DrawText(TEXT("WATCHED - FROZEN"), FLinearColor::Red, Canvas->ClipX * .4f, Canvas->ClipY * .7f, nullptr,
				         2.f);
			}
		}
	}
	if (PlayerController)
	{
		if (auto* ToyCharacter = Cast<ATBCharacter>(PlayerController->GetPawn()))
		{
			if (ToyCharacter->ContactItem)
			{
				Line(FString::Printf(TEXT("Item: %.0f%% %s"), ToyCharacter->ItemProgress * 100,
				                     ToyCharacter->IsGazeFrozen() ? TEXT("PAUSED") : TEXT("TOUCHING")),
				     FLinearColor::Yellow);
			}
		}
	}
	DrawText(TEXT("+"), FLinearColor::White, Canvas->ClipX * .5f, Canvas->ClipY * .5f);
}

void ATBBlock::OnRep_Size()
{
	SetActorScale3D(Size);
}

void ATBBlock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBBlock, Size);
}
