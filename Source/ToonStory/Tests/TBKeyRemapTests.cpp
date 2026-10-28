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
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerInput.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "Tests/AutomationCommon.h"
#include "Tests/TBKeyRemapper.h"

namespace
{
	ATBController* RemapTestController()
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World())
			{
				return Cast<ATBController>(Context.World()->GetFirstPlayerController());
			}
		}
		return nullptr;
	}

	UEnhancedInputUserSettings* RemapTestUser(ATBController* PC)
	{
		auto* Input = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
		return Input ? Input->GetUserSettings() : nullptr;
	}

	// 全13行の今のキーを控える。
	bool RememberAll(FTBKeyRemapper& Remapper)
	{
		for (const FName& Name : TB::Keys::All())
		{
			if (!Remapper.Remember(Name)) return false;
		}
		return true;
	}
}

// かぶり判定。Commonは全部、それ以外は自分のIMCとCommonだけ。使えないキーと使えるキーも見る。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRemapCheckTest, "ToonStory.Input.RemapCheck",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBRemapCheckTest::RunTest(const FString& Parameters)
{
	auto* PC = RemapTestController();
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), RemapTestUser(PC))) return false;

	const auto ExpectOK = [&](const FString& What, FName Name, const FKey& Key)
	{
		TestTrue(*What, TB::CheckRemap(PC, Name, Key).Result == TB::ERemapResult::OK);
	};
	const auto ExpectConflict = [&](const TCHAR* What, FName Name, const FKey& Key, FName With)
	{
		const TB::FRemapResult Check = TB::CheckRemap(PC, Name, Key);
		TestTrue(What, Check.Result == TB::ERemapResult::Conflict);
		TestTrue(*FString::Printf(TEXT("%s (with)"), What), Check.Conflicting.Num() == 1 && Check.Conflicting[0] == With);
	};
	using namespace TB::Keys;

	// 同じ範囲どうし、Commonとの組はかぶる。
	ExpectConflict(TEXT("Common vs Common"), MoveForward, EKeys::SpaceBar, Jump);
	ExpectConflict(TEXT("Common vs Human"), Jump, EKeys::LeftControl, HumanCrouch);
	ExpectConflict(TEXT("Human vs Common"), HumanCrouch, EKeys::W, MoveForward);
	ExpectConflict(TEXT("Common vs Toy"), Jump, EKeys::LeftShift, ToySprint);
	ExpectConflict(TEXT("Toy vs Common"), ToySprint, EKeys::SpaceBar, Jump);
	ExpectConflict(TEXT("Common vs Lobby"), MoveBack, EKeys::R, LobbyReady);
	ExpectConflict(TEXT("Lobby vs Common"), LobbyReady, EKeys::W, MoveForward);
	ExpectConflict(TEXT("Human vs Human"), HumanCrouch, EKeys::E, HumanInteract);
	ExpectConflict(TEXT("Toy vs Toy"), ToySprint, EKeys::F, ToyTaunt);
	ExpectConflict(TEXT("Lobby vs Lobby"), LobbyReady, EKeys::F5, LobbyStart);

	// かぶり先は全部返す。ジャンプをEにすると、人間と人形のEの2行にかぶる。
	const TB::FRemapResult Both = TB::CheckRemap(PC, Jump, EKeys::E);
	TestTrue(TEXT("Jump to E conflicts"), Both.Result == TB::ERemapResult::Conflict);
	TestTrue(TEXT("Jump to E conflicts with both Interact rows"),
	         Both.Conflicting.Num() == 2 && Both.Conflicting.Contains(HumanInteract) && Both.Conflicting.Contains(ToyInteract));

	// 別の陣営・ロビーとはかぶらない。Human_InteractとToy_Interactが両方Eなのは正しい。
	ExpectOK(TEXT("Human vs Toy (shift)"), HumanCrouch, EKeys::LeftShift);
	ExpectOK(TEXT("Human vs Toy (taunt key)"), HumanInteract, EKeys::F);
	ExpectOK(TEXT("Toy vs Human (drop key)"), ToyInteract, EKeys::Q);
	ExpectOK(TEXT("Toy vs Human (crouch key)"), ToyTaunt, EKeys::LeftControl);
	ExpectOK(TEXT("Lobby vs Human (crouch key)"), LobbyReady, EKeys::LeftControl);
	ExpectOK(TEXT("Lobby vs Human (interact key)"), LobbyReady, EKeys::E);
	ExpectOK(TEXT("Human vs Lobby"), HumanCrouch, EKeys::R);
	ExpectOK(TEXT("Lobby vs Toy"), LobbyStart, EKeys::F);
	ExpectOK(TEXT("Toy vs Lobby"), ToyTaunt, EKeys::F5);
	ExpectOK(TEXT("Human_Interact keeps E"), HumanInteract, EKeys::E);
	ExpectOK(TEXT("Toy_Interact keeps E"), ToyInteract, EKeys::E);
	ExpectOK(TEXT("A free key"), HumanCrouch, EKeys::K);
	// 左右は別のキー。Commonのジャンプに付けても、左のダッシュ・しゃがみとかぶらない。
	ExpectOK(TEXT("Right shift is not left shift"), Jump, EKeys::RightShift);
	ExpectOK(TEXT("Right ctrl is not left ctrl"), Jump, EKeys::RightControl);
	ExpectOK(TEXT("Right shift for Human_Crouch"), HumanCrouch, EKeys::RightShift);
	ExpectOK(TEXT("Right ctrl for Toy_Sprint"), ToySprint, EKeys::RightControl);

	// 全13行がちょうど1つのIMCに入っている。既定のキーどうし（今のキーではない）は、範囲が重なる行でかぶらない。
	const auto* Settings = GetDefault<UTBInputSettings>();
	const TSoftObjectPtr<UInputMappingContext>* ContextPtrs[] = {&Settings->CommonContext, &Settings->HumanContext,
	                                                             &Settings->ToyContext, &Settings->LobbyContext};
	TMap<FName, int32> Scopes;
	TMap<FName, FKey> Defaults;
	for (const FName& Name : TB::Keys::All())
	{
		int32 ContextCount = 0;
		for (int32 Scope = 0; Scope < UE_ARRAY_COUNT(ContextPtrs); ++Scope)
		{
			const UInputMappingContext* Context = ContextPtrs[Scope]->LoadSynchronous();
			if (Context && Context->GetMappings().ContainsByPredicate([&Name](const FEnhancedActionKeyMapping& Mapping) {
				    const UPlayerMappableKeySettings* Key = Mapping.GetPlayerMappableKeySettings();
				    return Key && Key->Name == Name;
			    }))
			{
				++ContextCount;
				Scopes.Add(Name, Scope);
			}
		}
		TestEqual(*FString::Printf(TEXT("%s is in exactly one IMC"), *Name.ToString()), ContextCount, 1);
		TB::FKeyboardKey Row;
		if (TestTrue(*FString::Printf(TEXT("%s has a keyboard row"), *Name.ToString()), TB::FindKeyboardKey(PC, Name, Row)))
		{
			Defaults.Add(Name, Row.DefaultKey);
		}
	}
	const TArray<FName>& Names = TB::Keys::All();
	for (int32 A = 0; A < Names.Num(); ++A)
	{
		for (int32 B = A + 1; B < Names.Num(); ++B)
		{
			const int32* ScopeA = Scopes.Find(Names[A]);
			const int32* ScopeB = Scopes.Find(Names[B]);
			const FKey* KeyA = Defaults.Find(Names[A]);
			const FKey* KeyB = Defaults.Find(Names[B]);
			if (!ScopeA || !ScopeB || !KeyA || !KeyB) continue;
			const bool bOverlap = *ScopeA == 0 || *ScopeB == 0 || *ScopeA == *ScopeB;
			TestFalse(*FString::Printf(TEXT("The defaults of %s and %s do not conflict"), *Names[A].ToString(), *Names[B].ToString()),
			          bOverlap && *KeyA == *KeyB);
		}
	}

	// 断るキー。全部の行で。ホイールの上下、Command、ConsoleKeys、Escape、パッド、マウスの軸、Invalid。
	TArray<FKey> Refused = {EKeys::MouseScrollUp, EKeys::MouseScrollDown, EKeys::LeftCommand, EKeys::RightCommand, EKeys::Escape,
	                        EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_Left2D, EKeys::MouseX, EKeys::MouseY, EKeys::Mouse2D,
	                        EKeys::MouseWheelAxis, EKeys::Invalid};
	for (const FKey& Key : GetDefault<UInputSettings>()->ConsoleKeys)
	{
		if (Key.IsValid()) Refused.Add(Key);
	}
	TestTrue(TEXT("ConsoleKeys are part of the refused keys"), Refused.Num() > 12);
	for (const FKey& Key : Refused)
	{
		TestFalse(*FString::Printf(TEXT("%s is not a usable key"), *Key.ToString()), TB::IsUsableKey(Key));
		for (const FName& Name : TB::Keys::All())
		{
			TestTrue(*FString::Printf(TEXT("%s is refused for %s"), *Key.ToString(), *Name.ToString()),
			         TB::CheckRemap(PC, Name, Key).Result == TB::ERemapResult::Unusable);
		}
	}

	// 許可するキー。Alt、Fキー、マウスのボタン、右Shift。
	for (const FKey& Key : {EKeys::LeftAlt, EKeys::RightAlt, EKeys::F2, EKeys::F6, EKeys::F12, EKeys::LeftMouseButton,
	                        EKeys::RightMouseButton, EKeys::MiddleMouseButton, EKeys::ThumbMouseButton, EKeys::ThumbMouseButton2,
	                        EKeys::RightShift})
	{
		TestTrue(*FString::Printf(TEXT("%s is a usable key"), *Key.ToString()), TB::IsUsableKey(Key));
		ExpectOK(FString::Printf(TEXT("%s is allowed for Human_Crouch"), *Key.ToString()), HumanCrouch, Key);
	}
	return true;
}

