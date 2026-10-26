#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBInputSettings.h"
#include "Core/TBPlayerState.h"
#include "Character/TBCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerInput.h"
#include "InputAction.h"
#include "Misc/App.h"
#include "Tests/AutomationCommon.h"

namespace
{
	// 1フレーム1/60秒に固定したときの、押す・確認する・離す・確認するフレーム。
	constexpr int32 PressHumanInteractFrame = 2;
	constexpr int32 CheckHumanInteractFrame = 6;
	constexpr int32 ReleaseCheckHumanInteractFrame = 10;
	constexpr int32 SwitchToyInteractFrame = 11;
	constexpr int32 PressToyInteractFrame = 13;
	constexpr int32 CheckToyInteractFrame = 17;
	constexpr int32 ReleaseCheckToyInteractFrame = 21;
	constexpr int32 PressTauntFrame = 23;
	constexpr int32 CheckTauntFrame = 27;
	constexpr int32 PressHeldInteractFrame = 29;
	constexpr int32 SwitchHeldInteractFrame = 33;
	constexpr int32 CheckHeldInteractFrame = 37;
	constexpr int32 PressAgainInteractFrame = 41;
	constexpr int32 CheckAgainInteractFrame = 45;
	constexpr int32 ReleaseCheckAgainInteractFrame = 49;
	// 試合の残り時間。0だとGameModeが時間切れで試合を終わらせてしまう。
	constexpr double KeepAliveSeconds = 1000.0;

	UWorld* ActionTestWorld()
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game) return Context.World();
		}
		return nullptr;
	}

	void SendActionKey(ATBController* PC, const FKey& Key, EInputEvent Event)
	{
		FInputKeyEventArgs Args(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event == IE_Released ? 0.0 : 1.0, 0.016f, 1, 0);
		Args.Event = Event;
		PC->PlayerInput->InputKey(Args);
	}

	// 人間と人形のEの長押し、Fの挑発、陣営が変わったあともEが効くことを確かめる。Qは結びだけ別のテストで見る。
	// 前提: 起動位置の近くに操作対象がないこと（あると操作がそちらへ渡る）。
	// 刻みを1/60秒に固定するのは、押し直しがLastRequestの連打防止（0.12秒）に弾かれないようにするため。
	// TryTaunt()は挑発のクールダウンを残すが、LastTauntTimeはprivateで戻せない。
	class FTBActionKeysCommand : public IAutomationLatentCommand
	{
	public:
		FTBActionKeysCommand(ATBController* InPC, FAutomationTestBase* InTest) : PC(InPC), Test(InTest)
		{
		}

		// 途中で打ち切られても時間の刻みを残さない。
		virtual ~FTBActionKeysCommand() override
		{
			RestoreFixedStep();
		}

		virtual bool Update() override
		{
			auto* Body = PC.IsValid() ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
			auto* State = PC.IsValid() ? PC->GetWorld()->GetGameState<ATBGameState>() : nullptr;
			if (!Body || !State)
			{
				Test->AddError(TEXT("Pawn or game state is gone"));
				Finish();
				return true;
			}
			if (!bStarted)
			{
				// 元の状態を控えてから、人間・試合中・固定の刻みにする。
				bStarted = true;
				OriginalTeam = Body->TBPS()->Team;
				OriginalPhase = State->Phase;
				OriginalEndTime = State->EndTime;
				bOriginalFixedStep = FApp::UseFixedTimeStep();
				OriginalFixedDelta = FApp::GetFixedDeltaTime();
				bFixedStepChanged = true;
				FApp::SetUseFixedTimeStep(true);
				FApp::SetFixedDeltaTime(1.0 / 60.0);
				Body->TBPS()->Team = ETBTeam::Human;
				PC->SyncInputContexts();
			}
			// 試合中のまま保つ。
			State->Phase = ETBPhase::Playing;
			State->EndTime = State->GetServerWorldTimeSeconds() + KeepAliveSeconds;
			switch (Frame)
			{
			case PressHumanInteractFrame:
				SendActionKey(PC.Get(), EKeys::E, IE_Pressed);
				break;
			case CheckHumanInteractFrame:
				Test->TestTrue(TEXT("Human: E starts the held interaction"), Body->bHoldingInteract);
				SendActionKey(PC.Get(), EKeys::E, IE_Released);
				break;
			case ReleaseCheckHumanInteractFrame:
				Test->TestFalse(TEXT("Human: releasing E ends the held interaction"), Body->bHoldingInteract);
				break;
			case SwitchToyInteractFrame:
				Body->TBPS()->Team = ETBTeam::Toy;
				PC->SyncInputContexts();
				break;
			case PressToyInteractFrame:
				SendActionKey(PC.Get(), EKeys::E, IE_Pressed);
				break;
			case CheckToyInteractFrame:
				Test->TestTrue(TEXT("Toy: E starts the held interaction"), Body->bHoldingInteract);
				SendActionKey(PC.Get(), EKeys::E, IE_Released);
				break;
			case ReleaseCheckToyInteractFrame:
				Test->TestFalse(TEXT("Toy: releasing E ends the held interaction"), Body->bHoldingInteract);
				break;
			case PressTauntFrame:
				SendActionKey(PC.Get(), EKeys::F, IE_Pressed);
				break;
			case CheckTauntFrame:
				// 挑発にはクールダウンがある。Fで挑発が起きていれば、直後の再実行は断られる。
				Test->TestTrue(TEXT("Toy: can act before the taunt check"), Body->CanAct());
				Test->TestFalse(TEXT("Toy: F triggered a taunt (cooldown is running)"), Body->TryTaunt());
				SendActionKey(PC.Get(), EKeys::F, IE_Released);
				break;
			case PressHeldInteractFrame:
				SendActionKey(PC.Get(), EKeys::E, IE_Pressed);
				break;
			case SwitchHeldInteractFrame:
				Test->TestTrue(TEXT("Held: Toy interaction is held before the switch"), Body->bHoldingInteract);
				// 本物の陣営切り替えはサーバーを通り、そこで解除される。Eは押したまま付け替える。
				Body->ClearHeldInteraction();
				Body->TBPS()->Team = ETBTeam::Human;
				PC->SyncInputContexts();
				break;
			case CheckHeldInteractFrame:
				Test->TestFalse(TEXT("Held: no interaction is held right after the switch"), Body->bHoldingInteract);
				SendActionKey(PC.Get(), EKeys::E, IE_Released);
				break;
			case PressAgainInteractFrame:
				SendActionKey(PC.Get(), EKeys::E, IE_Pressed);
				break;
			case CheckAgainInteractFrame:
				// 付け替えのあとでもEが効くこと（押しっぱなしの無視が残らないこと）。
				Test->TestTrue(TEXT("Held: E works again after the role changed"), Body->bHoldingInteract);
				SendActionKey(PC.Get(), EKeys::E, IE_Released);
				break;
			case ReleaseCheckAgainInteractFrame:
				Test->TestFalse(TEXT("Held: releasing E ends the interaction after the switch"), Body->bHoldingInteract);
				Finish();
				return true;
			default:
				break;
			}
			++Frame;
			return false;
		}

	private:
		// 押したキーを離し、陣営とフェーズと時間の刻みを元へ戻す。
		void Finish()
		{
			RestoreFixedStep();
			if (!PC.IsValid()) return;
			SendActionKey(PC.Get(), EKeys::E, IE_Released);
			SendActionKey(PC.Get(), EKeys::F, IE_Released);
			if (auto* Body = Cast<ATBCharacter>(PC->GetPawn()); Body && bStarted)
			{
				Body->TBPS()->Team = OriginalTeam;
				Body->ClearHeldInteraction();
			}
			if (auto* State = PC->GetWorld()->GetGameState<ATBGameState>(); State && bStarted)
			{
				State->Phase = OriginalPhase;
				State->EndTime = OriginalEndTime;
			}
			PC->SyncInputContexts();
		}

		// 変えていなければ何もしない。何度呼んでも安全。
		void RestoreFixedStep()
		{
			if (!bFixedStepChanged) return;
			bFixedStepChanged = false;
			FApp::SetUseFixedTimeStep(bOriginalFixedStep);
			FApp::SetFixedDeltaTime(OriginalFixedDelta);
		}

		TWeakObjectPtr<ATBController> PC;
		FAutomationTestBase* Test;
		int32 Frame = 0;
		bool bStarted = false;
		bool bFixedStepChanged = false;
		bool bOriginalFixedStep = false;
		double OriginalFixedDelta = 1.0 / 60.0;
		ETBTeam OriginalTeam = ETBTeam::None;
		ETBPhase OriginalPhase = ETBPhase::Lobby;
		double OriginalEndTime = 0;
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBActionKeysTest, "ToonStory.Input.InteractAndTaunt",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBActionKeysTest::RunTest(const FString& Parameters)
{
	auto* World = ActionTestWorld();
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	if (!TestNotNull(TEXT("Standalone game"), PC)) return false;
	if (!TestTrue(TEXT("Pawn"), Cast<ATBCharacter>(PC->GetPawn()) != nullptr)) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FTBActionKeysCommand(PC, this));
	return true;
}

