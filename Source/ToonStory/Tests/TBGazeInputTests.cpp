#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/TBTestMatch.h"
#include "Character/TBCharacter.h"
#include "Character/TBMovement.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Data/TBCharacterData.h"
#include "Data/TBRuleData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

// 凝視の入力が移動パケットのフラグで届き、予測の巻き戻しでも保たれ、人間の歩く速さだけを落とすことを確かめる。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBGazeFlagTest, "ToonStory.Input.GazeFlag",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBGazeFlagTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	}
	auto* PC = World ? Cast<ATBController>(World->GetFirstPlayerController()) : nullptr;
	auto* Body = PC ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
	auto* State = World ? World->GetGameState<ATBGameState>() : nullptr;
	if (!TestNotNull(TEXT("Pawn"), Body) || !TestNotNull(TEXT("Player state"), Body->TBPS()) ||
	    !TestNotNull(TEXT("Game state"), State))
		return false;
	auto* Movement = CastChecked<UTBMovement>(Body->GetCharacterMovement());
	const ETBTeam OriginalTeam = Body->TBPS()->Team;
	const EMovementMode OriginalMode = Movement->MovementMode;
	TBTest::FPlayingScope Playing(*State);
	ON_SCOPE_EXIT
	{
		Body->SetGazeRequested(false);
		Body->TBPS()->Team = OriginalTeam;
		Body->ApplyState();
		Movement->SetMovementMode(OriginalMode);
	};
	Body->TBPS()->Team = ETBTeam::Human;
	Body->ApplyState();
	Movement->SetMovementMode(MOVE_Walking);
	const float Walk = Movement->GetMaxSpeed();
	TestTrue(TEXT("Human walks before gazing"), Walk > 0.f);

	Body->SetGazeRequested(true);
	TestTrue(TEXT("Human is gazing"), Body->IsGazing());
	TestEqual(TEXT("Gazing slows the walk"), Movement->GetMaxSpeed(), Walk * TB::Rules().Gaze.WalkSpeedRatio, .01f);

	// 移動パケットに凝視のフラグが乗る。ダッシュのフラグとは別。
	auto* Prediction = Movement->GetPredictionData_Client_Character();
	auto Saved = Prediction->AllocateNewMove();
	Saved->SetMoveFor(Body, .016f, FVector(1, 0, 0), *Prediction);
	TestTrue(TEXT("Gaze travels with movement packets"),
	         (Saved->GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_1) != 0);
	TestFalse(TEXT("Gaze does not set the sprint flag"),
	          (Saved->GetCompressedFlags() & FSavedMove_Character::FLAG_Custom_0) != 0);

	// サーバー側の受け取り。フラグが無ければ戻り、予測の巻き戻しでは押していた状態に戻る。
	Movement->UpdateFromCompressedFlags(0);
	TestFalse(TEXT("Released flag ends gazing"), Body->IsGazing());
	TestEqual(TEXT("Released flag restores the walk"), Movement->GetMaxSpeed(), Walk, .01f);
	Movement->UpdateFromCompressedFlags(FSavedMove_Character::FLAG_Custom_1);
	TestTrue(TEXT("Server reads the gaze flag"), Body->IsGazing());
	Movement->UpdateFromCompressedFlags(0);
	Saved->PrepMoveFor(Body);
	TestTrue(TEXT("Prediction replay restores gazing"), Body->IsGazing());

	// 人形には効かない。フラグが残っていても凝視とはみなさず、速さも変えない。
	Body->TBPS()->Team = ETBTeam::Toy;
	Movement->bWantsToSprint = false;
	const float ToyWalk = TB::CharacterData().Toy.WalkSpeed;
	Body->ApplyState();
	TestFalse(TEXT("Toy is never gazing"), Body->IsGazing());
	TestEqual(TEXT("Gaze flag does not slow a toy"), Movement->GetMaxSpeed(), ToyWalk, .01f);
	return true;
}
#endif
