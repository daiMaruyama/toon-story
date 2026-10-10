#include "TBToyCharacter.h"
#include "TBCharacterAppearanceComponent.h"
#include "TBToyAnimationComponent.h"
#include "Data/TBToyDefinition.h"
#include "Core/TBPlayerState.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

ATBToyCharacter::ATBToyCharacter(const FObjectInitializer& Init) : Super(Init)
{
	Appearance = CreateDefaultSubobject<UTBCharacterAppearanceComponent>(TEXT("ToyAppearance"));
	ToyAnimation = CreateDefaultSubobject<UTBToyAnimationComponent>(TEXT("ToyAnimation"));
	// Defaults for this collection. Blueprint subclasses can replace the catalogue in the editor.
	for (const TCHAR* Name : {TEXT("SpaceRanger_Realistic"), TEXT("Duck_Realistic"), TEXT("TinRobot_Realistic"),
	                         TEXT("TeddyBear_Realistic"), TEXT("Dinosaur_Realistic"), TEXT("ArmyMan_StarterPack"), TEXT("Nutcracker")})
	{
		ToyCatalogue.Add(TSoftObjectPtr<UTBToyDefinition>(FSoftObjectPath(
			FString::Printf(TEXT("/Game/Characters/ToyDefinitions/DA_%s.DA_%s"), Name, Name))));
	}
	ToyDefinition = ToyCatalogue[0];
}

bool ATBToyCharacter::LoadToyDefinition()
{
	const auto Path = ToyCatalogue.IsValidIndex(SelectedToyIndex) ? ToyCatalogue[SelectedToyIndex] : ToyDefinition;
	auto* Definition = Path.LoadSynchronous();
	if (!Definition || (!Definition->SkeletalMesh && !Definition->StaticMesh)) return false;
	ToyDefinition = Path;
	Appearance->SetDefinition(Definition);
	ToySpeed = Definition->WalkSpeed > 0.f ? Definition->WalkSpeed : GetDefault<ATBToyCharacter>()->ToySpeed;
	ToySprintSpeed = Definition->RunSpeed > 0.f ? Definition->RunSpeed : GetDefault<ATBToyCharacter>()->ToySprintSpeed;
	// TBCharacter's private animation code stays untouched and inactive for these toys.
	// Its movement, child appearance, carry camera and newly merged camera blending still run normally.
	ToyMesh.Reset();
	ToySkeletalMesh.Reset();
	ToyIdleAnimation.Reset();
	ToyWalkAnimation.Reset();
	ToyJumpAnimation.Reset();
	bPresentationReady = true;
	return true;
}

void ATBToyCharacter::BeginPlay()
{
	LoadToyDefinition(); // Missing optional Content assets retain TBCharacter's original fallback.
	Super::BeginPlay();
	RefreshToyPresentation();
}

void ATBToyCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshToyPresentation();
}

void ATBToyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Reconcile after the base has applied replicated game state. No duplicate game rules or camera logic.
	RefreshToyPresentation();
}

void ATBToyCharacter::RefreshToyPresentation()
{
	if (!bPresentationReady || !TBPS()) return;
	const bool bToy = TBPS()->Team == ETBTeam::Toy;
	Appearance->Apply(*this, *ToyAnimation, bToy);
	if (bToy) ToyAnimation->Update(GetVelocity().Size2D(), GetCharacterMovement()->IsFalling(), IsMovementLocked());
}

UTBToyDefinition* ATBToyCharacter::GetToyDefinition() const
{
	return Appearance->GetDefinition();
}

void ATBToyCharacter::ServerSelectToy_Implementation(int32 Index)
{
#if !UE_BUILD_SHIPPING
	const auto* Info = TBPS();
	if (!Info || Info->Team != ETBTeam::Toy || Info->ToyState != ETBToyState::Free || IsMovementLocked() ||
	    !ToyCatalogue.IsValidIndex(Index) || Index == SelectedToyIndex) return;
	auto* Definition = ToyCatalogue[Index].LoadSynchronous();
	if (!Definition || (!Definition->SkeletalMesh && !Definition->StaticMesh)) return;
	SelectedToyIndex = Index;
	OnRep_SelectedToyIndex();
	ForceNetUpdate();
#endif
}

void ATBToyCharacter::OnRep_SelectedToyIndex()
{
	if (LoadToyDefinition())
	{
		ApplyState();
		RefreshToyPresentation();
	}
}

void ATBToyCharacter::NextToy()
{
#if !UE_BUILD_SHIPPING
	if (ToyCatalogue.IsEmpty()) return;
	const int32 Current = SelectedToyIndex == INDEX_NONE ? ToyCatalogue.IndexOfByKey(ToyDefinition) : SelectedToyIndex;
	ServerSelectToy((Current + 1) % ToyCatalogue.Num());
#endif
}

void ATBToyCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
#if !UE_BUILD_SHIPPING
	Input->BindKey(EKeys::F6, IE_Pressed, this, &ATBToyCharacter::NextToy);
#endif
}

void ATBToyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBToyCharacter, SelectedToyIndex);
}
