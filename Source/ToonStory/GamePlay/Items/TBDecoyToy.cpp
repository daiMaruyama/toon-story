#include "GamePlay/Items/TBDecoyToy.h"
#include "Audio/TBAudioSubsystem.h"
#include "Audio/TBCharacterAudioComponent.h"
#include "Character/TBCharacter.h"
#include "Character/TBToyCharacter.h"
#include "Character/TBToyPresentation.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBPlayerState.h"
#include "Data/TBDecoyToyData.h"
#include "Data/TBCharacterData.h"
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
	if (HasAuthority() && FMath::IsFinite(DeltaSeconds) && DeltaSeconds>0)
	{
		const float Dt=FMath::Min(DeltaSeconds,.1f);
		const float Speed=FMath::IsFinite(Data().WalkSpeed) ? FMath::Max(0.f,Data().WalkSpeed) : 0.f;
		const FVector From=GetActorLocation();
		const FVector Delta=bWalking && TB::Playing(GetWorld()) ? GetActorForwardVector()*Speed*Dt : FVector::ZeroVector;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DecoyMotion),false,this);
		const FVector Offset(0,0,10);
		const auto Shape=FCollisionShape::MakeSphere(8.f);
		const float ConfiguredStep=TB::CharacterData().Toy.MaxStepHeight;
		const float StepHeight=FMath::IsFinite(ConfiguredStep) ? FMath::Clamp(ConfiguredStep,0.f,100.f) : 0.f;
		if (!Delta.IsNearlyZero() && GetWorld()->SweepSingleByChannel(Hit,From+Offset,From+Offset+Delta,FQuat::Identity,ECC_WorldDynamic,Shape,Params))
		{
			// 段差分だけ上げた経路と頭上が空いていれば、前進してから床へ着地する。
			const FVector Raise(0,0,StepHeight);
			FHitResult UpHit, ForwardHit, FloorHit;
			const bool bCanStep=bGrounded && StepHeight>0 &&
			    !GetWorld()->SweepSingleByChannel(UpHit,From+Offset,From+Offset+Raise,FQuat::Identity,ECC_WorldDynamic,Shape,Params) &&
			    !GetWorld()->SweepSingleByChannel(ForwardHit,From+Offset+Raise,From+Offset+Raise+Delta,FQuat::Identity,ECC_WorldDynamic,Shape,Params) &&
			    GetWorld()->SweepSingleByChannel(FloorHit,From+Offset+Raise+Delta,From+Offset+Delta-FVector(0,0,2),FQuat::Identity,ECC_WorldDynamic,Shape,Params) &&
			    FloorHit.ImpactNormal.Z>=.7f;
			if (bCanStep)
			{
				SetActorLocation(FloorHit.Location-FVector(0,0,8)+FVector(0,0,.1f));
				VerticalSpeed=0;
			}
			else
			{
				SetActorLocation(From+Delta*FMath::Max(0.f,Hit.Time-.01f));
				FVector Normal=Hit.ImpactNormal; Normal.Z=0; Normal.Normalize();
				FVector Next=FMath::GetReflectionVector(GetActorForwardVector(),Normal); Next.Z=0;
				if (Normal.IsNearlyZero() || Next.IsNearlyZero()) Next=-GetActorForwardVector();
				SetActorRotation(Next.Rotation());
			}
		}
		else SetActorLocation(From+Delta);

		// 停止中にも重力を適用する。スイープで床を検出し、空中では実際に落下する。
		const FVector Position=GetActorLocation();
		const float Gravity=FMath::IsFinite(GetWorld()->GetGravityZ()) ? FMath::Min(0.f,GetWorld()->GetGravityZ()) : -980.f;
		VerticalSpeed=FMath::Max(-4000.f,VerticalSpeed+Gravity*Dt);
		const float Down=FMath::Max(2.f,-VerticalSpeed*Dt)+(bGrounded ? StepHeight : 0.f);
		const FVector Start=Position+FVector(0,0,8.1f);
		FHitResult Floor;
		const bool bFloor=GetWorld()->SweepSingleByChannel(Floor,Start,Start-FVector(0,0,Down),FQuat::Identity,ECC_WorldDynamic,Shape,Params) && Floor.ImpactNormal.Z>=.7f;
		if (bFloor)
		{
			SetActorLocation(Floor.Location-FVector(0,0,8)+FVector(0,0,.1f));
			VerticalSpeed=0; bGrounded=true;
		}
		else { SetActorLocation(Position+FVector(0,0,VerticalSpeed*Dt)); bGrounded=false; }
	}
	if (GetNetMode()==NM_DedicatedServer) return;
	const FVector Location=GetActorLocation();
	const float Distance=bHasSample ? FVector::Dist2D(Location,PreviousLocation) : 0.f;
	TBToyPresentation::Animate(Visual->GetChildActor(),bWalking ? Data().WalkSpeed : 0.f,!bWalking);
	TBToyPresentation::Ground(Visual,Location.Z);
	if (bWalking && bGrounded && DeltaSeconds>0 && DeltaSeconds<=.25f && Distance<100.f)
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
	DOREPLIFETIME(ATBDecoyToy,bGrounded);
}
