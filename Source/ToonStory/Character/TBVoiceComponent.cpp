#include "Character/TBVoiceComponent.h"
#include "Core/TBPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

void UTBVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// PlayerStateの到着待ちと、ラウンドごとの陣営の変化に追従するため定期チェック
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &UTBVoiceComponent::UpdateVoice, .5f, true, 0.f);
}

void UTBVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	Super::EndPlay(EndPlayReason);
}

void UTBVoiceComponent::UpdateVoice()
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	auto* Speaker = Pawn ? Pawn->GetPlayerState<ATBPlayerState>() : nullptr;
	if (!Speaker || !Speaker->GetUniqueId().IsValid())
	{
		return;
	}
	// 自分の声は自分のPCでは鳴らさない。
	if (Pawn->IsLocallyControlled())
	{
		GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
		return;
	}
	if (!Attenuation)
	{
		Attenuation = NewObject<USoundAttenuation>(this);
		Attenuation->Attenuation.AttenuationShapeExtents = FVector::ZeroVector; // // 既定の全音量域（半径400）を消す。
		Settings.ComponentToAttachTo = Pawn->GetRootComponent();
		Settings.AttenuationSettings = Attenuation;
		RegisterWithPlayerState(Speaker);
	}
	const auto* Controller = GetWorld()->GetFirstPlayerController();
	const auto* Listener = Controller ? Controller->GetPlayerState<ATBPlayerState>() : nullptr;
	const bool bToyToHuman = Speaker->Team == ETBTeam::Toy && Listener && Listener->Team == ETBTeam::Human;
	Attenuation->Attenuation.FalloffDistance = bToyToHuman ? ToyToHumanHearingDistance : HearingDistance;
}
