#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBCharacter.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBInputSettings.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
	const UInputAction* ActionOfKey(const UInputMappingContext* Imc, const FKey& Key)
	{
		if (!Imc) return nullptr;
		for (const FEnhancedActionKeyMapping& Mapping : Imc->GetMappings())
		{
			if (Mapping.Key == Key) return Mapping.Action;
		}
		return nullptr;
	}

	bool IsStartedBound(const UEnhancedInputComponent* Input, const UInputAction* Action)
	{
		if (!Input || !Action) return false;
		for (const auto& Binding : Input->GetActionEventBindings())
		{
			if (Binding->GetAction() == Action && Binding->GetTriggerEvent() == ETriggerEvent::Started) return true;
		}
		return false;
	}
}

// IMCがUserSettingsに登録され、外してGCを回して付け直しても、結びのIAが同じオブジェクトのままであること。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRegisteredSurviveGCTest, "ToonStory.Input.RegisteredSurviveGC",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBRegisteredSurviveGCTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	}
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	auto* Body = PC ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
	auto* Info = PC ? PC->GetPlayerState<ATBPlayerState>() : nullptr;
	auto* State = World ? World->GetGameState<ATBGameState>() : nullptr;
	auto* Input = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	auto* User = Input ? Input->GetUserSettings() : nullptr;
	if (!TestNotNull(TEXT("Pawn"), Body) || !TestNotNull(TEXT("Player state"), Info) || !TestNotNull(TEXT("Game state"), State) ||
	    !TestNotNull(TEXT("User settings"), User))
		return false;
	auto* ControllerInput = Cast<UEnhancedInputComponent>(PC->InputComponent);
	auto* PawnInput = Cast<UEnhancedInputComponent>(Body->InputComponent);
	if (!TestNotNull(TEXT("Controller input"), ControllerInput) || !TestNotNull(TEXT("Pawn input"), PawnInput)) return false;

	const auto* Settings = GetDefault<UTBInputSettings>();
	for (const auto* Soft : {&Settings->CommonContext, &Settings->HumanContext, &Settings->ToyContext, &Settings->LobbyContext})
	{
		TestTrue(*FString::Printf(TEXT("%s is registered"), *Soft->GetAssetName()), User->IsMappingContextRegistered(Soft->LoadSynchronous()));
	}

	const ETBTeam SavedTeam = Info->Team;
	const ETBPhase SavedPhase = State->Phase;
	ON_SCOPE_EXIT
	{
		Info->Team = SavedTeam;
		State->Phase = SavedPhase;
		PC->SyncInputContexts();
	};

	// 人間・人形・ロビーのIMCを外してGCを回す。
	Info->Team = ETBTeam::None;
	State->Phase = ETBPhase::Playing;
	PC->SyncInputContexts();
	TestFalse(TEXT("Human context is detached"), Input->HasMappingContext(Settings->HumanContext.LoadSynchronous()));
	TestFalse(TEXT("Toy context is detached"), Input->HasMappingContext(Settings->ToyContext.LoadSynchronous()));
	TestFalse(TEXT("Lobby context is detached"), Input->HasMappingContext(Settings->LobbyContext.LoadSynchronous()));
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// 付け直しても、IMCのActionと結びのActionは同じオブジェクト。
	Info->Team = ETBTeam::Human;
	State->Phase = ETBPhase::Lobby;
	PC->SyncInputContexts();
	TestTrue(TEXT("Crouch is still bound to the IMC action"),
	         IsStartedBound(PawnInput, ActionOfKey(Settings->HumanContext.LoadSynchronous(), EKeys::LeftControl)));
	TestTrue(TEXT("Ready is still bound to the IMC action"),
	         IsStartedBound(ControllerInput, ActionOfKey(Settings->LobbyContext.LoadSynchronous(), EKeys::R)));
	Info->Team = ETBTeam::Toy;
	PC->SyncInputContexts();
	TestTrue(TEXT("Sprint is still bound to the IMC action"),
	         IsStartedBound(PawnInput, ActionOfKey(Settings->ToyContext.LoadSynchronous(), EKeys::LeftShift)));

	// 設定の11個のIAは、どれかのIMCに載っている（載っていないIAはIMCに保持されない）。
	const TArray<const UInputMappingContext*> Contexts = Settings->GetAllContexts();
	for (const auto& [Name, Soft] : {TPair<const TCHAR*, const TSoftObjectPtr<UInputAction>*>{TEXT("Look"), &Settings->LookAction},
	                                 {TEXT("Move"), &Settings->MoveAction},
	                                 {TEXT("Jump"), &Settings->JumpAction},
	                                 {TEXT("Crouch"), &Settings->CrouchAction},
	                                 {TEXT("Sprint"), &Settings->SprintAction},
	                                 {TEXT("Interact"), &Settings->InteractAction},
	                                 {TEXT("Drop"), &Settings->DropAction},
	                                 {TEXT("Taunt"), &Settings->TauntAction},
	                                 {TEXT("Ready"), &Settings->ReadyAction},
	                                 {TEXT("Start"), &Settings->StartAction},
	                                 {TEXT("Menu"), &Settings->MenuAction}})
	{
		const UInputAction* Action = Soft->LoadSynchronous();
		const bool bOnContext = Action && Contexts.ContainsByPredicate([Action](const UInputMappingContext* Imc) {
			return Imc->GetMappings().ContainsByPredicate(
			    [Action](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Action; });
		});
		TestTrue(*FString::Printf(TEXT("%s action is on one of the contexts"), Name), bOnContext);
	}
	return true;
}
#endif
