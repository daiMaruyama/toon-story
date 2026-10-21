#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBNPCController.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameMode.h"
#include "Core/TBLobbySettings.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBController.h"
#include "GamePlay/TBPickup.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBDummyToy.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Character/TBNPCNavigation.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tests/AutomationCommon.h"
#include "UI/TBHUD.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"

// An isolated sight fixture checks both teams without relying on random patrol routes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNPCSightTest, "ToonStory.NPC.Sight",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNPCSightTest::RunTest(const FString& Parameters)
{
 UWorld* World = nullptr;
 for (const auto& Context : GEngine->GetWorldContexts())
  if (Context.WorldType == EWorldType::Game) World = Context.World();
 if (!World) return false;
 FActorSpawnParameters Spawn;
 Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Body = World->SpawnActor<ATBCharacter>(FVector(0, 0, 10000), FRotator::ZeroRotator, Spawn);
 auto* AI = World->SpawnActor<ATBNPCController>();
 AI->Possess(Body);
 AI->SetActorTickEnabled(false);
 AI->SetControlRotation(FRotator::ZeroRotator);
 const FVector Eye = Body->GetPawnViewLocation();
 auto* Battery = World->SpawnActor<ATBPickup>(Eye + FVector(500, 0, -15), FRotator::ZeroRotator, Spawn);
 auto* Dummy = World->SpawnActor<ATBDummyToy>(Eye + FVector(500, 0, -15), FRotator::ZeroRotator, Spawn);
 TestTrue(TEXT("Visible battery is detected"), AI->CanSee(Battery));
 TestTrue(TEXT("Visible dummy is detected under the same conditions"), AI->CanSee(Dummy));
 AI->SetControlRotation(FRotator(0, 180, 0));
 TestFalse(TEXT("Battery behind NPC is not detected"), AI->CanSee(Battery));
 TestFalse(TEXT("Dummy behind NPC is not detected"), AI->CanSee(Dummy));
 AI->SetControlRotation(FRotator::ZeroRotator);
 auto* Wall = World->SpawnActor<AActor>();
 auto* Collision = NewObject<UBoxComponent>(Wall);
 Wall->SetRootComponent(Collision);
 Collision->SetBoxExtent(FVector(20, 200, 200));
 Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
 Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
 Collision->RegisterComponent();
 Wall->SetActorLocation(Eye + FVector(250, 0, 0));
 TestFalse(TEXT("Wall conceals battery"), AI->CanSee(Battery));
 TestFalse(TEXT("Wall conceals dummy"), AI->CanSee(Dummy));
 Wall->Destroy();
 Battery->SetActorLocation(Eye + FVector(1500, 0, -15));
 TestFalse(TEXT("Distant battery is not detected"), AI->CanSee(Battery));
 Battery->Destroy(); Dummy->Destroy(); Body->Destroy(); AI->Destroy();
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNPCDummyTest, "ToonStory.NPC.DummyCapture",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNPCDummyTest::RunTest(const FString& Parameters)
{
 UWorld* World = nullptr;
 for (const auto& Context : GEngine->GetWorldContexts())
  if (Context.WorldType == EWorldType::Game) World = Context.World();
 if (!World || World->GetNetMode() != NM_Standalone) return false;
 auto* Mode = World->GetAuthGameMode<ATBGameMode>();
 auto* State = World->GetGameState<ATBGameState>();
 auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
 if (!Mode || !State || !PC || State->PlayerArray.Num() != 1) return false;
 Mode->SyncLobbyCounts();
 const auto Original = State->Settings;
 auto Rules = Original; Rules.NPCHumans = 1; Rules.NPCToys = 1;
 Mode->SetRules(PC, Rules);
 PC->GetPlayerState<ATBPlayerState>()->bReady = true;
 Mode->StartRound(PC);
 ATBNPCController* Human = nullptr;
 ATBCharacter* ToyBody = nullptr;
 for (TActorIterator<ATBNPCController> It(World); It; ++It)
 {
  It->SetActorTickEnabled(false);
  auto* Body = CastChecked<ATBCharacter>(It->GetPawn());
  if (Body->IsHuman()) Human = *It;
  else ToyBody = Body;
 }
 if (!Human || !ToyBody || !State->Box) return false;
 auto* HumanBody = CastChecked<ATBCharacter>(Human->GetPawn());
 const FTransform Box = State->Box->GetActorTransform();
 HumanBody->TeleportTo(Box.TransformPosition(FVector(-350, 0, 70)), Box.Rotator());
 ToyBody->TeleportTo(Box.TransformPosition(FVector(-50, 100, 30)), Box.Rotator());
 PC->GetPawn()->SetActorLocation(FVector(2500,2500,70));
 PC->SetControlRotation(FRotator::ZeroRotator);
 FActorSpawnParameters Spawn;
 Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Dummy = World->SpawnActor<ATBDummyToy>(Box.TransformPosition(FVector(-220,0,30)), Box.Rotator(), Spawn);
 Human->SetControlRotation((Dummy->GetActorLocation() - HumanBody->GetPawnViewLocation()).Rotation());
 TestTrue(TEXT("Closer dummy is visible"), Human->CanSee(Dummy));
 TestTrue(TEXT("Real toy is also visible"), Human->CanSee(ToyBody));
 Human->OverlookChance = 1;
 Human->Tick(.4f);
 TestFalse(TEXT("Overlooked dummy is not captured"), HumanBody->HasCarriedToy());
 TestFalse(TEXT("Unnoticed real toy is not frozen"), Mode->Watched(ToyBody, HumanBody));
 TestEqual(TEXT("Human NPC walks at half normal speed"), HumanBody->GetCharacterMovement()->GetMaxSpeed(),
  HumanBody->GetCharacterMovement()->MaxWalkSpeed * .5f);
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Human, HumanBody, Dummy, ToyBody]()
 {
  Human->OverlookChance = 0;
  Human->SetControlRotation((Dummy->GetActorLocation() - HumanBody->GetPawnViewLocation()).Rotation());
  // Isolate the dummy so the nearby real toy cannot become an alternative target.
  ToyBody->SetActorLocation(FVector(2500, 2400, 30));
  Human->Tick(.4f);
  TestFalse(TEXT("Overlook cooldown prevents immediate retry"), HumanBody->HasCarriedToy());
  TestFalse(TEXT("Overlooked target is not tracked"), Human->IsTracking(Dummy));
  return true;
 }));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(7.f));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Human, HumanBody, Dummy, ToyBody, Box]()
 {
  ToyBody->TeleportTo(Box.TransformPosition(FVector(-50, 100, 30)), Box.Rotator());
  Human->StopMovement();
  HumanBody->TeleportTo(Box.TransformPosition(FVector(-350, 0, 70)), Box.Rotator());
  Human->SetControlRotation((Dummy->GetActorLocation() - HumanBody->GetPawnViewLocation()).Rotation());
  Human->Tick(.4f);
  TestFalse(TEXT("Recognition delay prevents instant capture"), HumanBody->HasCarriedToy());
  return true;
 }));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Human, HumanBody, Dummy]()
 {
  Human->SetControlRotation((Dummy->GetActorLocation() - HumanBody->GetPawnViewLocation()).Rotation());
  Human->Tick(.4f);
  TestTrue(TEXT("Human selects closer dummy even with a real toy in view"), HumanBody->CarriedDummy == Dummy || Human->GetFocusActor() == Dummy);
  Human->SetActorTickEnabled(true);
  return true;
 }));
 const double Deadline = FPlatformTime::Seconds() + 40;
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Dummy, Deadline]()
 {
  if (!Dummy->StoredBox && FPlatformTime::Seconds() < Deadline) return false;
  TestNotNull(TEXT("Human captures and stores a dummy"), Dummy->StoredBox.Get());
  return true;
 }));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Mode, PC, State, Original, Dummy]()
 {
  Mode->Finish(ETBWinner::Humans, TEXT("NPC dummy test"));
  Mode->ReturnToLobby(PC);
  Dummy->Destroy();
  State->Settings = Original;
  Mode->SyncLobbyCounts();
  return true;
 }));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNPCFieldTransportTest, "ToonStory.NPC.FieldTransport",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNPCFieldTransportTest::RunTest(const FString& Parameters)
{
 UWorld* World = nullptr;
 for (const auto& Context : GEngine->GetWorldContexts())
  if (Context.WorldType == EWorldType::Game) World = Context.World();
 if (!World || World->GetNetMode() != NM_Standalone) return false;
 auto* Mode = World->GetAuthGameMode<ATBGameMode>();
 auto* State = World->GetGameState<ATBGameState>();
 auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
 if (!Mode || !State || !PC || State->PlayerArray.Num() != 1) return false;
 PC->ServerPreference(ETBTeam::Human);
 auto Rules = State->Settings; Rules.NPCHumans = 2; Rules.NPCToys = 5;
 Mode->SetRules(PC, Rules);
 PC->GetPlayerState<ATBPlayerState>()->bReady = true;
 Mode->StartRound(PC);
 State->Settings.RequiredItems = 100;
 const double Deadline = FPlatformTime::Seconds() + 100;
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, Deadline]()
 {
  if ((State->BoxedCount < 2 || State->Collected < 1) && FPlatformTime::Seconds() < Deadline) return false;
  TestTrue(TEXT("Normal spawns: humans deliver at least two toys"), State->BoxedCount >= 2);
  TestTrue(TEXT("Normal spawns: toys discover and collect a battery"), State->Collected >= 1);
  AddInfo(FString::Printf(TEXT("Field boxed=%d collected=%d"), State->BoxedCount, State->Collected));
  return true;
 }));
 return true;
}

