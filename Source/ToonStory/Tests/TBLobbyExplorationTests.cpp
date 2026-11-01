#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBCharacter.h"
#include "Character/TBToyPresentation.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "UI/TBHUD.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLobbyExplorationTest, "ToonStory.Lobby.Exploration",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBLobbyExplorationTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World() && Context.World()->GetNetMode() == NM_Standalone) World = Context.World();
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	auto* Pawn = PC ? PC->GetPawn<ATBCharacter>() : nullptr;
	auto* Info = PC ? PC->GetPlayerState<ATBPlayerState>() : nullptr;
	auto* State = World ? World->GetGameState<ATBGameState>() : nullptr;
	auto* HUD = PC ? Cast<ATBHUD>(PC->GetHUD()) : nullptr;
	if (!Pawn || !Info || !State || !HUD || !PC->HasAuthority()) return false;
	const auto OldPhase = State->Phase; const auto OldTeam = Info->Team;
	const auto OldPreparation = State->HidePreparation; const auto OldEnd = State->EndTime;
	const bool OldPending = State->bTravelPending;
	const FTransform OldTransform = Pawn->GetActorTransform();
	const bool OldSettings = HUD->IsLobbySettingsOpen();
	auto* Floor = World->SpawnActor<AActor>();
	auto* Surface = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Surface); Surface->SetBoxExtent(FVector(500, 500, 10));
	Surface->SetCollisionProfileName(TEXT("BlockAll")); Surface->RegisterComponent();
	Floor->SetActorLocation(FVector(16000, 0, 1900));
	ON_SCOPE_EXIT
	{
		PC->CloseChat(); State->Phase = ETBPhase::Results; Pawn->Tick(0);
		State->Phase = OldPhase; State->HidePreparation = OldPreparation; State->EndTime = OldEnd;
		State->bTravelPending = OldPending; Info->Team = OldTeam;
		HUD->NotifyHitBoxClick(OldSettings ? TEXT("OpenSettings") : TEXT("CloseSettings"));
		Pawn->SetActorTransform(OldTransform); Pawn->ApplyState(); PC->PlayerTick(0); Floor->Destroy();
	};
	State->Phase = ETBPhase::Lobby; State->HidePreparation = FTBHidePreparation(); State->EndTime = 0;
	State->bTravelPending = false; Info->Team = ETBTeam::None; Pawn->ApplyState();
	Pawn->SetActorLocation(FVector(16000, 0, 1910 + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.15));
	auto* Movement = Pawn->GetCharacterMovement(); Movement->SetMovementMode(MOVE_Walking);
	HUD->NotifyHitBoxClick(TEXT("CloseSettings")); PC->PlayerTick(0);
	TestFalse(TEXT("Host can move in lobby without a panel"), PC->IsMoveInputIgnored());
	TestFalse(TEXT("Host can look in lobby without a panel"), PC->IsLookInputIgnored());
	TestTrue(TEXT("Lobby exploration uses the pawn camera"), PC->GetViewTarget() == Pawn);
	const FVector Before = Pawn->GetActorLocation();
	Pawn->AddMovementInput(FVector::ForwardVector, 1);
	Movement->TickComponent(.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Lobby movement changes the host position"), Pawn->GetActorLocation().X > Before.X + 1);
	Movement->StopMovementImmediately(); Movement->SetMovementMode(MOVE_Walking);
	TestTrue(TEXT("Unassigned lobby character supports emotes"), Pawn->CanRequestEmote());
	TestTrue(TEXT("Lobby emote starts with no match countdown"), Pawn->RequestEmote(ETBEmoteKind::Wave));
	FTBEmoteSample Sample;
	TestTrue(TEXT("Lobby emote produces a presentation sample"), Pawn->GetEmoteSample(Sample));
	Pawn->Emote.StartsAt -= .5; Pawn->Emote.EndsAt -= .5;
	FTBEmotePose Pose;
	TBToyPresentation::BuildEmotePose(Pawn->GetMesh(), Pawn, Pose);
	TestFalse(TEXT("Unassigned lobby model has visible emote bone offsets"), Pose.Bones.IsEmpty());
	State->Phase = ETBPhase::Playing;
	TestFalse(TEXT("Lobby snapshot cannot replay after match start"), Pawn->GetEmoteSample(Sample));
	State->Phase = ETBPhase::Lobby;
	State->bTravelPending = true; Pawn->ApplyState(); PC->PlayerTick(0);
	TestTrue(TEXT("Pending travel blocks movement"), PC->IsMoveInputIgnored());
	TestFalse(TEXT("Pending travel invalidates lobby emote"), Pawn->GetEmoteSample(Sample));
	Pawn->Tick(0); State->bTravelPending = false; Pawn->ApplyState(); PC->PlayerTick(0);
	PC->ToggleLeaveMenu(); PC->PlayerTick(0);
	TestTrue(TEXT("Escape opens lobby settings"), HUD->IsLobbySettingsOpen());
	TestTrue(TEXT("Settings blocks movement and look"), PC->IsMoveInputIgnored() && PC->IsLookInputIgnored());
	PC->ToggleLeaveMenu(); PC->PlayerTick(0);
	TestFalse(TEXT("Closing settings releases input lock"), PC->IsMoveInputIgnored() || PC->IsLookInputIgnored());
	PC->OpenChat(); PC->PlayerTick(0);
	TestTrue(TEXT("Chat opens and blocks movement"), PC->IsChatOpen() && PC->IsMoveInputIgnored());
	PC->CloseChat(); PC->PlayerTick(0);
	TestFalse(TEXT("Closing chat releases input lock"), PC->IsMoveInputIgnored() || PC->IsLookInputIgnored());
	return true;
}
#endif
