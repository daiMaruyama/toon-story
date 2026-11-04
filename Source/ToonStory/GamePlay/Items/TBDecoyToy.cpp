#include "GamePlay/Items/TBDecoyToy.h"
#include "Audio/TBAudioSubsystem.h"
#include "Audio/TBCharacterAudioComponent.h"
#include "Character/TBCharacter.h"
#include "Character/TBToyCharacter.h"
#include "Character/TBToyPresentation.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBPlayerState.h"
#include "Data/TBDecoyToyData.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATBDecoyToy::ATBDecoyToy()
{
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	DisplayName = NSLOCTEXT("TBItems", "DecoyToy", "ニセおもちゃ");
	Actions.Reset();
	FTBItemAction Start; Start.Id=TEXT("Start"); Start.Label=NSLOCTEXT("TBItems", "DecoyStart", "起動"); Start.AllowedTeams=ETBInteractionTeams::ToyOnly;
	FTBItemAction Stop=Start; Stop.Id=TEXT("Stop"); Stop.Label=NSLOCTEXT("TBItems", "DecoyStop", "停止"); Stop.AllowedTeams=ETBInteractionTeams::HumanOnly;
	Actions={Start,Stop};
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetRelativeLocation(FVector(0,0,3));
	Mesh->SetRelativeScale3D(FVector(.12f,.12f,.06f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetHiddenInGame(true);
	InteractionPoint->SetRelativeLocation(FVector(0,0,12));
	Visual=CreateDefaultSubobject<UChildActorComponent>(TEXT("ToyVisual"));
	Visual->SetupAttachment(Root);
	Visual->SetRelativeScale3D(FVector(TBToyPresentation::SizeScale));
	DecoyData=TSoftObjectPtr<UTBDecoyToyData>(FSoftObjectPath(TEXT("/Game/Data/DA_DecoyToy.DA_DecoyToy")));
}
const UTBDecoyToyData& ATBDecoyToy::Data() const { return LoadedData ? *LoadedData : *GetDefault<UTBDecoyToyData>(); }
void ATBDecoyToy::BeginPlay()
{
	Super::BeginPlay();
	LoadedData=DecoyData.LoadSynchronous();
	for (auto& Action:Actions) Action.MaxDistance=FMath::Max(1.f,Data().InteractionDistance);
	if (HasAuthority())
	{
		const auto* Mode=GetWorld()->GetAuthGameMode();
		const auto* Defaults=Mode && Mode->DefaultPawnClass ? Cast<ATBToyCharacter>(Mode->DefaultPawnClass->GetDefaultObject()) : nullptr;
		if (Defaults)
		{
			const int32 Index=TBToyPresentation::SelectToy(Defaults->ToyCatalogue);
			if (Index!=INDEX_NONE) VisualClass=Defaults->ToyCatalogue[Index];
		}
	}
	OnRep_Visual();
}
void ATBDecoyToy::OnRep_Visual()
{
	if (GetNetMode()==NM_DedicatedServer) return;
	Visual->SetChildActorClass(VisualClass);
	if (auto* Child=Visual->GetChildActor()) Child->SetActorTickEnabled(false);
	TBToyPresentation::Initialize(Visual->GetChildActor());
}
bool ATBDecoyToy::CanPerformAction_Implementation(const FTBInteractionContext& Context) const
{
	return TB::Playing(GetWorld()) && ((Context.ActionId==TEXT("Start") && !bWalking) || (Context.ActionId==TEXT("Stop") && bWalking));
}
bool ATBDecoyToy::PerformAction_Implementation(const FTBInteractionContext& Context)
{
	bWalking=Context.ActionId==TEXT("Start"); ForceNetUpdate(); return true;
}
void ATBDecoyToy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && bWalking && !TB::Playing(GetWorld())) { bWalking=false; ForceNetUpdate(); }
	if (HasAuthority() && bWalking && TB::Playing(GetWorld()) && FMath::IsFinite(DeltaSeconds) && DeltaSeconds>0)
	{
		const float Speed=FMath::IsFinite(Data().WalkSpeed) ? FMath::Max(0.f,Data().WalkSpeed) : 0.f;
		const FVector From=GetActorLocation();
		const FVector Delta=GetActorForwardVector()*Speed*FMath::Min(DeltaSeconds,.1f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DecoyMotion),false,this);
		const FVector Offset(0,0,10);
		if (GetWorld()->SweepSingleByChannel(Hit,From+Offset,From+Offset+Delta,FQuat::Identity,ECC_WorldDynamic,FCollisionShape::MakeSphere(8.f),Params))
		{
			SetActorLocation(From+Delta*FMath::Max(0.f,Hit.Time-.01f));
			FVector Normal=Hit.ImpactNormal; Normal.Z=0; Normal.Normalize();
			FVector Next=FMath::GetReflectionVector(GetActorForwardVector(),Normal); Next.Z=0;
			if (Normal.IsNearlyZero() || Next.IsNearlyZero()) Next=-GetActorForwardVector();
			SetActorRotation(Next.Rotation());
		}
		else SetActorLocation(From+Delta);
	}
	if (GetNetMode()==NM_DedicatedServer) return;
	const FVector Location=GetActorLocation();
	const float Distance=bHasSample ? FVector::Dist2D(Location,PreviousLocation) : 0.f;
	TBToyPresentation::Animate(Visual->GetChildActor(),bWalking ? Data().WalkSpeed : 0.f,!bWalking);
	TBToyPresentation::Ground(Visual,Location.Z);
	if (bWalking && DeltaSeconds>0 && DeltaSeconds<=.25f && Distance<100.f)
	{
		StepDistance+=Distance;
		const float Interval=FMath::Max(1.f,GetDefault<UTBCharacterAudioComponent>()->ToyStepDistance);
		if (StepDistance>=Interval)
		{
			StepDistance=FMath::Fmod(StepDistance,Interval);
			if (auto* GI=GetGameInstance()) if (auto* Audio=GI->GetSubsystem<UTBAudioSubsystem>()) Audio->PlaySoundEvent(ETBSoundEvent::Footstep,Location);
		}
	}
	else StepDistance=0;
	PreviousLocation=Location; bHasSample=true;
}
void ATBDecoyToy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBDecoyToy,VisualClass);
	DOREPLIFETIME(ATBDecoyToy,bWalking);
}