// Same map/camera, first without NPCs, then with the maximum seven. Captures include rendering.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNPCPerfTest, "ToonStory.NPC.PerformanceCapture",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNPCPerfTest::RunTest(const FString& Parameters)
{
 UWorld* World = nullptr;
 for (const auto& Context : GEngine->GetWorldContexts())
  if (Context.WorldType == EWorldType::Game) World = Context.World();
 if (!World || World->GetNetMode() != NM_Standalone) return false;
 auto* Mode = World->GetAuthGameMode<ATBGameMode>();
 auto* State = World->GetGameState<ATBGameState>();
 auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
 if (!Mode || !State || !PC || State->PlayerArray.Num() != 1) return false;
 Mode->SyncLobbyCounts();
 auto Camera = PC->GetPawn()->GetActorTransform();
 auto Look = PC->GetControlRotation();
 State->Phase = ETBPhase::Playing;
 State->Settings.RequiredItems = 100;
 State->EndTime = World->GetTimeSeconds() + 600;
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(10));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
 {
  FCsvProfiler::Get()->BeginCapture(-1, FPaths::ProjectSavedDir() / TEXT("Profiling"), TEXT("NPC-0.csv"));
  return true;
 }));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(15));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]() { FCsvProfiler::Get()->EndCapture(); return true; }));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Mode, State, PC, Camera, Look]()
 {
  State->Phase = ETBPhase::Lobby;
  auto Rules = State->Settings;
  Rules.RequiredItems = 5;
  Rules.NPCHumans = 2;
  Rules.NPCToys = 5;
  Mode->SetRules(PC, Rules);
  PC->GetPlayerState<ATBPlayerState>()->bReady = true;
  Mode->StartRound(PC);
  TestEqual(TEXT("Seven NPCs spawned for comparison"), State->PlayerArray.Num(), 8);
  State->Settings.RequiredItems = 100;
  PC->GetPawn()->SetActorTransform(Camera);
  PC->SetControlRotation(Look);
  return true;
 }));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(15));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
 {
  TestEqual(TEXT("NPC performance capture runs during match"), State->Phase, ETBPhase::Playing);
  FCsvProfiler::Get()->BeginCapture(-1, FPaths::ProjectSavedDir() / TEXT("Profiling"), TEXT("NPC-7.csv"));
  return true;
 }));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(15));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]() { FCsvProfiler::Get()->EndCapture(); return true; }));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3));
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]() { TestEqual(TEXT("Match remains active during measurement"), State->Phase, ETBPhase::Playing); return true; }));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNPCSoloStartTest, "ToonStory.NPC.SoloStart",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNPCSoloStartTest::RunTest(const FString& Parameters)
{
 UWorld* World = nullptr;
 for (const auto& Context : GEngine->GetWorldContexts())
  if (Context.WorldType == EWorldType::Game) World = Context.World();
 if (!World || World->GetNetMode() != NM_Standalone) return false;
 auto* Mode = World->GetAuthGameMode<ATBGameMode>();
 auto* State = World->GetGameState<ATBGameState>();
 auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
 if (!Mode || !State || !PC || State->PlayerArray.Num() != 1) return false;
 Mode->SyncLobbyCounts();
 const auto OriginalRules = State->Settings;
 for (int32 HumanNPCs : {1, 0})
 {
  auto Rules = State->Settings;
  Rules.NPCHumans = HumanNPCs;
  Rules.NPCToys = 1 - HumanNPCs;
  Mode->SetRules(PC, Rules);
  TestEqual(TEXT("Only connected players require ready"), State->Settings.Humans + State->Settings.Toys, 1);
  TestEqual(TEXT("Player fills the team without an NPC"), State->Settings.Toys, HumanNPCs);
  Mode->StartRound(PC);
  TestEqual(TEXT("Unready player still prevents start"), State->Phase, ETBPhase::Lobby);
  PC->GetPlayerState<ATBPlayerState>()->bReady = true;
  Mode->StartRound(PC);
  if (!TestEqual(TEXT("One ready player and one NPC can start"), State->Phase, ETBPhase::Playing)) return false;
  TestEqual(TEXT("One NPC spawned"), State->PlayerArray.Num(), 2);
  TestEqual(TEXT("Player assigned opposite team"), PC->GetPlayerState<ATBPlayerState>()->Team,
   HumanNPCs ? ETBTeam::Toy : ETBTeam::Human);
  Mode->Finish(ETBWinner::Humans, TEXT("Solo start test"));
  Mode->ReturnToLobby(PC);
  TestEqual(TEXT("NPC removed before rematch"), State->PlayerArray.Num(), 1);
 }
 State->Settings = OriginalRules;
 Mode->SyncLobbyCounts();
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLobbyPreferenceTest, "ToonStory.NPC.LobbyPreference",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBLobbyPreferenceTest::RunTest(const FString& Parameters)
{
 UWorld* World = nullptr;
 for (const auto& Context : GEngine->GetWorldContexts())
  if (Context.WorldType == EWorldType::Game) World = Context.World();
 if (!World || World->GetNetMode() != NM_Standalone) return false;
 auto* Mode = World->GetAuthGameMode<ATBGameMode>();
 auto* State = World->GetGameState<ATBGameState>();
 auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
 auto* HUD = PC ? Cast<ATBHUD>(PC->GetHUD()) : nullptr;
 if (!Mode || !State || !PC || !HUD || State->PlayerArray.Num() != 1) return false;
 auto* Host = PC->GetPlayerState<ATBPlayerState>();
 const auto Original = State->Settings;
 const auto OriginalPreference = Host->Preference;
 auto* Guest = World->SpawnActor<ATBPlayerState>();
 State->AddPlayerState(Guest);
 Host->bReady = Guest->bReady = true;
 State->Settings.NPCHumans = State->Settings.NPCToys = 0;
 HUD->NotifyHitBoxClick(TEXT("CloseSettings"));
 for (ETBTeam Team : {ETBTeam::Toy, ETBTeam::Human})
 {
  Guest->Preference = Team;
  PC->ServerPreference(Team);
  TestEqual(TEXT("Both explicit choices determine the player count"),
   Team == ETBTeam::Toy ? State->Settings.Toys : State->Settings.Humans, 2);
  HUD->NotifyHitBoxClick(TEXT("Start"));
  TestEqual(TEXT("Single-team lobby cannot start from closed settings"), State->Phase, ETBPhase::Lobby);
 }
 State->RemovePlayerState(Guest);
 Guest->Destroy();
 auto Rules = State->Settings;
 Rules.Humans = 1; Rules.Toys = 0; Rules.NPCHumans = 1; Rules.NPCToys = 1;
 Mode->SetRules(PC, Rules);
 for (ETBTeam Team : {ETBTeam::Toy, ETBTeam::Human})
 {
  PC->ServerPreference(Team);
  TestEqual(TEXT("Solo host choice updates lobby counts with both NPC teams"),
   Team == ETBTeam::Toy ? State->Settings.Toys : State->Settings.Humans, 1);
  Host->bReady = false;
  HUD->NotifyHitBoxClick(TEXT("CloseSettings"));
  HUD->NotifyHitBoxClick(TEXT("Start"));
  TestEqual(TEXT("Closed settings still requires ready"), State->Phase, ETBPhase::Lobby);
  Host->bReady = true;
  HUD->NotifyHitBoxClick(TEXT("Start"));
  TestEqual(TEXT("Closed settings start reaches the server"), State->Phase, ETBPhase::Playing);
  TestEqual(TEXT("Host receives the selected team"), Host->Team, Team);
  Mode->Finish(ETBWinner::Humans, TEXT("Preference test"));
  Mode->ReturnToLobby(PC);
 }
 Host->Preference = OriginalPreference;
 State->Settings = Original;
 Mode->SyncLobbyCounts();
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNPCMatchTest, "ToonStory.NPC.MatchLifecycle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBNPCMatchTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
	if (!Mode || !State || !PC) return false;
	if (!TestTrue(TEXT("Run NPC lifecycle in a fresh stage lobby"), State->Phase == ETBPhase::Lobby && State->PlayerArray.Num() == 1)) return false;
	Mode->SyncLobbyCounts();
	const auto OriginalRules = State->Settings;
	auto Rules = State->Settings;
	Rules.NPCHumans = 1;
	Rules.NPCToys = 1;
	Mode->SetRules(PC, Rules);
	TestEqual(TEXT("Host can select human NPC count"), State->Settings.NPCHumans, 1);
	TestEqual(TEXT("Host can select toy NPC count"), State->Settings.NPCToys, 1);
	Rules.NPCToys = -1;
	Mode->SetRules(PC, Rules);
	TestEqual(TEXT("Negative NPC count rejected"), State->Settings.NPCToys, 1);
	Rules.NPCToys = 7;
	Mode->SetRules(PC, Rules);
	TestEqual(TEXT("More than eight total participants rejected"), State->Settings.NPCToys, 1);
	PC->GetPlayerState<ATBPlayerState>()->bReady = true;
	Mode->StartRound(PC);
	if (!TestEqual(TEXT("Solo host can start with NPCs"), State->Phase, ETBPhase::Playing)) return false;
	ATBNPCController* Human = nullptr;
	ATBNPCController* Toy = nullptr;
	for (TActorIterator<ATBNPCController> It(World); It; ++It)
	{
		if (It->GetPlayerState<ATBPlayerState>()->Team == ETBTeam::Human) Human = *It;
		else Toy = *It;
	}
	if (!TestNotNull(TEXT("Human NPC"), Human) || !TestNotNull(TEXT("Toy NPC"), Toy)) return false;
	TestEqual(TEXT("NPCs count as match participants"), State->PlayerArray.Num(), 3);
	Human->OverlookChance = 0;
	Human->RecognitionSeconds = 0;
	Human->SetActorTickEnabled(false);
	auto* HumanBody = CastChecked<ATBCharacter>(Human->GetPawn());
	auto* ToyBody = CastChecked<ATBCharacter>(Toy->GetPawn());
	HumanBody->TeleportTo(FVector(2300, 2300, 70), FRotator(0, 0, 0));
	PC->GetPawn()->TeleportTo(FVector(2500, 2300, 70), FRotator::ZeroRotator);
	PC->SetControlRotation(FRotator::ZeroRotator);
	if (TBLobbySettings::StageIndex(World) == INDEX_NONE)
		ToyBody->TeleportTo(FVector(-1900, -1900, 30), FRotator::ZeroRotator);
	const int32 CollectionTarget = TBLobbySettings::StageIndex(World) == 1 ? 5 : 1;
	State->Settings.RequiredItems = 10; // Keep playing for the storage phase after collection.
	const double Deadline = FPlatformTime::Seconds() + 150;
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, World, State, ToyBody, Deadline, CollectionTarget]()
	{
		if (State->Collected < CollectionTarget && FPlatformTime::Seconds() < Deadline) return false;
		if (State->Collected < CollectionTarget)
		{
			auto* AI = Cast<ATBNPCController>(ToyBody->GetController());
			const auto& Agent = AI->GetNavAgentPropertiesRef();
			AddInfo(FString::Printf(TEXT("Toy stopped at %s canAct=%d frozen=%d move=%d agent=%.1f/%.1f/%.1f contact=%s"), *ToyBody->GetActorLocation().ToString(), ToyBody->CanAct(), ToyBody->IsGazeFrozen(), int32(AI->GetMoveStatus()), Agent.AgentRadius, Agent.AgentHeight, Agent.AgentStepHeight, *GetNameSafe(ToyBody->ContactItem)));
			for (TActorIterator<ATBPickup> It(World); It; ++It)
			{
				auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, ToyBody->GetActorLocation(), It->GetActorLocation(), AI);
				AddInfo(FString::Printf(TEXT("Toy path %s valid=%d partial=%d"), *It->GetName(), Path && Path->IsValid(), Path && Path->IsPartial()));
			}
		}
		TestTrue(FString::Printf(TEXT("Toy collects %d batteries (actual %d)"), CollectionTarget, State->Collected), State->Collected >= CollectionTarget);
		if (TBLobbySettings::StageIndex(World) == INDEX_NONE)
		{
			auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, FVector(-1900,-1900,30), FVector(-950,-2200,15));
			TestTrue(TEXT("Runtime navmesh produces a complete route"), Path && Path->IsValid() && !Path->IsPartial());
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([World, State, Human, HumanBody, Toy, ToyBody]()
	{
		Toy->StopMovement();
		Toy->SetActorTickEnabled(false);
		ToyBody->SetFrozen(false);
		HumanBody->TeleportTo(State->Box->GetActorTransform().TransformPosition(FVector(-350, 0, 70)), State->Box->GetActorRotation());
		ToyBody->TeleportTo(State->Box->GetActorTransform().TransformPosition(FVector(-220, 0, 30)), State->Box->GetActorRotation());
		Human->SetFocus(ToyBody);
		Human->SetControlRotation((ToyBody->GetActorLocation() - HumanBody->GetPawnViewLocation()).Rotation());
		Human->SetActorTickEnabled(true);
		return true;
	}));
	// Deadline is initialized when this phase actually begins.
	const auto StorageDeadline = MakeShared<double>(0);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, HumanBody, ToyBody, StorageDeadline]()
	{
		if (*StorageDeadline == 0) *StorageDeadline = FPlatformTime::Seconds() + 20;
		if (ToyBody->TBPS()->ToyState != ETBToyState::Boxed && FPlatformTime::Seconds() < *StorageDeadline) return false;
		AddInfo(FString::Printf(TEXT("Storage: human=%s toy=%s state=%d holding=%d store=%.2f inRange=%d"), *HumanBody->GetActorLocation().ToString(), *ToyBody->GetActorLocation().ToString(), int32(ToyBody->TBPS()->ToyState), HumanBody->bHoldingInteract, HumanBody->StoreStarted, State->Box->CanStoreFrom(HumanBody)));
		TestEqual(TEXT("Human NPC captures, walks to box and stores toy"), ToyBody->TBPS()->ToyState, ETBToyState::Boxed);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, World, Mode, State, PC, OriginalRules]()
	{
		Mode->Finish(ETBWinner::Humans, TEXT("NPC test"));
		Mode->ReturnToLobby(PC);
		TestEqual(TEXT("Return to lobby removes bot player states"), State->PlayerArray.Num(), 1);
		TestFalse(TEXT("Return to lobby removes NPC controllers"), bool(TActorIterator<ATBNPCController>(World)));
		TestEqual(TEXT("NPC selection retained for rematch"), State->Settings.NPCToys, 1);
		State->Settings = OriginalRules;
		Mode->SyncLobbyCounts();
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNPCNavigationTest, "ToonStory.NPC.StageNavigation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNPCNavigationTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World || World->GetNetMode() != NM_Standalone) return false;
	if (!TestTrue(TEXT("Runtime navigation bounds prepared"), TBNPCNavigation::Prepare(World))) return false;
	const double Deadline = FPlatformTime::Seconds() + 30;
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, World, Deadline]()
	{
		auto* Nav = UNavigationSystemV1::GetCurrent(World);
		if (FPlatformTime::Seconds() < Deadline &&
		    (!Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) ||
		     UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))) return false;
		TArray<FVector> Starts;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Starts.Add(It->GetActorLocation());
			FNavLocation Projected;
			const auto* Data = Nav->GetNavDataForProps(FNavAgentProperties(7.5f, 19.5f), It->GetActorLocation());
			const bool OnNav = Data && Nav->ProjectPointToNavigation(It->GetActorLocation(), Projected, FVector(100,100,500), Data);
			AddInfo(FString::Printf(TEXT("Start %s: %s projected=%d %s"), *It->GetName(), *It->GetActorLocation().ToString(), OnNav, *Projected.Location.ToString()));
		}
		AddInfo(FString::Printf(TEXT("Navigation building=%d"), UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World)));
		int32 Items = 0, Reachable = 0;
		auto Reaches = [&](const FVector& Destination, bool Human)
		{
			for (const FVector& Start : Starts)
			{
				const FNavAgentProperties Agent = Human ? FNavAgentProperties(26, 130) : FNavAgentProperties(7.5f, 19.5f);
				auto* Data = Nav->GetNavDataForProps(Agent, Start);
				auto* Path = Data ? UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, Destination, const_cast<ANavigationData*>(Data)) : nullptr;
				if (Path && Path->IsValid() && !Path->IsPartial()) return true;
			}
			return false;
		};
		for (TActorIterator<ATBPickup> It(World); It; ++It)
		{
			++Items;
			FHitResult Floor;
			World->LineTraceSingleByChannel(Floor, It->GetActorLocation() + FVector(0,0,15), It->GetActorLocation() - FVector(0,0,100), ECC_Visibility);
			FNavLocation Projected;
			const auto* Data = Nav->GetNavDataForProps(FNavAgentProperties(7.5f,19.5f));
			bool ProjectedOK = Data && Nav->ProjectPointToNavigation(It->GetActorLocation(), Projected, FVector(40,40,50), Data);
			AddInfo(FString::Printf(TEXT("Pickup %s floor=%s z=%.1f nav=%d %s"), *It->GetName(), *GetNameSafe(Floor.GetActor()), Floor.ImpactPoint.Z, ProjectedOK, *Projected.Location.ToString()));
			if (Reaches(It->GetActorLocation(), false)) ++Reachable;
			else AddInfo(FString::Printf(TEXT("Battery has no walking route: %s %s"), *It->GetName(), *It->GetActorLocation().ToString()));
		}
		AddInfo(FString::Printf(TEXT("%s: reachable batteries %d/%d"), *World->GetMapName(), Reachable, Items));
		TestTrue(TEXT("Enough batteries for the default match have complete walking routes"), Reachable >= World->GetGameState<ATBGameState>()->Settings.RequiredItems);
		for (TActorIterator<ATBBox> It(World); It; ++It)
			TestTrue(TEXT("Box front is reachable"), Reaches(It->GetActorTransform().TransformPosition(FVector(-100,0,0)), true));
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNPCSettingsUITest, "ToonStory.NPC.SettingsUI",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNPCSettingsUITest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World) return false;
	auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
	auto* HUD = PC ? Cast<ATBHUD>(PC->GetHUD()) : nullptr;
	auto* State = World->GetGameState<ATBGameState>();
	if (!HUD || !State) return false;
	const auto Original = State->Settings;
	HUD->NotifyHitBoxClick(TEXT("OpenSettings"));
	HUD->NotifyHitBoxClick(TEXT("MoreNPCHumans"));
	HUD->NotifyHitBoxClick(TEXT("MoreNPCToys"));
	TestEqual(TEXT("NPC human button reaches authority settings"), State->Settings.NPCHumans, Original.NPCHumans + 1);
	TestEqual(TEXT("NPC toy button reaches authority settings"), State->Settings.NPCToys, Original.NPCToys + 1);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]()
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/NPCSettings.png"), false, false);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([HUD, State, Original]()
	{
		HUD->NotifyHitBoxClick(TEXT("CloseSettings"));
		State->Settings = Original;
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBNPCTravelTest, "ToonStory.NPC.LobbyTravel",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBNPCTravelTest::RunTest(const FString& Parameters)
{
	auto CurrentWorld = []() -> UWorld*
	{
		for (const auto& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::Game) return Context.World();
		return nullptr;
	};
	auto* World = CurrentWorld();
	if (!World || !TestTrue(TEXT("Run travel test from TB_Lobby"), TBLobbySettings::IsLobby(World))) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
	if (!Mode || !State || !PC) return false;
	auto Rules = State->Settings;
	Rules.NPCHumans = 1;
	Rules.NPCToys = 1;
	Mode->SetRules(PC, Rules);
	PC->GetPlayerState<ATBPlayerState>()->bReady = true;
	TBLobbySettings::SelectStage(PC, 1);
	Mode->StartRound(PC);
		TestTrue(TEXT("Supported stage starts travel"), State->bTravelPending);
	TestEqual(TEXT("No bots in lobby arrival count"), State->PlayerArray.Num(), 1);
	const double Deadline = FPlatformTime::Seconds() + 60;
	auto Arrived = MakeShared<bool>(false);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, CurrentWorld, Deadline, Arrived]()
	{
		auto* Current = CurrentWorld();
		auto* Match = Current ? Current->GetGameState<ATBGameState>() : nullptr;
		if ((!Match || Match->Phase != ETBPhase::Playing) && FPlatformTime::Seconds() < Deadline) return false;
		*Arrived = Match && Match->Phase == ETBPhase::Playing && TBLobbySettings::StageIndex(Current) == 1;
		TestTrue(TEXT("Match starts after arriving at Demo"), *Arrived);
		if (*Arrived)
		{
			TestEqual(TEXT("NPC settings survive travel"), Match->Settings.NPCToys, 1);
			TestEqual(TEXT("NPCs spawn only on destination"), Match->PlayerArray.Num(), 3);
			auto* Host = Cast<ATBController>(Current->GetFirstPlayerController());
			auto* GM = Current->GetAuthGameMode<ATBGameMode>();
			GM->Finish(ETBWinner::Humans, TEXT("NPC travel test"));
			GM->ReturnToLobby(Host);
		}
		return true;
	}));
	auto ReturnDeadline = MakeShared<double>(0);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, CurrentWorld, Arrived, ReturnDeadline]()
	{
		if (!*Arrived) return true;
		if (*ReturnDeadline == 0) *ReturnDeadline = FPlatformTime::Seconds() + 60;
		auto* Current = CurrentWorld();
		if (!TBLobbySettings::IsLobby(Current) && FPlatformTime::Seconds() < *ReturnDeadline) return false;
		if (TestTrue(TEXT("Returns to dedicated lobby"), TBLobbySettings::IsLobby(Current)))
		{
			auto* Match = Current->GetGameState<ATBGameState>();
			if (!TestNotNull(TEXT("Lobby state restored"), Match)) return true;
			TestEqual(TEXT("Bot states removed before return travel"), Match->PlayerArray.Num(), 1);
			TestEqual(TEXT("NPC count retained after return travel"), Match->Settings.NPCToys, 1);
		}
		return true;
	}));
	return true;
}
#endif
