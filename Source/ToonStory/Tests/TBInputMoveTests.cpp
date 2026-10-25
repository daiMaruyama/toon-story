#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Character/TBCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Tests/AutomationCommon.h"

namespace
{
	// 押してから確認するまで、確認してから次の操作まで、全体の終わりまでのフレーム数。
	constexpr int32 CheckForwardFrame = 6;
	constexpr int32 CheckRightFrame = 12;
	constexpr int32 EndFrame = 18;

	void SendKey(ATBController* PC, const FKey& Key, EInputEvent Event)
	{
		FInputKeyEventArgs Args(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event == IE_Released ? 0.0 : 1.0, 0.016f, 1, 0);
		Args.Event = Event;
		PC->PlayerInput->InputKey(Args);
	}

	// 押したキーを全部離す。途中で抜けても次の実行に残さない。
	void ReleaseAll(ATBController* PC)
	{
		for (const FKey& Key : {EKeys::W, EKeys::D, EKeys::SpaceBar})
		{
			SendKey(PC, Key, IE_Released);
		}
	}

	// 前進とジャンプ（WとSpace）、続けて右（D）の入力が起きることを確かめる。状態は実行ごとに作り直す。
	class FTBMoveInputCommand : public IAutomationLatentCommand
	{
	public:
		FTBMoveInputCommand(ATBController* InPC, FAutomationTestBase* InTest) : PC(InPC), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			auto* Body = PC.IsValid() ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
			if (!Body)
			{
				Test->AddError(TEXT("Pawn is gone"));
				if (PC.IsValid()) ReleaseAll(PC.Get());
				return true;
			}
			auto* Movement = Body->GetCharacterMovement();
			PC->ResetIgnoreMoveInput();
			bJumped |= Body->JumpCurrentCount > 0 || Movement->IsFalling();
			if (Frame == 0)
			{
				// 空中から始まるとジャンプの確認をすり抜けるので、地面にいることを先に確かめる。
				bGrounded = Movement->IsMovingOnGround();
				Test->TestTrue(TEXT("Starts on the ground"), bGrounded);
				SendKey(PC.Get(), EKeys::W, IE_Pressed);
				SendKey(PC.Get(), EKeys::SpaceBar, IE_Pressed);
			}
			if (Frame == CheckForwardFrame)
			{
				const FVector Forward = FRotator(0, Body->GetControlRotation().Yaw, 0).Vector();
				Test->TestTrue(TEXT("W moves forward"), FVector::DotProduct(Movement->GetLastInputVector(), Forward) > .9f);
				if (bGrounded)
				{
					Test->TestTrue(TEXT("Space starts a jump"), bJumped);
				}
				SendKey(PC.Get(), EKeys::W, IE_Released);
				SendKey(PC.Get(), EKeys::SpaceBar, IE_Released);
				SendKey(PC.Get(), EKeys::D, IE_Pressed);
			}
			if (Frame == CheckRightFrame)
			{
				const FVector Right = FRotationMatrix(FRotator(0, Body->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y);
				Test->TestTrue(TEXT("D moves right"), FVector::DotProduct(Movement->GetLastInputVector(), Right) > .9f);
				ReleaseAll(PC.Get());
			}
			return ++Frame > EndFrame;
		}

	private:
		TWeakObjectPtr<ATBController> PC;
		FAutomationTestBase* Test;
		int32 Frame = 0;
		bool bJumped = false;
		bool bGrounded = false;
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMoveInputTest, "ToonStory.Input.MoveAndJump",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMoveInputTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	}
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	if (!TestNotNull(TEXT("Standalone game"), PC)) return false;
	if (!TestTrue(TEXT("Pawn"), PC->GetPawn() != nullptr)) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FTBMoveInputCommand(PC, this));
	return true;
}
#endif
