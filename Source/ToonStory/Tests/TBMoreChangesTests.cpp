#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Character/TBCharacter.h"
#include "Character/TBToyCharacter.h"
#include "Character/TBToyPresentation.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBDummyToy.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "Core/TBController.h"
#include "Core/TBSession.h"
#include "Engine/GameInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRescueEligibilityTest, "ToonStory.MoreChanges.RescueEligibility",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBRescueEligibilityTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game && Context.World()->GetAuthGameMode<ATBGameMode>()) World = Context.World();
	if (!TestNotNull(TEXT("Standalone world"), World) || World->GetNetMode() != NM_Standalone) return false;
	auto* State = World->GetGameState<ATBGameState>();
	auto* Box = State->Box.Get();
	auto* Player = UGameplayStatics::CreatePlayer(World, -1, true);
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Box"), Box)) return false;
	auto* Dummy = World->SpawnActor<ATBDummyToy>();
	const auto Phase = State->Phase;
	ON_SCOPE_EXIT { State->Phase = Phase; if (Dummy) Dummy->Destroy(); UGameplayStatics::RemovePlayer(Player, true); };
	auto* Toy = Cast<ATBCharacter>(Player->GetPawn());
	auto* Info = Player->GetPlayerState<ATBPlayerState>();
	if (!Toy || !Info || !Dummy) return false;
	Info->Team = ETBTeam::Toy;
	Info->ToyState = ETBToyState::Free;
	Toy->ApplyState();
	State->Phase = ETBPhase::Playing;
	TestEqual(TEXT("Default solo rescue is five seconds"), Box->BaseRescueSeconds, 5.f);
	TestFalse(TEXT("Empty box cannot start rescue"), Box->HasPrisoners());
	Dummy->StoredBox = Box;
	TestFalse(TEXT("A decoy alone cannot start rescue"), Box->HasPrisoners());
	Toy->bRescuing = Toy->bHoldingInteract = true;
	Box->RescueProgress = .6f;
	Box->Tick(.1f);
	TestFalse(TEXT("Empty rescue clears held rescue state"), Toy->bRescuing);
	TestEqual(TEXT("Empty rescue clears progress"), Box->RescueProgress, 0.f);
	TestEqual(TEXT("Empty rescue clears rescuers"), Box->Rescuers, 0);
	Info->ToyState = ETBToyState::Boxed;
	TestTrue(TEXT("Connected boxed player is rescuable"), Box->HasPrisoners());
	auto* Rescuer = Cast<ATBCharacter>(World->GetFirstPlayerController()->GetPawn());
	if (!TestNotNull(TEXT("Rescuer pawn"), Rescuer)) return false;
	auto* RescuerInfo = Rescuer->TBPS();
	const auto OriginalTeam = RescuerInfo->Team;
	const FTransform OriginalTransform = Rescuer->GetActorTransform();
	ON_SCOPE_EXIT { RescuerInfo->Team = OriginalTeam; Rescuer->ClearHeldInteraction(); Rescuer->ApplyState(); Rescuer->SetActorTransform(OriginalTransform); };
	RescuerInfo->Team = ETBTeam::Toy;
	Rescuer->ApplyState();
	Rescuer->SetActorLocation(Box->GetActorTransform().TransformPosition(FVector(-100, 0, 25)));
	TestTrue(TEXT("Rescuer reaches box"), Box->InRange(Rescuer));
	Rescuer->bHoldingInteract = Rescuer->bRescuing = true;
	Box->Tick(4.9f);
	TestEqual(TEXT("Prisoner stays boxed before five seconds"), Info->ToyState, ETBToyState::Boxed);
	TestTrue(TEXT("Rescue progress advances"), Box->RescueProgress > .97f);
	Box->Tick(.11f);
	TestEqual(TEXT("Prisoner released at five seconds"), Info->ToyState, ETBToyState::Free);
	Info->ToyState = ETBToyState::Boxed;
	Player->UnPossess();
	TestFalse(TEXT("Disconnected boxed pawn is not rescuable"), Box->HasPrisoners());
	Player->Possess(Toy);
	Info->ToyState = ETBToyState::Free;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBToyWeightsTest, "ToonStory.MoreChanges.ToyWeights",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBToyWeightsTest::RunTest(const FString& Parameters)
{
	const auto* Class = LoadClass<ATBToyCharacter>(nullptr, TEXT("/Game/Characters/Toys/BP_ToyCharacter.BP_ToyCharacter_C"));
	if (!TestNotNull(TEXT("Catalogue class"), Class)) return false;
	const auto& Catalogue = Class->GetDefaultObject<ATBToyCharacter>()->ToyCatalogue;
	bool FoundRanger = false;
	for (auto Visual : Catalogue)
	{
		if (!Visual) continue;
		const bool Ranger = Visual->GetName().Contains(TEXT("SpaceRanger"));
		FoundRanger |= Ranger;
		TestEqual(TEXT("Ranger has one quarter the normal weight"), TBToyPresentation::SelectionWeight(Visual.Get()), Ranger ? 1 : 4);
	}
	TestTrue(TEXT("Ranger exists in actual catalogue"), FoundRanger);
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (World)
	{
		auto* Dummy = World->SpawnActor<ATBDummyToy>();
		TestTrue(TEXT("Decoy picks a real catalogue entry"), Dummy && Catalogue.Contains(Dummy->VisualClass));
		if (Dummy)
		{
			TestEqual(TEXT("Decoys cannot suspend toy capsules"), Dummy->Collision->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Ignore);
			Dummy->Destroy();
		}
		for (auto VisualClass : Catalogue)
		{
			if (!VisualClass || !VisualClass->GetName().Contains(TEXT("SpaceRanger"))) continue;
			auto* Visual = World->SpawnActor<AActor>(VisualClass);
			TBToyPresentation::Initialize(Visual);
			int32 Tinted = 0;
			TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual);
			for (auto* Part : Parts)
				for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
					Tinted += Cast<UMaterialInstanceDynamic>(Part->GetMaterial(Slot)) ? 1 : 0;
			TestTrue(TEXT("Ranger armor actually receives runtime tint"), Tinted > 0);
			Visual->Destroy();
		}
	}
	TArray<TSubclassOf<AActor>> Empty;
	TestEqual(TEXT("Empty catalogue is safe"), TBToyPresentation::SelectToy(Empty), INDEX_NONE);
	TArray<TSubclassOf<AActor>> Sparse = {nullptr, ATBDummyToy::StaticClass(), nullptr};
	for (int32 I = 0; I < 64; ++I) TestEqual(TEXT("Null entries are never selected"), TBToyPresentation::SelectToy(Sparse), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLobbyVisibleTest, "ToonStory.Lobby.VisibleSpawn",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBLobbyVisibleTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	if (!World || !World->GetAuthGameMode<ATBGameMode>()) return false;
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	TestTrue(TEXT("Host has a visible, clear lobby position"), Mode->PlaceInLobby(World->GetFirstPlayerController()));
	TArray<APlayerController*> Added;
	ON_SCOPE_EXIT { for (auto* PC : Added) UGameplayStatics::RemovePlayer(PC, true); };
	for (int32 I = 0; I < 3; ++I)
	{
		auto* PC = UGameplayStatics::CreatePlayer(World, -1, true);
		if (!TestNotNull(TEXT("Joining player"), PC)) break;
		Added.Add(PC);
		TestTrue(TEXT("Joining player fits in camera-visible lobby ground"), Mode->PlaceInLobby(PC));
	}
	return true;
}

