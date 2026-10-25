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
	void PressKey(ATBController* PC, const FKey& Key, EInputEvent Event)
	{
		FInputKeyEventArgs Args(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event == IE_Released ? 0.0 : 1.0, 0.016f, 1, 0);
		Args.Event = Event;
		PC->PlayerInput->InputKey(Args);
	}
}

// WとSpaceを押し続けて、前への移動入力とジャンプが起きることを確かめる。
DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FTBMoveInputCommand, TWeakObjectPtr<ATBController>, PC, FAutomationTestBase*, Test);
bool FTBMoveInputCommand::Update()
{
	static int32 Frame = 0;
	static bool bJumped = false;
	auto* Body = PC.IsValid() ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
	if (!Body)
	{
		Test->AddError(TEXT("Pawn is gone"));
		Frame = 0;
		return true;
	}
	PC->ResetIgnoreMoveInput();
	bJumped |= Body->JumpCurrentCount > 0 || Body->GetCharacterMovement()->IsFalling();
	if (Frame == 0)
	{
		PressKey(PC.Get(), EKeys::W, IE_Pressed);
		PressKey(PC.Get(), EKeys::SpaceBar, IE_Pressed);
	}
	if (Frame == 6)
	{
		const FVector Input = Body->GetCharacterMovement()->GetLastInputVector();
		const FVector Forward = FRotator(0, Body->GetControlRotation().Yaw, 0).Vector();
		Test->TestTrue(TEXT("W moves forward"), FVector::DotProduct(Input, Forward) > .9f);
		Test->TestTrue(TEXT("Space starts a jump"), bJumped);
		PressKey(PC.Get(), EKeys::W, IE_Released);
		PressKey(PC.Get(), EKeys::SpaceBar, IE_Released);
	}
	if (Frame == 12)
	{
		Frame = 0;
		bJumped = false;
		return true;
	}
	++Frame;
	return false;
}

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
