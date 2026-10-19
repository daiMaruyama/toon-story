#include "GamePlay/Items/TBNoiseRadio.h"
#include "Audio/TBAudioSubsystem.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBPlayerState.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/ConstructorHelpers.h"

ATBNoiseRadio::ATBNoiseRadio()
{
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;
	DisplayName = NSLOCTEXT("TBItems", "NoiseRadio", "Noise Radio");
	Actions.Reset();
	FTBItemAction Start;
	Start.Id = TEXT("Start");
	Start.Label = NSLOCTEXT("TBItems", "RadioStart", "Turn on");
	Start.AllowedTeams = ETBInteractionTeams::ToyOnly;
	Start.MaxDistance = 350.f;
	Actions.Add(Start);
	FTBItemAction Stop = Start;
	Stop.Id = TEXT("Stop");
	Stop.Label = NSLOCTEXT("TBItems", "RadioStop", "Turn off");
	Stop.AllowedTeams = ETBInteractionTeams::HumanOnly;
	Actions.Add(Stop);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Mesh->SetStaticMesh(Cube.Object);
	Mesh->SetRelativeLocation(FVector(0, 0, 35));
	Mesh->SetRelativeScale3D(FVector(.35f, .8f, .6f));
	InteractionPoint->SetRelativeLocation(FVector(20, 0, 35));
	Speaker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Speaker"));
	Speaker->SetupAttachment(Root);
	Speaker->SetStaticMesh(Cylinder.Object);
	Speaker->SetRelativeLocation(FVector(19, -14, 35));
	Speaker->SetRelativeRotation(FRotator(90, 0, 0));
	Speaker->SetRelativeScale3D(FVector(.4f, .4f, .04f));
	Speaker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	auto* Dial = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dial"));
	Dial->SetupAttachment(Root);
	Dial->SetStaticMesh(Cylinder.Object);
	Dial->SetRelativeLocation(FVector(20, 24, 42));
	Dial->SetRelativeRotation(FRotator(90, 0, 0));
	Dial->SetRelativeScale3D(FVector(.12f, .12f, .06f));
	Dial->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	auto* Antenna = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Antenna"));
	Antenna->SetupAttachment(Root);
	Antenna->SetStaticMesh(Cylinder.Object);
	Antenna->SetRelativeLocation(FVector(0, 27, 90));
	Antenna->SetRelativeRotation(FRotator(0, 0, 15));
	Antenna->SetRelativeScale3D(FVector(.025f, .025f, .6f));
	Antenna->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatusLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Status"));
	StatusLabel->SetupAttachment(Root);
	StatusLabel->SetRelativeLocation(FVector(23, 0, 78));
	StatusLabel->SetHorizontalAlignment(EHTA_Center);
	StatusLabel->SetWorldSize(9);
	StatusLabel->SetText(FText::FromString(TEXT("RADIO - Toy: E to start")));
	RadioAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RadioAudio"));
	RadioAudio->SetupAttachment(Root);
	RadioAudio->bAutoActivate = false;
	RadioAudio->bAutoDestroy = false;
	RadioAudio->bStopWhenOwnerDestroyed = true;
}

