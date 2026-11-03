// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBTitleGameMode.h"
#include "Input/TBInputSettings.h"
#include "Core/TBSession.h"
#include "UI/TBTitleMenu.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Core/TBTitleEmote.h"
#include "Character/TBToyPresentation.h"
#include "EngineUtils.h"

namespace
{
	const FName TitleToyTag(TEXT("TitleToy"));
}

ATBTitleGameMode::ATBTitleGameMode()
{
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;
	// Keep the base empty HUD: ClientSetHUD attempts SpawnActor even for nullptr.
	PlayerControllerClass = ATBTitleController::StaticClass();
}

void ATBTitleController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	// 子ども部屋には寸劇用のカメラも並ぶので、タイトル用の印が付いたカメラを先に選ぶ。
	ACameraActor* Camera = nullptr;
	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("TitleCamera")))
		{
			Camera = *It;
			break;
		}
		if (!Camera) Camera = *It;
	}
	if (Camera) SetViewTarget(Camera);
	// ループで踊るおもちゃに、ゲームと同じエモートを出させる。メニューが寸劇を流し始める前に整える。
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TitleToyTag) && !It->FindComponentByClass<UTBTitleEmote>())
		{
			auto* Emote = NewObject<UTBTitleEmote>(*It);
			Emote->RegisterComponent();
			TBToyPresentation::UseLookPose(*It);
		}
	}
	GetGameInstance()->GetSubsystem<UTBSession>()->Activate();
	Menu = CreateWidget<UTBTitleMenu>(this, UTBTitleMenu::StaticClass());
	if (Menu)
	{
		Menu->AddToViewport();
		FInputModeGameAndUI Input;
		Input.SetWidgetToFocus(Menu->TakeWidget());
		Input.SetHideCursorDuringCapture(false);
		Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Input);
	}
	bShowMouseCursor = true;
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void ATBTitleController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ATBTitleController::ClickToyPressed);
}

bool ATBTitleController::ClickToy()
{
	if (!Menu || Menu->GetState() != UTBTitleMenu::EState::Menu) return false;
	FVector Origin, Direction;
	if (!DeprojectMousePositionToWorld(Origin, Direction)) return false;
	UTBTitleEmote* Toy = PickToy(Origin, Direction);
	return Toy && Toy->Next();
}

UTBTitleEmote* ATBTitleController::PickToy(const FVector& Origin, const FVector& Direction) const
{
	const FVector End = Origin + Direction * 100000.;
	UTBTitleEmote* Best = nullptr;
	double BestMiss = TNumericLimits<double>::Max();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		auto* Emote = It->ActorHasTag(TitleToyTag) ? It->FindComponentByClass<UTBTitleEmote>() : nullptr;
		if (!Emote) continue;
		FVector Center, Extent, Hit, Normal;
		It->GetActorBounds(true, Center, Extent);
		if (Extent.IsNearlyZero()) It->GetActorBounds(false, Center, Extent);
		float Time = 0.f;
		if (!FMath::LineExtentBoxIntersection(FBox(Center - Extent, Center + Extent), Origin, End, FVector::ZeroVector, Hit, Normal, Time)) continue;
		// 箱は重なりやすいので、クリックした線に体の中心がいちばん近いおもちゃを選ぶ（大きさで割って比べる）。
		const FVector ToCenter = Center - Origin;
		const double Miss = (ToCenter - Direction * (ToCenter | Direction)).Size() / FMath::Max(Extent.Size(), 1.);
		if (Miss >= BestMiss) continue;
		// 壁や家具の向こうにいるおもちゃは選ばない。
		FHitResult Block;
		FCollisionQueryParams Params(TEXT("TitleToyClick"), false, *It);
		if (GetWorld()->LineTraceSingleByChannel(Block, Origin, Hit, ECC_Visibility, Params)) continue;
		Best = Emote;
		BestMiss = Miss;
	}
	return Best;
}

// タイトルのキー設定でも使うので。
void ATBTitleController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (IsLocalController())
	{
		UTBInputSettings::RegisterContexts(GetLocalPlayer());
	}
}

void ATBTitleController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Menu) Menu->RemoveFromParent();
	Super::EndPlay(Reason);
}

bool ATBTitleController::IsStoryPlaying() const
{
	return Menu && Menu->GetState() == UTBTitleMenu::EState::Story;
}

void ATBTitleController::TBHost() { GetGameInstance()->GetSubsystem<UTBSession>()->Host(); }
void ATBTitleController::TBFind() { GetGameInstance()->GetSubsystem<UTBSession>()->Find(); }
void ATBTitleController::TBJoin(int32 Index) { GetGameInstance()->GetSubsystem<UTBSession>()->Join(Index); }
void ATBTitleController::TBLeave() { GetGameInstance()->GetSubsystem<UTBSession>()->Leave(); }
