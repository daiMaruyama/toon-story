#include "Character/TBVoiceComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

void UTBVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// PlayerStateはキャラより遅れて届くことがあるので、揃うまで待つ。
	GetWorld()->GetTimerManager().SetTimer(RegisterTimer, this, &UTBVoiceComponent::TryRegister, .5f, true, 0.f);
}

void UTBVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(RegisterTimer);
	Super::EndPlay(EndPlayReason);
}

void UTBVoiceComponent::TryRegister()
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	if (!PlayerState || !PlayerState->GetUniqueId().IsValid())
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(RegisterTimer);
	// 自分の声は自分のPCでは鳴らさない。
	if (Pawn->IsLocallyControlled())
	{
		return;
	}
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(InnerRadius, 0, 0);
	Attenuation->Attenuation.FalloffDistance = FalloffDistance;
	Settings.ComponentToAttachTo = Pawn->GetRootComponent();
	Settings.AttenuationSettings = Attenuation;
	RegisterWithPlayerState(PlayerState);
}