// キャラの入力コンポーネントに、IAごとに必要なイベントが結ばれていることを確かめる。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBActionBindingsTest, "ToonStory.Input.ActionBindings",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBActionBindingsTest::RunTest(const FString& Parameters)
{
	auto* World = ActionTestWorld();
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	auto* Body = PC ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Pawn"), Body)) return false;
	auto* Input = Cast<UEnhancedInputComponent>(Body->InputComponent);
	if (!TestNotNull(TEXT("Enhanced input component"), Input)) return false;

	const auto* Settings = GetDefault<UTBInputSettings>();
	const auto Has = [Input](const TSoftObjectPtr<UInputAction>& Soft, ETriggerEvent Event)
	{
		const UInputAction* Action = Soft.LoadSynchronous();
		for (const auto& Binding : Input->GetActionEventBindings())
		{
			if (Action && Binding->GetAction() == Action && Binding->GetTriggerEvent() == Event) return true;
		}
		return false;
	};
	TestTrue(TEXT("Look: Triggered"), Has(Settings->LookAction, ETriggerEvent::Triggered));
	TestTrue(TEXT("Move: Triggered"), Has(Settings->MoveAction, ETriggerEvent::Triggered));
	for (const auto& [Name, Soft] : {TPair<const TCHAR*, const TSoftObjectPtr<UInputAction>*>{TEXT("Jump"), &Settings->JumpAction},
	                                 {TEXT("Crouch"), &Settings->CrouchAction},
	                                 {TEXT("Sprint"), &Settings->SprintAction},
	                                 {TEXT("Interact"), &Settings->InteractAction}})
	{
		TestTrue(FString::Printf(TEXT("%s: Started"), Name), Has(*Soft, ETriggerEvent::Started));
		TestTrue(FString::Printf(TEXT("%s: Completed"), Name), Has(*Soft, ETriggerEvent::Completed));
		TestTrue(FString::Printf(TEXT("%s: Canceled"), Name), Has(*Soft, ETriggerEvent::Canceled));
	}
	TestTrue(TEXT("Drop: Started"), Has(Settings->DropAction, ETriggerEvent::Started));
	TestTrue(TEXT("Taunt: Started"), Has(Settings->TauntAction, ETriggerEvent::Started));
	return true;
}
#endif
