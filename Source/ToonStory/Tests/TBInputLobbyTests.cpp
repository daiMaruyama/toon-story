#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Input/TBInputSettings.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Tests/AutomationCommon.h"

namespace
{
	constexpr int32 PressReadyFrame = 2;
	constexpr int32 CheckReadyFrame = 6;
	constexpr int32 PressMenuInLobbyFrame = 8;
	constexpr int32 CheckMenuInLobbyFrame = 12;
	constexpr int32 StartPlayingFrame = 14;
	constexpr int32 PressMenuOpenFrame = 16;
	constexpr int32 CheckMenuOpenFrame = 20;
	constexpr int32 PressMenuCloseFrame = 22;
	constexpr int32 CheckMenuCloseFrame = 26;
	// 試合の残り時間。0だとGameModeが時間切れで試合を終わらせてしまう。
	constexpr double LobbyKeepAliveSeconds = 1000.0;

	UWorld* LobbyTestWorld()
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game) return Context.World();
		}
		return nullptr;
	}

	void SendLobbyKey(ATBController* PC, const FKey& Key, EInputEvent Event)
	{
		FInputKeyEventArgs Args(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event == IE_Released ? 0.0 : 1.0, 0.016f, 1, 0);
		Args.Event = Event;
		PC->PlayerInput->InputKey(Args);
	}

	// ロビーのRで準備完了、ロビーのEscは何も起きず、試合中のEscで退出メニューが開閉することを確かめる。F5は結びだけ別のテストで見る。
	class FTBLobbyKeysCommand : public IAutomationLatentCommand
	{
	public:
		FTBLobbyKeysCommand(ATBController* InPC, FAutomationTestBase* InTest) : PC(InPC), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			auto* Info = PC.IsValid() ? PC->GetPlayerState<ATBPlayerState>() : nullptr;
			auto* State = PC.IsValid() ? PC->GetWorld()->GetGameState<ATBGameState>() : nullptr;
			if (!Info || !State)
			{
				Test->AddError(TEXT("Player state or game state is gone"));
				Finish();
				return true;
			}
			if (!bStarted)
			{
				bStarted = true;
				OriginalReady = Info->bReady;
				OriginalPhase = State->Phase;
				OriginalEndTime = State->EndTime;
			}
			// 開始の前まではロビー、そのあとは試合中のまま保つ。
			State->Phase = Frame < StartPlayingFrame ? ETBPhase::Lobby : ETBPhase::Playing;
			if (Frame >= StartPlayingFrame)
			{
				State->EndTime = State->GetServerWorldTimeSeconds() + LobbyKeepAliveSeconds;
			}
			switch (Frame)
			{
			case PressReadyFrame:
				SendLobbyKey(PC.Get(), EKeys::R, IE_Pressed);
				break;
			case CheckReadyFrame:
				Test->TestTrue(TEXT("Lobby: R toggles ready"), Info->bReady != OriginalReady);
				SendLobbyKey(PC.Get(), EKeys::R, IE_Released);
				break;
			case PressMenuInLobbyFrame:
				SendLobbyKey(PC.Get(), EKeys::Escape, IE_Pressed);
				break;
			case CheckMenuInLobbyFrame:
				Test->TestFalse(TEXT("Lobby: Esc does not open the leave menu"), PC->IsLeaveMenuOpen());
				SendLobbyKey(PC.Get(), EKeys::Escape, IE_Released);
				break;
			case StartPlayingFrame:
				PC->SyncInputContexts();
				break;
			case PressMenuOpenFrame:
				SendLobbyKey(PC.Get(), EKeys::Escape, IE_Pressed);
				break;
			case CheckMenuOpenFrame:
				Test->TestTrue(TEXT("Playing: Esc opens the leave menu"), PC->IsLeaveMenuOpen());
				SendLobbyKey(PC.Get(), EKeys::Escape, IE_Released);
				break;
			case PressMenuCloseFrame:
				SendLobbyKey(PC.Get(), EKeys::Escape, IE_Pressed);
				break;
			case CheckMenuCloseFrame:
				Test->TestFalse(TEXT("Playing: Esc again closes the leave menu"), PC->IsLeaveMenuOpen());
				Finish();
				return true;
			default:
				break;
			}
			++Frame;
			return false;
		}

	private:
		// 押したキーを離し、メニューを閉じて、準備とフェーズを元へ戻す。
		void Finish()
		{
			if (!PC.IsValid()) return;
			SendLobbyKey(PC.Get(), EKeys::R, IE_Released);
			SendLobbyKey(PC.Get(), EKeys::Escape, IE_Released);
			auto* State = PC->GetWorld()->GetGameState<ATBGameState>();
			if (State && PC->IsLeaveMenuOpen())
			{
				State->Phase = ETBPhase::Playing;
				PC->ToggleLeaveMenu();
			}
			if (bStarted)
			{
				if (auto* Info = PC->GetPlayerState<ATBPlayerState>()) Info->bReady = OriginalReady;
				if (State)
				{
					State->Phase = OriginalPhase;
					State->EndTime = OriginalEndTime;
				}
			}
			PC->SyncInputContexts();
		}

		TWeakObjectPtr<ATBController> PC;
		FAutomationTestBase* Test;
		int32 Frame = 0;
		bool bStarted = false;
		bool OriginalReady = false;
		ETBPhase OriginalPhase = ETBPhase::Lobby;
		double OriginalEndTime = 0;
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLobbyKeysTest, "ToonStory.Input.LobbyKeys",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBLobbyKeysTest::RunTest(const FString& Parameters)
{
	auto* World = LobbyTestWorld();
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	if (!TestNotNull(TEXT("Standalone game"), PC)) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FTBLobbyKeysCommand(PC, this));
	return true;
}

