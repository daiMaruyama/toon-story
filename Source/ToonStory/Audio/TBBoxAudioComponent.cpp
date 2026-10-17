#include "Audio/TBBoxAudioComponent.h"
#include "Audio/TBAudioSubsystem.h"
#include "Audio/TBAudioTypes.h"
#include "Core/TBGameHelpers.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/TBBox.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UTBBoxAudioComponent::UTBBoxAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UTBBoxAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	Box = Cast<ATBBox>(GetOwner());
	if (GetNetMode() == NM_DedicatedServer || !Box.IsValid())
	{
		SetComponentTickEnabled(false);
		return;
	}
	AddTickPrerequisiteActor(Box.Get());
}

void UTBBoxAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const auto* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	StopRescueLoop(GameInstance ? GameInstance->GetSubsystem<UTBAudioSubsystem>() : nullptr);
	Super::EndPlay(EndPlayReason);
}

void UTBBoxAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ATBBox* OwnerBox = Box.Get();
	const auto* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	auto* Audio = GameInstance ? GameInstance->GetSubsystem<UTBAudioSubsystem>() : nullptr;
	if (!OwnerBox || !Audio)
	{
		StopRescueLoop(Audio);
		return;
	}
	const auto* LocalPlayer = GameInstance->GetFirstGamePlayer();
	const auto* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	const auto* Listener = Controller ? Controller->GetPlayerState<ATBPlayerState>() : nullptr;
	const bool bDesired = TB::Playing(GetWorld()) && Listener && Listener->Team == ETBTeam::Toy &&
	                      OwnerBox->Rescuers > 0 && OwnerBox->RescueProgress > 0.f;
	const uint32 Revision = Audio->GetSettingsRevision();
	if (bDesired == bRescueLoopDesired && Revision == LastSettingsRevision)
	{
		return;
	}
	StopRescueLoop(Audio);
	bRescueLoopDesired = bDesired;
	LastSettingsRevision = Revision;
	if (bDesired)
	{
		RescueAudio = Audio->StartLoop(ETBSoundEvent::RescueLoop, OwnerBox->GetRootComponent());
	}
}

void UTBBoxAudioComponent::StopRescueLoop(UTBAudioSubsystem* Audio)
{
	if (UAudioComponent* PlayingAudio = RescueAudio.Get())
	{
		if (Audio)
		{
			Audio->StopLoop(PlayingAudio);
		}
		else
		{
			PlayingAudio->Stop();
		}
	}
	RescueAudio.Reset();
	bRescueLoopDesired = false;
}