// 付け替え・入れ替え・既定に戻す。KeyLabelが付け替え後のキー名を返す。13行を控えて書き戻す。保存はしない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRemapApplyTest, "ToonStory.Input.RemapApply",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBRemapApplyTest::RunTest(const FString& Parameters)
{
	auto* PC = RemapTestController();
	auto* User = RemapTestUser(PC);
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User)) return false;

	using namespace TB::Keys;
	FTBKeyRemapper Remapper(*this, User);
	if (!RememberAll(Remapper)) return false;
	const auto Label = [&](FName Name) { return TB::KeyLabel(PC, Name); };

	// 付け替える。かぶる・使えないときは何も変わらない。
	TestTrue(TEXT("Remap to a free key"), TB::Remap(PC, HumanCrouch, EKeys::C).Result == TB::ERemapResult::OK);
	TestEqual(TEXT("Label after remap"), Label(HumanCrouch), FString(TEXT("C")));
	TestTrue(TEXT("Remap to a conflicting key is refused"), TB::Remap(PC, HumanCrouch, EKeys::E).Result == TB::ERemapResult::Conflict);
	TestTrue(TEXT("Remap to Escape is refused"), TB::Remap(PC, HumanCrouch, EKeys::Escape).Result == TB::ERemapResult::Unusable);
	TestEqual(TEXT("Label is unchanged after refusals"), Label(HumanCrouch), FString(TEXT("C")));

	// 右Shiftは別のキーとして付けられ、「右Shift」と出る。
	TestTrue(TEXT("Remap to right shift"), TB::Remap(PC, HumanCrouch, EKeys::RightShift).Result == TB::ERemapResult::OK);
	TestEqual(TEXT("Right shift label"), Label(HumanCrouch), FString(TEXT("右Shift")));

	// 入れ替える。かぶりが無いときは入れ替えない。
	TestTrue(TEXT("Swap without a conflict has no partner"), TB::SwapRemap(PC, HumanInteract, EKeys::K).Result == TB::ERemapResult::NoPartner);
	const TB::FRemapResult Swapped = TB::SwapRemap(PC, HumanInteract, EKeys::Q);
	TestTrue(TEXT("Swap with the one that conflicts"), Swapped.Result == TB::ERemapResult::OK);
	TestEqual(TEXT("The swap partner is returned"), Swapped.SwappedWith, HumanDrop);
	TestEqual(TEXT("Human_Interact got Q"), Label(HumanInteract), FString(TEXT("Q")));
	TestEqual(TEXT("Human_Drop got E"), Label(HumanDrop), FString(TEXT("E")));

	// 入れ替えた2行は、どちらを戻してもかぶる。相手が1行なので、画面は「入れ替えて戻しますか」と案内する。
	// その流れ（SwapRemapに既定のキーを渡す）で、2行とも元に戻る。
	const TB::FRemapResult Blocked = TB::ResetToDefault(PC, HumanInteract);
	TestTrue(TEXT("Reset one row would conflict"), Blocked.Result == TB::ERemapResult::Conflict);
	TestTrue(TEXT("The only conflict is Human_Drop"), Blocked.Conflicting.Num() == 1 && Blocked.Conflicting[0] == HumanDrop);
	TestEqual(TEXT("A refused reset changes nothing"), Label(HumanInteract), FString(TEXT("Q")));
	TestTrue(TEXT("Reset the other row also conflicts"), TB::ResetToDefault(PC, HumanDrop).Result == TB::ERemapResult::Conflict);
	TestTrue(TEXT("Swap back with the default key"), TB::SwapRemap(PC, HumanInteract, EKeys::E).Result == TB::ERemapResult::OK);
	TestEqual(TEXT("Human_Interact is back to E by the swap"), Label(HumanInteract), FString(TEXT("E")));
	TestEqual(TEXT("Human_Drop is back to Q by the swap"), Label(HumanDrop), FString(TEXT("Q")));

	// 全部戻す。
	TestTrue(TEXT("Reset all"), TB::ResetAll(PC).Result == TB::ERemapResult::OK);
	TestEqual(TEXT("Human_Interact is still E"), Label(HumanInteract), FString(TEXT("E")));
	TestEqual(TEXT("Human_Drop is still Q"), Label(HumanDrop), FString(TEXT("Q")));
	TestEqual(TEXT("Human_Crouch is back to Ctrl"), Label(HumanCrouch), FString(TEXT("Ctrl")));
	TestEqual(TEXT("Lobby_Ready is back to R"), Label(LobbyReady), FString(TEXT("R")));
	return true;
}

