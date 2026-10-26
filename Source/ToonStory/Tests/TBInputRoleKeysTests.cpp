#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Character/TBCharacter.h"
#include "Character/TBMovement.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "Tests/AutomationCommon.h"

namespace
{
	// 陣営を切り替えてから、IMCが付き直るのを待って押すまでの余裕を含めたフレーム数。
	constexpr int32 PressHumanFrame = 2;
	constexpr int32 CheckHumanFrame = 6;
	constexpr int32 ReleaseCheckHumanFrame = 10;
	constexpr int32 SwitchToyFrame = 11;
	constexpr int32 PressToyFrame = 13;
	constexpr int32 CheckToyFrame = 17;
	constexpr int32 ReleaseCheckToyFrame = 21;
	// 押したまま陣営が変わるケース。
	constexpr int32 BackToHumanFrame = 22;
	constexpr int32 PressHeldCrouchFrame = 24;
	constexpr int32 SwitchHeldCrouchFrame = 28;
	constexpr int32 CheckHeldCrouchFrame = 32;
	constexpr int32 PressHeldSprintFrame = 34;
	constexpr int32 SwitchHeldSprintFrame = 38;
	constexpr int32 CheckHeldSprintFrame = 42;

	void SendRoleKey(ATBController* PC, const FKey& Key, EInputEvent Event)
	{
		FInputKeyEventArgs Args(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event == IE_Released ? 0.0 : 1.0, 0.016f, 1, 0);
		Args.Event = Event;
		PC->PlayerInput->InputKey(Args);
	}