// 設定のIMCとIAが実行時に読め、キーが入っていること。コントローラにIAごとにStartedが結ばれていること。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLobbyBindingsTest, "ToonStory.Input.LobbyBindings",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBLobbyBindingsTest::RunTest(const FString& Parameters)
{
	auto* World = LobbyTestWorld();
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	if (!TestNotNull(TEXT("Standalone game"), PC)) return false;
	const auto* Settings = GetDefault<UTBInputSettings>();

	const auto HasKey = [](const UInputMappingContext* Imc, const FKey& Key, const UInputAction* Action)
	{
		if (!Imc || !Action) return false;
		for (const FEnhancedActionKeyMapping& Mapping : Imc->GetMappings())
		{
			if (Mapping.Key == Key && Mapping.Action == Action) return true;
		}
		return false;
	};
	const UInputAction* Ready = Settings->ReadyAction.LoadSynchronous();
	const UInputAction* Start = Settings->StartAction.LoadSynchronous();
	const UInputAction* Menu = Settings->MenuAction.LoadSynchronous();
	const UInputMappingContext* Lobby = Settings->LobbyContext.LoadSynchronous();
	const UInputMappingContext* Common = Settings->CommonContext.LoadSynchronous();
	TestTrue(TEXT("IMC_Lobby maps R to Ready"), HasKey(Lobby, EKeys::R, Ready));
	TestTrue(TEXT("IMC_Lobby maps F5 to Start"), HasKey(Lobby, EKeys::F5, Start));
	TestTrue(TEXT("IMC_Common maps Escape to Menu"), HasKey(Common, EKeys::Escape, Menu));

	auto* Input = Cast<UEnhancedInputComponent>(PC->InputComponent);
	if (!TestNotNull(TEXT("Enhanced input component"), Input)) return false;
	const auto IsStarted = [Input](const UInputAction* Action)
	{
		for (const auto& Binding : Input->GetActionEventBindings())
		{
			if (Action && Binding->GetAction() == Action && Binding->GetTriggerEvent() == ETriggerEvent::Started) return true;
		}
		return false;
	};
	TestTrue(TEXT("Ready: Started"), IsStarted(Ready));
	TestTrue(TEXT("Start: Started"), IsStarted(Start));
	TestTrue(TEXT("Menu: Started"), IsStarted(Menu));
	return true;
}
#endif