// 既定に戻すときの新しいかぶり。パッドは触らない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRemapResetTest, "ToonStory.Input.RemapReset",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBRemapResetTest::RunTest(const FString& Parameters)
{
	auto* PC = RemapTestController();
	auto* User = RemapTestUser(PC);
	auto* Profile = User ? User->GetActiveKeyProfile() : nullptr;
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("Profile"), Profile)) return false;

	using namespace TB::Keys;
	FTBKeyRemapper Remapper(*this, User);
	if (!RememberAll(Remapper)) return false;
	const auto Label = [&](FName Name) { return TB::KeyLabel(PC, Name); };
	const auto PadKeys = [&](FName Name)
	{
		TArray<FKey> Keys;
		Profile->GetMappedKeysInRow(Name, Keys);
		return Keys.FilterByPredicate([](const FKey& Key) { return Key.IsGamepadKey(); });
	};
	// 先にジャンプのパッドを別のパッドのキーに変えておく。キーボードを戻しても、この変更は残る。
	Remapper.Remap(Jump, EKeys::Gamepad_FaceButton_Top, true);
	const TArray<FKey> PadBefore = PadKeys(Jump);
	TestTrue(TEXT("The gamepad key of Jump was changed"), PadBefore.Contains(EKeys::Gamepad_FaceButton_Top));

	// ジャンプをC、しゃがみをSpaceにする。ジャンプを戻すと、しゃがみとかぶる。
	TestTrue(TEXT("Jump to C"), TB::Remap(PC, Jump, EKeys::C).Result == TB::ERemapResult::OK);
	TestTrue(TEXT("Crouch to Space"), TB::Remap(PC, HumanCrouch, EKeys::SpaceBar).Result == TB::ERemapResult::OK);
	const TB::FRemapResult Refused = TB::ResetToDefault(PC, Jump);
	TestTrue(TEXT("Resetting Jump now conflicts"), Refused.Result == TB::ERemapResult::Conflict);
	TestTrue(TEXT("Resetting Jump conflicts with Human_Crouch"), Refused.Conflicting.Num() == 1 && Refused.Conflicting[0] == HumanCrouch);
	TestEqual(TEXT("Jump stays C"), Label(Jump), FString(TEXT("C")));

	// しゃがみを戻せば、ジャンプも戻せる。
	TestTrue(TEXT("Reset Human_Crouch"), TB::ResetToDefault(PC, HumanCrouch).Result == TB::ERemapResult::OK);
	TestEqual(TEXT("Human_Crouch is back to Ctrl"), Label(HumanCrouch), FString(TEXT("Ctrl")));
	TestTrue(TEXT("Reset Jump"), TB::ResetToDefault(PC, Jump).Result == TB::ERemapResult::OK);
	TestEqual(TEXT("Jump is back to Space"), Label(Jump), FString(TEXT("Space")));

	// パッドのキーは変わらない。
	TestTrue(TEXT("The changed gamepad key of Jump stays after resets"), PadKeys(Jump) == PadBefore);
	TestTrue(TEXT("Reset all succeeds"), TB::ResetAll(PC).Result == TB::ERemapResult::OK);
	TestTrue(TEXT("Reset all keeps the changed gamepad key"), PadKeys(Jump) == PadBefore);
	return true;
}

