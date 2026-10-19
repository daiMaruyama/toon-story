#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Character/TBCharacter.h"
#include "Character/TBToyCharacter.h"
#include "Character/TBToyPresentation.h"
#include "Data/TBRuleData.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBDummyToy.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Core/TBController.h"
#include "Core/TBSession.h"
#include "Core/TBLobbySettings.h"
#include "Engine/GameInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/Material.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRescueEligibilityTest, "ToonStory.MoreChanges.RescueEligibility",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBRescueEligibilityTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game && Context.World()->GetAuthGameMode<ATBGameMode>())
		{
			World = Context.World();
		}
	}
	if (!TestNotNull(TEXT("Standalone world"), World) || World->GetNetMode() != NM_Standalone)
	{
		return false;
	}
	auto* State = World->GetGameState<ATBGameState>();
	auto* Box = State->Box.Get();
	auto* Player = UGameplayStatics::CreatePlayer(World, -1, true);
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Box"), Box))
	{
		return false;
	}
	auto* Dummy = World->SpawnActor<ATBDummyToy>();
	const auto Phase = State->Phase;
	ON_SCOPE_EXIT
	{
		State->Phase = Phase;
		if (Dummy)
		{
			const auto OriginalVisual = Dummy->VisualClass;
			for (int32 I = 0; I < 32; ++I) Dummy->ResetPlacement();
			TestTrue(TEXT("Cleanup preserves decoy identity"), Dummy->VisualClass == OriginalVisual);
			Dummy->Destroy();
		}
		UGameplayStatics::RemovePlayer(Player, true);
	};
	auto* Toy = Cast<ATBCharacter>(Player->GetPawn());
	auto* Info = Player->GetPlayerState<ATBPlayerState>();
	if (!Toy || !Info || !Dummy)
	{
		return false;
	}
	Info->Team = ETBTeam::Toy;
	Info->ToyState = ETBToyState::Free;
	Toy->ApplyState();
	State->Phase = ETBPhase::Playing;
	TestEqual(TEXT("Default solo rescue is five seconds"), TB::Rules().Box.RescueSeconds, 5.f);
	TestFalse(TEXT("Empty box cannot start rescue"), Box->HasPrisoners());
	TestEqual(TEXT("Free decoys do not block toy movement"),
	          Dummy->Collision->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Ignore);
	Dummy->Place(FVector(0, 0, 5000), Box);
	TestEqual(TEXT("Stored decoys block storage occupancy sweeps"),
	          Dummy->Collision->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Block);
	FHitResult DropHit;
	const FVector DummyCenter = Dummy->Collision->GetComponentLocation();
	TestTrue(TEXT("Drop sweep detects a stored decoy"),
	         World->SweepSingleByChannel(DropHit, DummyCenter + FVector(0, 0, 80), DummyCenter,
	                                     FQuat::Identity, ECC_GameTraceChannel1,
	                                     FCollisionShape::MakeSphere(2)) && DropHit.GetActor() == Dummy);
	Dummy->Place(FVector(0, 0, 5000));
	TestEqual(TEXT("Released decoys no longer block toy movement"),
	          Dummy->Collision->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Ignore);
	Dummy->Place(FVector(0, 0, 5000), Box);
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
	if (!TestNotNull(TEXT("Rescuer pawn"), Rescuer))
	{
		return false;
	}
	auto* RescuerInfo = Rescuer->TBPS();
	const auto OriginalTeam = RescuerInfo->Team;
	const FTransform OriginalTransform = Rescuer->GetActorTransform();
	ON_SCOPE_EXIT
	{
		RescuerInfo->Team = OriginalTeam;
		Rescuer->ClearHeldInteraction();
		Rescuer->ApplyState();
		Rescuer->SetActorTransform(OriginalTransform);
	};
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
	const auto* Class =
	    LoadClass<ATBToyCharacter>(nullptr, TEXT("/Game/Characters/Toys/BP_ToyCharacter.BP_ToyCharacter_C"));
	if (!TestNotNull(TEXT("Catalogue class"), Class))
	{
		return false;
	}
	const auto& Catalogue = Class->GetDefaultObject<ATBToyCharacter>()->ToyCatalogue;
	bool FoundRanger = false;
	for (auto Visual : Catalogue)
	{
		if (!Visual)
		{
			continue;
		}
		const bool Ranger = Visual->GetName().Contains(TEXT("SpaceRanger"));
		FoundRanger |= Ranger;
		TestEqual(TEXT("Ranger has one quarter the normal weight"), TBToyPresentation::SelectionWeight(Visual.Get()),
		          Ranger ? 1 : 4);
	}
	TestTrue(TEXT("Ranger exists in actual catalogue"), FoundRanger);
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game)
		{
			World = Context.World();
		}
	}
	if (World)
	{
		auto* Dummy = World->SpawnActor<ATBDummyToy>();
		TestTrue(TEXT("Decoy picks a real catalogue entry"), Dummy && Catalogue.Contains(Dummy->VisualClass));
		if (Dummy)
		{
			TestEqual(TEXT("Decoys cannot suspend toy capsules"),
			          Dummy->Collision->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Ignore);
			const auto OriginalVisual = Dummy->VisualClass;
			for (int32 I = 0; I < 32; ++I) Dummy->ResetPlacement();
			TestTrue(TEXT("Cleanup preserves decoy identity"), Dummy->VisualClass == OriginalVisual);
			Dummy->Destroy();
		}
		for (auto VisualClass : Catalogue)
		{
			if (!VisualClass || !VisualClass->GetName().Contains(TEXT("SpaceRanger")))
			{
				continue;
			}
			auto* Visual = World->SpawnActor<AActor>(VisualClass);
			TBToyPresentation::Initialize(Visual);
			int32 Tinted = 0;
			TInlineComponentArray<USkeletalMeshComponent*> Parts(Visual);
			for (auto* Part : Parts)
			{
				for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
				{
					auto* Material = Part->GetMaterial(Slot);
					if (!Material || Material->GetName() != TEXT("M_plastic_base")) continue;
					++Tinted;
					TestTrue(TEXT("Authored armor supports skeletal meshes before runtime"), Material->GetMaterial()->GetUsageByFlag(MATUSAGE_SkeletalMesh));
					FLinearColor Color;
					TestTrue(TEXT("Armor tint is saved in the referenced material"),
					         Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ArmorTint")), Color));
					TestTrue(TEXT("Authored armor tint matches intended color"), Color.Equals(FLinearColor(.30f, .42f, .37f)));
				}
			}
			TestTrue(TEXT("Ranger uses the cookable authored armor material"), Tinted > 0);
			Visual->Destroy();
		}
	}
	TArray<TSubclassOf<AActor>> Empty;
	TestEqual(TEXT("Empty catalogue is safe"), TBToyPresentation::SelectToy(Empty), INDEX_NONE);
	TArray<TSubclassOf<AActor>> Sparse = {nullptr, ATBDummyToy::StaticClass(), nullptr};
	for (int32 I = 0; I < 64; ++I)
	{
		TestEqual(TEXT("Null entries are never selected"), TBToyPresentation::SelectToy(Sparse), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLobbyVisibleTest, "ToonStory.Lobby.VisibleSpawn",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBLobbyVisibleTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game)
		{
			World = Context.World();
		}
	}
	if (!World || !World->GetAuthGameMode<ATBGameMode>())
	{
		return false;
	}
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	if (TBLobbySettings::IsLobby(World))
	{
		auto* PC = CastChecked<ATBController>(World->GetFirstPlayerController());
		APawn* OriginalPawn = PC->GetPawn();
		const int32 Counts[] = {7, 10, 8};
		const TCHAR* Names[] = {TEXT("Kids' Rooms"), TEXT("Demo ＋ こども部屋"), TEXT("ArchViz Apartment")};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			PC->ServerSelectStage(Index);
			TestEqual(TEXT("Selected stage display name"), World->GetGameState<ATBGameState>()->StageName,
			          FString(Names[Index]));
			TestTrue(TEXT("All stage selections retain the lobby pawn"), PC->GetPawn() == OriginalPawn);
			TestEqual(TEXT("Selected stage item limit"), TBLobbySettings::AvailableItems(World), Counts[Index]);
		}
	}
	TestTrue(TEXT("Host has a visible, clear lobby position"), Mode->PlaceInLobby(World->GetFirstPlayerController()));
	TArray<APlayerController*> Added;
	ON_SCOPE_EXIT
	{
		for (auto* PC : Added)
		{
			UGameplayStatics::RemovePlayer(PC, true);
		}
	};
	for (int32 I = 0; I < 3; ++I)
	{
		auto* PC = UGameplayStatics::CreatePlayer(World, -1, true);
		if (!TestNotNull(TEXT("Joining player"), PC))
		{
			break;
		}
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
	FTBRoomLifecycle(FAutomationTestBase* InTest, UGameInstance* Instance, bool IsHost)
	    : Test(InTest), GI(Instance), Host(IsHost)
	{
	}
	bool Update() override
	{
		if (!GI.IsValid() || FPlatformTime::Seconds() > Deadline)
		{
			Test->AddError(FString::Printf(TEXT("Room lifecycle timed out at step %d (host %d)"), Step, Host));
			return true;
		}
		auto* World = GI->GetWorld();
		if (!World || !World->HasBegunPlay())
		{
			return false;
		}
		auto* State = World->GetGameState<ATBGameState>();
		auto* PC = Cast<ATBController>(GI->GetFirstLocalPlayerController(World));
		auto* Session = GI->GetSubsystem<UTBSession>();
		if (Step == 0 && State && State->PlayerArray.Num() >= 2 && PC)
		{
			Test->TestTrue(TEXT("Room opens dedicated lobby"), World->GetMapName().EndsWith(TEXT("TB_Lobby")));
			if (Host)
			{
				PC->ServerSelectStage(0);
			}
			Step = 1;
		}
		else if (Step == 1 && State && State->SelectedStage == 0 && PC)
		{
			Test->TestTrue(TEXT("Selecting a stage does not move lobby"),
			               World->GetMapName().EndsWith(TEXT("TB_Lobby")));
			PC->ServerReady();
			Step = 2;
		}
		else if (Step == 2 && State)
		{
			if (Host && !State->bTravelPending && World->GetMapName().EndsWith(TEXT("TB_Lobby")))
			{
				bool Ready = true;
				for (APlayerState* Player : State->PlayerArray)
				{
					Ready &= CastChecked<ATBPlayerState>(Player)->bReady;
				}
				if (Ready)
				{
					PC->ServerStart();
				}
			}
			if (State->Phase == ETBPhase::Playing)
			{
				Test->TestTrue(TEXT("Start travels to selected stage"),
				               World->GetMapName().EndsWith(TEXT("TB_KidsRoom")));
				Test->TestEqual(TEXT("Travel retains multiplayer"), World->GetNetMode(),
				                Host ? NM_ListenServer : NM_Client);
				Step = 3;
				Deadline = FPlatformTime::Seconds() + 60;
			}
		}
		else if (Step == 3 && Host && FPlatformTime::Seconds() > Deadline - 50 && State)
		{
			auto* Mode = World->GetAuthGameMode<ATBGameMode>();
			Mode->Finish(ETBWinner::Humans, TEXT("Lifecycle test"));
			Mode->ReturnToLobby(PC);
			Step = 4;
		}
		else if ((Step == 3 || Step == 4) && State && PC && PC->GetPlayerState<ATBPlayerState>() &&
		         World->GetMapName().EndsWith(TEXT("TB_Lobby")) && State->PlayerArray.Num() >= 2)
		{
			Test->TestEqual(TEXT("Selected stage survives return"), State->SelectedStage, 0);
			Test->TestFalse(TEXT("Return clears ready"), PC->GetPlayerState<ATBPlayerState>()->bReady);
			Step = 5;
			Deadline = FPlatformTime::Seconds() + 60;
		}
		else if (Step == 5 && Host && FPlatformTime::Seconds() > Deadline - 50)
		{
			Session->Leave();
			Step = 6;
		}
		else if (Step >= 5 && World->GetMapName().EndsWith(TEXT("Title")) && !Session->IsBusy())
		{
			Test->TestEqual(TEXT("Host leave returns to offline title"), World->GetNetMode(), NM_Standalone);
			if (!Host)
			{
				Test->TestTrue(TEXT("Client sees room closed message"), Session->Status.Contains(TEXT("部屋が解散")));
			}
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
	{
		if (Context.WorldType == EWorldType::Game)
		{
			if (Context.World()->GetMapName().EndsWith(TEXT("Title")))
			{
				Context.World()->GetGameInstance()->GetSubsystem<UTBSession>()->Host();
			}
			ADD_LATENT_AUTOMATION_COMMAND(FTBRoomLifecycle(this, Context.World()->GetGameInstance(), true));
			return true;
		}
	}
	return false;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBClientLifecycleTest, "ToonStory.Network.ClientRoomLifecycle",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBClientLifecycleTest::RunTest(const FString& Parameters)
{
	// UE logs these expected transport errors when a listen host intentionally closes.
	AddExpectedError(TEXT("UEngine::BroadcastNetworkFailure: FailureType = (FailureReceived|ConnectionLost)"),
	                 EAutomationExpectedErrorFlags::Contains, 0);
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FTBRoomLifecycle(this, Context.World()->GetGameInstance(), false));
			return true;
		}
	}
	return false;
}
class FTBLobbyPhoto : public IAutomationLatentCommand
{
	bool Update() override
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("LobbyPreview.png"), true, false);
		return true;
	}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLobbyPhotoTest, "ToonStory.Lobby.Preview",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBLobbyPhotoTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(15));
	ADD_LATENT_AUTOMATION_COMMAND(FTBLobbyPhoto());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3));
	return true;
}
#endif
