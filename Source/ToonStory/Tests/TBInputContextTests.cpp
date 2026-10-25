#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Core/TBInputSettings.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBInputContextTest, "ToonStory.Input.RoleContexts",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBInputContextTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	}
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	auto* Info = PC ? PC->GetPlayerState<ATBPlayerState>() : nullptr;
	auto* Input = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!TestNotNull(TEXT("Standalone game"), Info) || !TestNotNull(TEXT("Input subsystem"), Input)) return false;

	auto* Settings = GetMutableDefault<UTBInputSettings>();
	const auto SavedCommon = Settings->CommonContext;
	const auto SavedHuman = Settings->HumanContext;
	const auto SavedToy = Settings->ToyContext;
	const ETBTeam SavedTeam = Info->Team;
	ON_SCOPE_EXIT
	{
		Settings->CommonContext = SavedCommon;
		Settings->HumanContext = SavedHuman;
		Settings->ToyContext = SavedToy;
		Info->Team = SavedTeam;
		PC->SyncInputContexts();
	};

	auto* Common = NewObject<UInputMappingContext>(GetTransientPackage());
	auto* Human = NewObject<UInputMappingContext>(GetTransientPackage());
	auto* Toy = NewObject<UInputMappingContext>(GetTransientPackage());
	Common->AddToRoot();
	Human->AddToRoot();
	Toy->AddToRoot();
	ON_SCOPE_EXIT
	{
		Common->RemoveFromRoot();
		Human->RemoveFromRoot();
		Toy->RemoveFromRoot();
	};

	Settings->CommonContext = nullptr;
	Settings->HumanContext = nullptr;
	Settings->ToyContext = nullptr;
	Info->Team = ETBTeam::Human;
	PC->SyncInputContexts();
	TestFalse(TEXT("Empty settings attach nothing"), Input->HasMappingContext(Common) || Input->HasMappingContext(Human));

	Settings->CommonContext = Common;
	Settings->HumanContext = Human;
	Settings->ToyContext = Toy;
	PC->SyncInputContexts();
	TestTrue(TEXT("Human: common"), Input->HasMappingContext(Common));
	TestTrue(TEXT("Human: human"), Input->HasMappingContext(Human));
	TestFalse(TEXT("Human: not toy"), Input->HasMappingContext(Toy));

	Info->Team = ETBTeam::Toy;
	PC->SyncInputContexts();
	TestTrue(TEXT("Toy: common"), Input->HasMappingContext(Common));
	TestFalse(TEXT("Toy: not human"), Input->HasMappingContext(Human));
	TestTrue(TEXT("Toy: toy"), Input->HasMappingContext(Toy));

	Info->Team = ETBTeam::None;
	PC->SyncInputContexts();
	TestTrue(TEXT("Lobby: common"), Input->HasMappingContext(Common));
	TestFalse(TEXT("Lobby: no role"), Input->HasMappingContext(Human) || Input->HasMappingContext(Toy));
	return true;
}
#endif