// 入れ替え。相手が複数なら断る。範囲が非対称でも、入れ替えたあとにかぶりが残るなら断る。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRemapSwapTest, "ToonStory.Input.RemapSwap",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBRemapSwapTest::RunTest(const FString& Parameters)
{
	auto* PC = RemapTestController();
	auto* User = RemapTestUser(PC);
	if (!TestNotNull(TEXT("Player controller"), PC) || !TestNotNull(TEXT("User settings"), User)) return false;

	using namespace TB::Keys;
	FTBKeyRemapper Remapper(*this, User);
	if (!RememberAll(Remapper)) return false;
	const auto Label = [&](FName Name) { return TB::KeyLabel(PC, Name); };

	// ジャンプをEにすると人間と人形のEの2行にかぶる。断り、何も変えない。
	const TB::FRemapResult Multiple = TB::SwapRemap(PC, Jump, EKeys::E);
	TestTrue(TEXT("Swap with two rows is refused"), Multiple.Result == TB::ERemapResult::Conflict);
	TestTrue(TEXT("Both rows are reported"), Multiple.Conflicting.Contains(HumanInteract) && Multiple.Conflicting.Contains(ToyInteract));
	TestEqual(TEXT("Jump is unchanged"), Label(Jump), FString(TEXT("Space")));
	TestEqual(TEXT("Human_Interact is unchanged"), Label(HumanInteract), FString(TEXT("E")));

	// しゃがみをRにする（ロビーとは範囲が別なのでOK）。ロビーの準備をSpaceにしてジャンプと入れ替えると、
	// ジャンプがRになってしゃがみとかぶる。断り、何も変えない。
	TestTrue(TEXT("Human_Crouch to R"), TB::Remap(PC, HumanCrouch, EKeys::R).Result == TB::ERemapResult::OK);
	const TB::FRemapResult Asymmetric = TB::SwapRemap(PC, LobbyReady, EKeys::SpaceBar);
	TestTrue(TEXT("Swap that leaves a conflict is refused"), Asymmetric.Result == TB::ERemapResult::Conflict);
	TestTrue(TEXT("The left conflict is Human_Crouch"), Asymmetric.Conflicting.Contains(HumanCrouch));
	TestEqual(TEXT("Lobby_Ready is unchanged"), Label(LobbyReady), FString(TEXT("R")));
	TestEqual(TEXT("Jump is unchanged by the refusal"), Label(Jump), FString(TEXT("Space")));

	// しゃがみを戻せば、同じ入れ替えが通る。
	TestTrue(TEXT("Human_Crouch back to Ctrl"), TB::Remap(PC, HumanCrouch, EKeys::LeftControl).Result == TB::ERemapResult::OK);
	TestTrue(TEXT("The swap now works"), TB::SwapRemap(PC, LobbyReady, EKeys::SpaceBar).Result == TB::ERemapResult::OK);
	TestEqual(TEXT("Lobby_Ready got Space"), Label(LobbyReady), FString(TEXT("Space")));
	TestEqual(TEXT("Jump got R"), Label(Jump), FString(TEXT("R")));
	return true;
}