// Run these two tests in separate listen-server/client processes on the same port.
class FTBRoomLifecycle : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	TWeakObjectPtr<UGameInstance> GI;
	bool Host;
	int32 Step = 0;
	double Deadline = FPlatformTime::Seconds() + 90;
public:
	FTBRoomLifecycle(FAutomationTestBase* InTest, UGameInstance* Instance, bool IsHost) : Test(InTest), GI(Instance), Host(IsHost) {}
	bool Update() override
	{
		if (!GI.IsValid() || FPlatformTime::Seconds() > Deadline)
		{
			Test->AddError(FString::Printf(TEXT("Room lifecycle timed out at step %d (host %d)"), Step, Host));
			return true;
		}
		auto* World = GI->GetWorld();
		if (!World || !World->HasBegunPlay()) return false;
		auto* State = World->GetGameState<ATBGameState>();
		auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
		auto* Session = GI->GetSubsystem<UTBSession>();
		if (Step == 0 && State && State->PlayerArray.Num() >= 2 && PC)
		{
			if (Host) PC->ServerSelectStage(0);
			Step = 1;
		}
		else if (Step == 1 && State && World->GetMapName().EndsWith(TEXT("TB_KidsRoom")) && State->PlayerArray.Num() >= 2)
		{
			Test->TestEqual(TEXT("Travel retains multiplayer"), World->GetNetMode(), Host ? NM_ListenServer : NM_Client);
			Step = 2;
			Deadline = FPlatformTime::Seconds() + 60;
		}
		else if (Step == 2 && Host && FPlatformTime::Seconds() > Deadline - 50)
		{
			Session->Leave();
			Step = 3;
		}
		else if (Step >= 2 && World->GetMapName().EndsWith(TEXT("Title")) && !Session->IsBusy())
		{
			Test->TestEqual(TEXT("Host leave returns to offline title"), World->GetNetMode(), NM_Standalone);
			if (!Host) Test->TestTrue(TEXT("Client sees room closed message"), Session->Status.Contains(TEXT("部屋が解散")));
			return true;
		}
		return false;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBHostLifecycleTest, "ToonStory.Network.HostRoomLifecycle",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBHostLifecycleTest::RunTest(const FString& Parameters)
{
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game)
		{
			if (Context.World()->GetMapName().EndsWith(TEXT("Title")))
				Context.World()->GetGameInstance()->GetSubsystem<UTBSession>()->Host();
			ADD_LATENT_AUTOMATION_COMMAND(FTBRoomLifecycle(this, Context.World()->GetGameInstance(), true));
			return true;
		}
	return false;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBClientLifecycleTest, "ToonStory.Network.ClientRoomLifecycle",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBClientLifecycleTest::RunTest(const FString& Parameters)
{
	// UE logs these expected transport errors when a listen host intentionally closes.
	AddExpectedError(TEXT("UEngine::BroadcastNetworkFailure: FailureType = (FailureReceived|ConnectionLost)"), EAutomationExpectedErrorFlags::Contains, 0);
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FTBRoomLifecycle(this, Context.World()->GetGameInstance(), false));
			return true;
		}
	return false;
}
#endif