	// 人間は左Ctrlでしゃがみ、人形は左Shiftでダッシュが要求され、離すと戻ることを確かめる。
	// 反対の陣営のキーは効かないことも見る。押したまま陣営が変わったときは、IMCが外れてCompletedが来て戻ることを見る。
	// 終わったら陣営とフェーズを元へ戻す。
	class FTBRoleKeysCommand : public IAutomationLatentCommand
	{
	public:
		FTBRoleKeysCommand(ATBController* InPC, FAutomationTestBase* InTest) : PC(InPC), Test(InTest)
		{
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
			auto* Movement = CastChecked<UTBMovement>(Body->GetCharacterMovement());
			if (!bStarted)
			{
				// 元の状態を控えてから、人間・試合中にする。
				bStarted = true;
				OriginalTeam = Body->TBPS()->Team;
				OriginalPhase = State->Phase;
				bHasOriginalPhase = true;
				Body->TBPS()->Team = ETBTeam::Human;
				PC->SyncInputContexts();
			}
			// 人がいない試合はGameModeがすぐ終わらせるので、毎フレーム試合中へ戻す。
			State->Phase = ETBPhase::Playing;
			switch (Frame)
			{
			case PressHumanFrame:
				SendRoleKey(PC.Get(), EKeys::LeftControl, IE_Pressed);
				SendRoleKey(PC.Get(), EKeys::LeftShift, IE_Pressed);
				break;
			case CheckHumanFrame:
				Test->TestTrue(TEXT("Human: left Ctrl requests crouch"), Movement->bWantsToCrouch);
				Test->TestFalse(TEXT("Human: left Shift does not request sprint"), Movement->bWantsToSprint);
				SendRoleKey(PC.Get(), EKeys::LeftControl, IE_Released);
				SendRoleKey(PC.Get(), EKeys::LeftShift, IE_Released);
				break;
			case ReleaseCheckHumanFrame:
				Test->TestFalse(TEXT("Human: releasing left Ctrl ends crouch"), Movement->bWantsToCrouch);
				break;
			case SwitchToyFrame:
				Body->TBPS()->Team = ETBTeam::Toy;
				PC->SyncInputContexts();
				break;
			case PressToyFrame:
				SendRoleKey(PC.Get(), EKeys::LeftControl, IE_Pressed);
				SendRoleKey(PC.Get(), EKeys::LeftShift, IE_Pressed);
				break;
			case CheckToyFrame:
				Test->TestTrue(TEXT("Toy: left Shift requests sprint"), Movement->bWantsToSprint);
				// CrouchPressedのIsHumanゲートでも通るので、IMCが無いことの確認としては弱い。
				Test->TestFalse(TEXT("Toy: left Ctrl does not request crouch"), Movement->bWantsToCrouch);
				SendRoleKey(PC.Get(), EKeys::LeftControl, IE_Released);
				SendRoleKey(PC.Get(), EKeys::LeftShift, IE_Released);
				break;
			case ReleaseCheckToyFrame:
				Test->TestFalse(TEXT("Toy: releasing left Shift ends sprint"), Movement->bWantsToSprint);
				break;
			case BackToHumanFrame:
				Body->TBPS()->Team = ETBTeam::Human;
				PC->SyncInputContexts();
				break;
			case PressHeldCrouchFrame:
				SendRoleKey(PC.Get(), EKeys::LeftControl, IE_Pressed);
				break;
			case SwitchHeldCrouchFrame:
				Test->TestTrue(TEXT("Held: Human crouch is requested before the switch"), Movement->bWantsToCrouch);
				Body->TBPS()->Team = ETBTeam::Toy;
				PC->SyncInputContexts();
				break;
			case CheckHeldCrouchFrame:
				// 左Ctrlは押したまま。IMCが外れてCompletedが来れば戻る。
				Test->TestFalse(TEXT("Held: crouch ends when the role changes to Toy"), Movement->bWantsToCrouch);
				SendRoleKey(PC.Get(), EKeys::LeftControl, IE_Released);
				break;
			case PressHeldSprintFrame:
				SendRoleKey(PC.Get(), EKeys::LeftShift, IE_Pressed);
				break;
			case SwitchHeldSprintFrame:
				Test->TestTrue(TEXT("Held: Toy sprint is requested before the switch"), Movement->bWantsToSprint);
				Body->TBPS()->Team = ETBTeam::Human;
				PC->SyncInputContexts();
				break;
			case CheckHeldSprintFrame:
				// 左Shiftは押したまま。IMCが外れてCompletedが来れば戻る。
				Test->TestFalse(TEXT("Held: sprint ends when the role changes to Human"), Movement->bWantsToSprint);
				Finish();
				return true;
			default:
				break;
			}
			++Frame;
			return false;
		}

	private:
		// 押したキーを離し、陣営とフェーズを元へ戻す。途中で抜けても残さない。
		void Finish()
		{
			if (!PC.IsValid()) return;
			SendRoleKey(PC.Get(), EKeys::LeftControl, IE_Released);
			SendRoleKey(PC.Get(), EKeys::LeftShift, IE_Released);
			if (auto* Body = Cast<ATBCharacter>(PC->GetPawn()); Body && bStarted)
			{
				Body->TBPS()->Team = OriginalTeam;
			}
			if (auto* State = PC->GetWorld()->GetGameState<ATBGameState>(); State && bHasOriginalPhase)
			{
				State->Phase = OriginalPhase;
			}
			PC->SyncInputContexts();
		}

		TWeakObjectPtr<ATBController> PC;
		FAutomationTestBase* Test;
		int32 Frame = 0;
		bool bStarted = false;
		bool bHasOriginalPhase = false;
		ETBTeam OriginalTeam = ETBTeam::None;
		ETBPhase OriginalPhase = ETBPhase::Lobby;
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRoleKeysTest, "ToonStory.Input.RoleKeys",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBRoleKeysTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	}
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	if (!TestNotNull(TEXT("Standalone game"), PC)) return false;
	if (!TestTrue(TEXT("Pawn"), Cast<ATBCharacter>(PC->GetPawn()) != nullptr)) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FTBRoleKeysCommand(PC, this));
	return true;
}
#endif
