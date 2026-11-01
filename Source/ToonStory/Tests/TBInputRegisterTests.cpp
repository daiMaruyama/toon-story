#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/TBTestMatch.h"
#include "Character/TBCharacter.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Input/TBInputSettings.h"
#include "Input/TBKeyLabels.h"
#include "Input/TBKeyRemap.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
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
	TBTest::FPlayingScope Playing(*State);
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

	// 設定のIAは、どれかのIMCに載っている（載っていないIAはIMCに保持されない）。
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
	                                 {TEXT("Menu"), &Settings->MenuAction},
	                                 {TEXT("EmoteWave"), &Settings->EmoteWaveAction},
	                                 {TEXT("EmoteCheer"), &Settings->EmoteCheerAction},
	                                 {TEXT("EmoteDance"), &Settings->EmoteDanceAction}})
	{
		const UInputAction* Action = Soft->LoadSynchronous();
		const bool bOnContext = Action && Contexts.ContainsByPredicate([Action](const UInputMappingContext* Imc) {
			return Imc->GetMappings().ContainsByPredicate(
			    [Action](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Action; });
		});
		TestTrue(*FString::Printf(TEXT("%s action is on one of the contexts"), Name), bOnContext);
	}
	const UInputMappingContext* Common = Settings->CommonContext.LoadSynchronous();
	const TSoftObjectPtr<UInputAction> EmoteActions[] = {Settings->EmoteWaveAction, Settings->EmoteCheerAction, Settings->EmoteDanceAction};
	const FName EmoteNames[] = {TB::Keys::EmoteWave, TB::Keys::EmoteCheer, TB::Keys::EmoteDance};
	const FKey Defaults[] = {EKeys::One, EKeys::Two, EKeys::Three};
	TMap<FName, FKey> SavedKeys;
	for (const FName& Name : TB::Keys::All())
	{
		TB::FKeyboardKey Row;
		if (!TestTrue(TEXT("Every registered row has a readable profile slot"), TB::FindKeyboardKey(PC, Name, Row))) return false;
		SavedKeys.Add(Name, Row.Key);
	}
	UTBInputSettings::RegisterContexts(PC->GetLocalPlayer()); // Re-registering must not reset existing custom keys.
	for (const auto& Pair : SavedKeys)
	{
		TB::FKeyboardKey Row;
		TestTrue(TEXT("Registering contexts preserves the current profile"), TB::FindKeyboardKey(PC, Pair.Key, Row) && Row.Key == Pair.Value);
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(EmoteActions); ++Index)
	{
		const UInputAction* Action = EmoteActions[Index].LoadSynchronous();
		if (!TestNotNull(TEXT("Emote input action"), Action) || !TestNotNull(TEXT("Common context"), Common)) return false;
		TestTrue(TEXT("Emote is a digital Started binding after context detach/GC"),
		    Action->ValueType == EInputActionValueType::Boolean && IsStartedBound(PawnInput, Action));
		TestFalse(TEXT("Emote cannot consume another action before the saved-key conflict guard runs"), Action->bConsumeInput);
		int32 NamedMappings = 0;
		for (const auto& Mapping : Common->GetMappings())
		{
			const auto* Mappable = Mapping.GetPlayerMappableKeySettings();
			if (Mapping.Action == Action && Mappable && Mappable->Name == EmoteNames[Index] && Mapping.Key == Defaults[Index]) ++NamedMappings;
		}
		TestEqual(TEXT("Emote default has one named keyboard mapping"), NamedMappings, 1);
		for (const auto* Context : Contexts)
			for (const auto& Mapping : Context->GetMappings())
				if (Mapping.Key == Defaults[Index])
					TestTrue(TEXT("Default digit is not stolen from another action"), Mapping.Action == Action);
		// A saved jump key may deliberately be unbound or use a now-reserved key.
		FKey ConflictKey;
		for (const auto& Pair : SavedKeys)
			if (Pair.Key != EmoteNames[Index] && TB::IsUsableKey(Pair.Value)) { ConflictKey = Pair.Value; break; }
		TestTrue(TEXT("Emote participates in existing same-scope conflict detection"),
		    ConflictKey.IsValid() && TB::CheckRemap(PC, EmoteNames[Index], ConflictKey).Result == TB::ERemapResult::Conflict);
	}
	return true;
}
#endif