namespace
{
	constexpr int32 RemapPressNewFrame = 4;
	constexpr int32 RemapCheckNewFrame = 8;
	constexpr int32 RemapPressOldFrame = 12;
	constexpr int32 RemapCheckOldFrame = 16;
	constexpr double RemapKeepAliveSeconds = 1000.0;

	void SendRemapKey(ATBController* PC, const FKey& Key, EInputEvent Event)
	{
		FInputKeyEventArgs Args(nullptr, FInputDeviceId::CreateFromInternalId(0), Key, Event == IE_Released ? 0.0 : 1.0, 0.016f, 1, 0);
		Args.Event = Event;
		PC->PlayerInput->InputKey(Args);
	}

	// しゃがみをCに替えると、Cでしゃがみが要求され、左Ctrlでは要求されない。入力の割り当ての作り直しは次の処理なので、少し待つ。
	class FTBRemapWorksCommand : public IAutomationLatentCommand
	{
	public:
		FTBRemapWorksCommand(ATBController* InPC, FAutomationTestBase* InTest) : PC(InPC), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			auto* Body = PC.IsValid() ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
			auto* State = PC.IsValid() ? PC->GetWorld()->GetGameState<ATBGameState>() : nullptr;
			auto* User = PC.IsValid() ? RemapTestUser(PC.Get()) : nullptr;
			if (!Body || !State || !User)
			{
				Test->AddError(TEXT("Pawn, game state or user settings is gone"));
				Finish();
				return true;
			}
			if (!bStarted)
			{
				bStarted = true;
				OriginalTeam = Body->TBPS()->Team;
				Body->TBPS()->Team = ETBTeam::Human;
				PC->SyncInputContexts();
				Remapper = MakeUnique<FTBKeyRemapper>(*Test, User);
				Remapper->Remember(TB::Keys::HumanCrouch);
				Test->TestTrue(TEXT("Remap Human_Crouch to C"),
				               TB::Remap(PC.Get(), TB::Keys::HumanCrouch, EKeys::C).Result == TB::ERemapResult::OK);
			}
			const double KeepUntil = State->GetServerWorldTimeSeconds() + RemapKeepAliveSeconds;
			if (!Playing.IsSet()) Playing.Emplace(*State, KeepUntil);
			Playing->Hold(KeepUntil);
			auto* Movement = Body->GetCharacterMovement();
			switch (Frame)
			{
			case RemapPressNewFrame:
				SendRemapKey(PC.Get(), EKeys::C, IE_Pressed);
				break;
			case RemapCheckNewFrame:
				Test->TestTrue(TEXT("The new key C requests crouch"), Movement->bWantsToCrouch);
				Test->TestEqual(TEXT("Label shows C"), TB::KeyLabel(PC.Get(), TB::Keys::HumanCrouch), FString(TEXT("C")));
				SendRemapKey(PC.Get(), EKeys::C, IE_Released);
				break;
			case RemapPressOldFrame:
				SendRemapKey(PC.Get(), EKeys::LeftControl, IE_Pressed);
				break;
			case RemapCheckOldFrame:
				Test->TestFalse(TEXT("The old key left Ctrl no longer requests crouch"), Movement->bWantsToCrouch);
				Finish();
				return true;
			default:
				break;
			}
			++Frame;
			return false;
		}

