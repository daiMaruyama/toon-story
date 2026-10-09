#include "TBTVRemote.h"
#include "TBBoxTV.h"
#include "TBTVButton.h"
#include "Character/TBCharacter.h"
#include "Components/StaticMeshComponent.h"

ATBTVRemote::ATBTVRemote()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
}

void ATBTVRemote::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	TArray<UTBTVButton*> Buttons;
	GetComponents(Buttons);
	for (auto* Button : Buttons)
	{
		Button->OnComponentBeginOverlap.AddDynamic(this, &ATBTVRemote::OnButtonOverlap);
	}
}

void ATBTVRemote::OnButtonOverlap(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent*, int32, bool,
                                  const FHitResult&)
{
	const auto* Button = Cast<UTBTVButton>(Component);
	auto* Viewer = Cast<ATBCharacter>(OtherActor);
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Button || !Viewer || !IsValid(TV) || (LastPressTime >= 0 && Now - LastPressTime < PressCooldown))
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
	}
}