void ATBNoiseRadio::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!bUsePrototypeColors) return;
	TInlineComponentArray<UStaticMeshComponent*> Parts(this);
	for (auto* Part : Parts)
	{
		if (auto* Material = Part->CreateDynamicMaterialInstance(0))
		{
			const FLinearColor Color = Part == Mesh ? FLinearColor(.02f, .3f, .4f) :
			    Part == Speaker ? FLinearColor(.035f, .035f, .04f) : FLinearColor(.8f, .4f, .05f);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

void ATBNoiseRadio::BeginPlay()
{
	Super::BeginPlay();
	OnRep_RadioOn();
}

bool ATBNoiseRadio::CanPerformAction_Implementation(const FTBInteractionContext& Context) const
{
	const auto* Player = Context.InstigatorCharacter ? Context.InstigatorCharacter->TBPS() : nullptr;
	return TB::Playing(GetWorld()) && Player &&
	    ((!bRadioOn && Context.ActionId == TEXT("Start") && Player->Team == ETBTeam::Toy) ||
	     (bRadioOn && Context.ActionId == TEXT("Stop") && Player->Team == ETBTeam::Human));
}

bool ATBNoiseRadio::PerformAction_Implementation(const FTBInteractionContext& Context)
{
	bRadioOn = Context.ActionId == TEXT("Start");
	ForceNetUpdate();
	OnRep_RadioOn();
	return true;
}

float ATBNoiseRadio::GetMaskAtLocation(const FVector& ListenerLocation) const
{
	if (!bRadioOn || !TB::Playing(GetWorld()) || !FMath::IsFinite(MaskRadius) || MaskRadius <= 0.f) return 1.f;
	const float DistanceAlpha = FMath::Clamp(FVector::Dist(ListenerLocation, GetActorLocation()) / MaskRadius, 0.f, 1.f);
	const float Minimum = FMath::IsFinite(ToyFootstepMultiplier) ? FMath::Clamp(ToyFootstepMultiplier, 0.f, 1.f) : 1.f;
	return FMath::Lerp(Minimum, 1.f, DistanceAlpha);
}

void ATBNoiseRadio::OnRep_RadioOn()
{
	StatusLabel->SetText(FText::FromString(bRadioOn ? TEXT("ON - Human: E to stop") : TEXT("OFF - Toy: E to start")));
	StatusLabel->SetTextRenderColor(bRadioOn ? FColor::Green : FColor::White);
	UpdateLocalPlayback();
}

void ATBNoiseRadio::BuildPlaceholderSound()
{
	// Original two-second, eight-note tune. Mono PCM avoids external content and licensing dependencies.
	constexpr int32 Rate = 24000;
	constexpr int32 Samples = Rate * 2;
	PlaceholderPCM.SetNumUninitialized(Samples * sizeof(int16));
	const float Notes[] = {261.63f, 329.63f, 392.f, 523.25f, 392.f, 329.63f, 293.66f, 392.f};
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const int32 BeatSample = Index % (Rate / 4);
		const float Time = static_cast<float>(BeatSample) / Rate;
		const float Envelope = FMath::Min(Time / .01f, 1.f) * FMath::Clamp((.25f - Time) / .04f, 0.f, 1.f);
		const float Phase = 2.f * PI * Notes[Index / (Rate / 4)] * Time;
		const int16 Sample = static_cast<int16>(6500.f * Envelope * (FMath::Sin(Phase) + .25f * FMath::Sin(Phase * 2.f)));
		FMemory::Memcpy(PlaceholderPCM.GetData() + Index * sizeof(int16), &Sample, sizeof(int16));
	}
	PlaceholderSound = NewObject<USoundWaveProcedural>(this);
	PlaceholderSound->SetSampleRate(Rate);
	PlaceholderSound->NumChannels = 1;
	PlaceholderSound->Duration = INDEFINITELY_LOOPING_DURATION;
	PlaceholderSound->SoundGroup = SOUNDGROUP_Effects;
	PlaceholderSound->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
}

void ATBNoiseRadio::UpdateLocalPlayback()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	auto* GI = GetGameInstance();
	auto* Audio = GI ? GI->GetSubsystem<UTBAudioSubsystem>() : nullptr;
	const bool bDesired = bRadioOn && TB::Playing(GetWorld());
	if (!bDesired)
	{
		if (!bLocalPlaying) return;
		RadioAudio->Stop();
		if (PlaceholderSound) PlaceholderSound->ResetAudio();
		if (Audio) Audio->RemoveToyActionMask(this);
		bLocalPlaying = false;
		return;
	}
	if (!bLocalPlaying)
	{
		if (!LoopSound && !PlaceholderSound) BuildPlaceholderSound();
		RadioAudio->SetSound(LoopSound ? LoopSound.Get() : PlaceholderSound.Get());
		FSoundAttenuationSettings Attenuation;
		Attenuation.bAttenuate = true;
		Attenuation.bSpatialize = true;
		Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
		Attenuation.AttenuationShapeExtents = FVector::ZeroVector;
		Attenuation.FalloffDistance = FMath::IsFinite(AudibleDistance) ? FMath::Max(1.f, AudibleDistance) : 1800.f;
		RadioAudio->AdjustAttenuation(Attenuation);
		bLocalPlaying = true;
	}
	if (!LoopSound && PlaceholderSound && PlaceholderSound->GetAvailableAudioByteCount() < PlaceholderPCM.Num())
	{
		PlaceholderSound->QueueAudio(PlaceholderPCM.GetData(), PlaceholderPCM.Num());
	}
	RadioAudio->SetVolumeMultiplier((FMath::IsFinite(RadioVolume) ? FMath::Clamp(RadioVolume, 0.f, 4.f) : 0.f) *
	    (Audio ? Audio->GetCategoryVolume(ETBAudioCategory::Effects) : 1.f));
	if (!RadioAudio->IsPlaying()) RadioAudio->Play();
	const auto* PC = GI ? GI->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (Audio) Audio->SetToyActionMask(this, Pawn ? GetMaskAtLocation(Pawn->GetActorLocation()) : 1.f);
}

void ATBNoiseRadio::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && bRadioOn && !TB::Playing(GetWorld()))
	{
		bRadioOn = false;
		ForceNetUpdate();
		OnRep_RadioOn();
	}
	UpdateLocalPlayback();
	const float Pulse = bRadioOn && bAnimateSpeaker ? FMath::Sin(GetWorld()->GetTimeSeconds() * 24.f) * .035f : 0.f;
	Speaker->SetRelativeScale3D(FVector(.4f + Pulse, .4f + Pulse, .04f));
}

void ATBNoiseRadio::EndPlay(const EEndPlayReason::Type Reason)
{
	RadioAudio->Stop();
	if (auto* GI = GetGameInstance())
	{
		if (auto* Audio = GI->GetSubsystem<UTBAudioSubsystem>()) Audio->RemoveToyActionMask(this);
	}
	Super::EndPlay(Reason);
}

void ATBNoiseRadio::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBNoiseRadio, bRadioOn);
}
