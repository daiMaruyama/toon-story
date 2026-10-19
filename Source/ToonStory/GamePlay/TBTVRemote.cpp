// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBTVRemote.h"
#include "Audio/TBAudioEvents.h"
#include "Audio/TBAudioSubsystem.h"
#include "Engine/GameInstance.h"
#include "TBBoxTV.h"
#include "TBTVButton.h"
#include "Character/TBCharacter.h"
#include "Data/TBPresentationData.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
// M_RemoteGlow の Btn 値。
constexpr float GlowPressed = 2.f;      // 光って沈む
constexpr float GlowAvailable = 1.f;    // 光る
constexpr float GlowUnavailable = -.6f; // 暗くする
}

ATBTVRemote::ATBTVRemote()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

void ATBTVRemote::BeginPlay()
{
	Super::BeginPlay();
	// 光り方は全員の画面で変えるので、検知はクライアントでも行う。テレビの操作はサーバーだけ。
	if (GetNetMode() != NM_DedicatedServer)
	{
		GlowMaterial = Mesh->CreateDynamicMaterialInstance(0);
	}
	GetComponents(Buttons);
	for (UTBTVButton* Button : Buttons)
	{
		Button->OnComponentBeginOverlap.AddDynamic(this, &ATBTVRemote::OnButtonOverlap);
		Button->OnComponentEndOverlap.AddDynamic(this, &ATBTVRemote::OnButtonEndOverlap);
		GlowParameters.Add(Button->GetGlowParameterName());
	}
	if (IsValid(TV))
	{
		TV->OnPowerChanged.AddUObject(this, &ATBTVRemote::UpdateGlow);
	}
	UpdateGlow();
}

void ATBTVRemote::UpdateGlow()
{
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		UpdateButtonGlow(Index);
	}
}

void ATBTVRemote::UpdateButtonGlow(int32 Index)
{
	if (!GlowMaterial || !Buttons.IsValidIndex(Index))
	{
		return;
	}
	const UTBTVButton* Button = Buttons[Index];
	// 電源OFF中に押せるのは電源ボタンだけ。押せないボタンは踏んでも沈ませない。
	const bool bUsable = (IsValid(TV) && TV->IsPowerOn()) || Button->CurrentKey == ETBRemoteKey::Power;
	TArray<AActor*> Riders;
	Button->GetOverlappingActors(Riders, ATBCharacter::StaticClass());
	const float State = !bUsable ? GlowUnavailable : Riders.Num() > 0 ? GlowPressed : GlowAvailable;
	GlowMaterial->SetScalarParameterValue(GlowParameters[Index], State);
}

void ATBTVRemote::OnButtonEndOverlap(UPrimitiveComponent* Component, AActor*, UPrimitiveComponent*, int32)
{
	UpdateButtonGlow(Buttons.IndexOfByKey(Component));
}

void ATBTVRemote::OnButtonOverlap(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent*, int32, bool,
                                  const FHitResult&)
{
	UpdateButtonGlow(Buttons.IndexOfByKey(Component));
	if (!HasAuthority())
	{
		return;
	}
	const auto* Button = Cast<UTBTVButton>(Component);
	auto* Viewer = Cast<ATBCharacter>(OtherActor);
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Button || !Viewer || !IsValid(TV) || (LastPressTime >= 0 && Now - LastPressTime < TB::PresentationData().Remote.PressCooldown))
	{
		return;
	}
	// 誰も乗っていないボタンに乗ったときだけ押す。通知時点で本人は一覧に含まれている。
	TArray<AActor*> Riders;
	Button->GetOverlappingActors(Riders, ATBCharacter::StaticClass());
	if (Riders.Num() > 1)
	{
		return;
	}
	bool bHandled = false;
	switch (Button->CurrentKey)
	{
		case ETBRemoteKey::Digit:
			bHandled = TV->SelectChannel(Viewer, Button->Digit);
			break;
		case ETBRemoteKey::Next:
			bHandled = TV->StepChannel(Viewer, 1);
			break;
		case ETBRemoteKey::Prev:
			bHandled = TV->StepChannel(Viewer, -1);
			break;
		case ETBRemoteKey::Power:
			bHandled = TV->TogglePower(Viewer);
			break;
	}
	// 断られた操作（電源OFF中の番号など）では連打防止を始めない。
	if (bHandled)
	{
		LastPressTime = Now;
		UTBAudioEvents::Emit(this, ETBSoundEvent::TVButton, nullptr, FVector::ZeroVector);
	}
}
