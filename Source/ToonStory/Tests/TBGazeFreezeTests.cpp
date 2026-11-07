#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/TBTestMatch.h"
#include "Character/TBCharacter.h"
#include "Character/TBMovement.h"
#include "Character/TBNPCController.h"
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Data/TBRuleData.h"
#include "GamePlay/TBRoomLighting.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

// 凝視で止まるまでの流れ。映るだけでは止まらない、凝視が続くと止まる、途中で外れたら戻る、解除a/b、NPC、壁越し、暗い部屋。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBGazeFreezeTest, "ToonStory.Gaze.Freeze",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBGazeFreezeTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
	auto* Toy = PC ? PC->GetPawn<ATBCharacter>() : nullptr;
	if (!TestNotNull(TEXT("Mode"), Mode) || !TestNotNull(TEXT("State"), State) || !TestNotNull(TEXT("Toy"), Toy) || !Toy->TBPS())
		return false;
	TBTest::FPlayingScope Playing(*State, World->GetTimeSeconds() + 600);
	auto* Info = Toy->TBPS();
	const auto OldTeam = Info->Team;
	const auto OldState = Info->ToyState;
	const bool OldFrozen = Info->bFrozen;
	const auto OldTransform = Toy->GetActorTransform();
	auto& Gaze = const_cast<UTBRuleData&>(TB::Rules()).Gaze;
	const auto OldGaze = Gaze;
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* HumanPC = World->SpawnActor<ATBController>();
	auto* Human = World->SpawnActor<ATBCharacter>(FVector(0, -300, 10000), FRotator::ZeroRotator, Spawn);
	auto* HumanPS = HumanPC->GetPlayerState<ATBPlayerState>();
	if (!HumanPS)
	{
		HumanPS = World->SpawnActor<ATBPlayerState>();
		HumanPC->PlayerState = HumanPS;
		State->AddPlayerState(HumanPS);
	}
	HumanPS->Team = ETBTeam::Human;
	HumanPC->Possess(Human);
	Human->ApplyState();
	Info->Team = ETBTeam::Toy;
	Info->ToyState = ETBToyState::Free;
	Info->bFrozen = false;
	Info->SetGaze(ETBGazeState::None);
	Toy->ApplyState();
	Toy->SetActorLocation(FVector(0, 0, 10000));
	auto* Blocker = World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Blocker);
	Blocker->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(80, 12, 120));
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->RegisterComponent();
	Blocker->SetActorLocation(FVector(0, 0, 20000));
	auto* Room = World->SpawnActor<ATBRoomLighting>(Toy->GetActorLocation(), FRotator::ZeroRotator, Spawn);
	Room->Bounds->SetBoxExtent(FVector(30, 30, 30));
	ATBNPCController* AI = nullptr;
	ON_SCOPE_EXIT
	{
		Gaze = OldGaze;
		Room->Destroy();
		Blocker->Destroy();
		if (AI)
		{
			if (auto* AIState = AI->GetPlayerState<ATBPlayerState>())
			{
				State->RemovePlayerState(AIState);
				AIState->Destroy();
			}
			AI->UnPossess();
			AI->Destroy();
		}
		HumanPC->UnPossess();
		Human->Destroy();
		State->RemovePlayerState(HumanPS);
		HumanPS->Destroy();
		HumanPC->Destroy();
		Info->Team = OldTeam;
		Info->ToyState = OldState;
		Info->bFrozen = OldFrozen;
		Info->SetGaze(ETBGazeState::None);
		Toy->ApplyState();
		Toy->SetActorTransform(OldTransform);
	};
	Gaze.FreezeDelaySeconds = .3f;
	Gaze.bReleaseWhenGazeEnds = false;
	const float Half = Gaze.FreezeDelaySeconds * .5f;
	auto LookAtToy = [&](float Yaw = 0.f)
	{
		HumanPC->SetControlRotation((Toy->GetActorLocation() - Human->GetPawnViewLocation()).Rotation() + FRotator(0, Yaw, 0));
	};
	auto Expect = [&](const TCHAR* What, ETBGazeState Expected)
	{
		TestEqual(What, Info->GazeState, Expected);
		TestEqual(*FString::Printf(TEXT("%s (frozen flag)"), What), Info->bFrozen, Expected == ETBGazeState::Frozen);
	};
	auto* Movement = CastChecked<UTBMovement>(Toy->GetCharacterMovement());
	LookAtToy();

	// 映るだけでは、いくら見られても止まらない。
	Mode->EvaluateGaze(5.f);
	Expect(TEXT("Seen without gaze does not freeze"), ETBGazeState::Seen);
	const float SeenSpeed = Movement->GetMaxSpeed();

	// 凝視が続くと止まる。止まる前は遅くなるだけで、作業はできる。
	Human->SetGazeRequested(true);
	Mode->EvaluateGaze(Half);
	Expect(TEXT("Gaze starts counting"), ETBGazeState::Gazed);
	TestTrue(TEXT("Gazed toy is slowed"), FMath::IsNearlyEqual(Movement->GetMaxSpeed(), SeenSpeed * Gaze.GazedToySpeedRatio, 1.f));
	TestTrue(TEXT("Gazed toy can still act"), Toy->CanAct());
	Mode->EvaluateGaze(Half);
	Expect(TEXT("Gaze for the delay freezes"), ETBGazeState::Frozen);

	// 解除a: 凝視をやめても、視界に入っている間は止まったまま。視界から外れたら解く。
	Human->SetGazeRequested(false);
	Mode->EvaluateGaze(1.f);
	Expect(TEXT("Release a: still frozen while seen"), ETBGazeState::Frozen);
	LookAtToy(180.f);
	Mode->EvaluateGaze(.01f);
	Expect(TEXT("Release a: thaws when out of sight"), ETBGazeState::None);
	TestTrue(TEXT("Thawed toy walks at full speed"), FMath::IsNearlyEqual(Movement->GetMaxSpeed(), SeenSpeed, 1.f));

	// 途中で凝視が外れたら止めずに戻し、数え直す。
	LookAtToy();
	Human->SetGazeRequested(true);
	Mode->EvaluateGaze(Half);
	Human->SetGazeRequested(false);
	Mode->EvaluateGaze(Half);
	Expect(TEXT("Broken gaze falls back to seen"), ETBGazeState::Seen);
	Human->SetGazeRequested(true);
	Mode->EvaluateGaze(Half);
	Expect(TEXT("Gaze restarts from zero"), ETBGazeState::Gazed);
	Mode->EvaluateGaze(Half);
	Expect(TEXT("Restarted gaze freezes after the full delay"), ETBGazeState::Frozen);

	// 解除b: 凝視が外れたら、視界に入っていても解く。
	Gaze.bReleaseWhenGazeEnds = true;
	Human->SetGazeRequested(false);
	Mode->EvaluateGaze(.01f);
	Expect(TEXT("Release b: thaws when the gaze ends"), ETBGazeState::Seen);
	Gaze.bReleaseWhenGazeEnds = false;

	// 凝視の範囲（中心の円）の外なら、画面に映っていても数えない。
	Human->SetGazeRequested(true);
	LookAtToy(Gaze.HalfAngle + 8.f);
	Mode->EvaluateGaze(1.f);
	Expect(TEXT("Toy outside the gaze cone is only seen"), ETBGazeState::Seen);
	LookAtToy();

	// 壁越しは凝視しても止まらない。
	Blocker->SetActorLocation(FVector(0, -150, 10000));
	Mode->EvaluateGaze(1.f);
	Expect(TEXT("Wall blocks gaze"), ETBGazeState::None);
	Blocker->SetActorLocation(FVector(0, 0, 20000));

	// 暗い部屋では3mより遠い凝視では止まらず、近づけば止まる。
	const float Meters = TB::Rules().Vision.DarkProtectionDistanceMeters;
	Room->SetLightsOn(false);
	Human->SetActorLocation(FVector(0, -(Meters * 100.f + 100.f), 10000));
	LookAtToy();
	Mode->EvaluateGaze(1.f);
	Expect(TEXT("Dark room protects beyond the distance"), ETBGazeState::None);
	Human->SetActorLocation(FVector(0, -(Meters * 100.f - 100.f), 10000));
	LookAtToy();
	Mode->EvaluateGaze(1.f);
	Expect(TEXT("Dark room does not protect up close"), ETBGazeState::Frozen);
	Room->SetLightsOn(true);
	Toy->SetFrozen(false);
	Human->SetGazeRequested(false);
	Mode->EvaluateGaze(.01f);

	// NPCは、見つけて見つめているおもちゃを凝視しているとみなす。
	Human->SetActorLocation(FVector(0, -300, 10000));
	HumanPC->UnPossess();
	AI = World->SpawnActor<ATBNPCController>();
	AI->SetActorTickEnabled(false);
	AI->Possess(Human);
	auto* AIState = AI->GetPlayerState<ATBPlayerState>();
	if (!TestNotNull(TEXT("NPC observer state"), AIState)) return false;
	AIState->Team = ETBTeam::Human;
	Human->ApplyState();
	AI->SetControlRotation((Toy->GetActorLocation() - Human->GetPawnViewLocation()).Rotation());
	Mode->EvaluateGaze(1.f);
	Expect(TEXT("Unnoticed toy is not frozen by NPC"), ETBGazeState::None);
	TestTrue(TEXT("NPC can see the toy"), AI->CanSee(Toy));
	AI->SetFocus(Toy); // 見つける前に向きが戻らないようにする。見つけるかはティックで決まる。
	AI->OverlookChance = 0;
	AI->RecognitionSeconds = 0;
	AI->Tick(.4f);
	AI->UpdateControlRotation(.016f);
	TestTrue(TEXT("NPC tracks the toy"), AI->IsTracking(Toy));
	Mode->EvaluateGaze(Half);
	Expect(TEXT("NPC tracking counts as gaze"), ETBGazeState::Gazed);
	Mode->EvaluateGaze(Half);
	Expect(TEXT("NPC tracking freezes after the delay"), ETBGazeState::Frozen);
	return true;
}
#endif