	private:
		// 押したキーを離し、陣営とフェーズを戻し、キー割り当てを元に書き戻す。
		void Finish()
		{
			if (!PC.IsValid()) return;
			SendRemapKey(PC.Get(), EKeys::C, IE_Released);
			SendRemapKey(PC.Get(), EKeys::LeftControl, IE_Released);
			Remapper.Reset();
			if (auto* Body = Cast<ATBCharacter>(PC->GetPawn()); Body && bStarted)
			{
				Body->TBPS()->Team = OriginalTeam;
				Body->GetCharacterMovement()->bWantsToCrouch = false;
			}
			Playing.Reset();
			PC->SyncInputContexts();
		}

		TWeakObjectPtr<ATBController> PC;
		FAutomationTestBase* Test;
		TUniquePtr<FTBKeyRemapper> Remapper;
		int32 Frame = 0;
		bool bStarted = false;
		ETBTeam OriginalTeam = ETBTeam::None;
		TOptional<TBTest::FPlayingScope> Playing;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRemapWorksTest, "ToonStory.Input.RemapWorks",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBRemapWorksTest::RunTest(const FString& Parameters)
{
	auto* PC = RemapTestController();
	if (!TestNotNull(TEXT("Player controller"), PC)) return false;
	if (!TestTrue(TEXT("Pawn"), Cast<ATBCharacter>(PC->GetPawn()) != nullptr)) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FTBRemapWorksCommand(PC, this));
	return true;
}
#endif
