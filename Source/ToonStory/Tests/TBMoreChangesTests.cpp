#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Character/TBCharacter.h"
#include "Character/TBNPCController.h"
#include "Character/TBToyCharacter.h"
#include "Character/TBToyPresentation.h"
#include "Data/TBRuleData.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBDummyToy.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
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
	auto* LobbyState = World->GetGameState<ATBGameState>();
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	if (!LobbyState || !Host || !TestTrue(TEXT("Visible-spawn fixture requires an isolated standalone Lobby"),
	    World->GetNetMode() == NM_Standalone && LobbyState->Phase == ETBPhase::Lobby && LobbyState->PlayerArray.Num() == 1)) return false;
	if (TBLobbySettings::IsLobby(World))
	{
		auto* PC = CastChecked<ATBController>(World->GetFirstPlayerController());
		APawn* OriginalPawn = PC->GetPawn();
		TestEqual(TEXT("Demo ID 1 keeps its authored path"), FString(TBLobbySettings::StagePath(1)),
		          FString(TEXT("/Game/Maps/TB_ToyWorldDemoExpanded")));
		TestEqual(TEXT("Stylized House occupies stable stage ID 3"), FString(TBLobbySettings::StagePath(3)),
		          FString(TEXT("/Game/Maps/TB_StylizedHouse")));
		// Retained IDs are stable; retired/invalid selections safely resolve to Demo ID 1.
		for (const int32 Requested : {1, 3, 0, 2, -1, 4, MIN_int32, MAX_int32})
		{
			const int32 ExpectedId = Requested == 3 ? 3 : 1;
			const TCHAR* ExpectedName = ExpectedId == 3 ? TEXT("Stylized House") : TEXT("Demo ＋ こども部屋");
			const int32 ExpectedItems = ExpectedId == 3 ? 8 : 10;
			PC->ServerSelectStage(Requested);
			TestEqual(TEXT("Selected stage retains its stable ID or falls back to Demo"),
			          World->GetGameState<ATBGameState>()->SelectedStage, ExpectedId);
			TestEqual(TEXT("Selected stage display name"), World->GetGameState<ATBGameState>()->StageName,
			          FString(ExpectedName));
			TestTrue(TEXT("All stage selections retain the lobby pawn"), PC->GetPawn() == OriginalPawn);
			TestEqual(TEXT("Selected stage item limit"), TBLobbySettings::AvailableItems(World), ExpectedItems);
			TestEqual(TEXT("Stage path follows the same stable ID and fallback policy"),
			          FString(TBLobbySettings::StagePath(Requested)), FString(TBLobbySettings::StagePath(ExpectedId)));
			auto* Pending = World->GetGameInstance()->GetSubsystem<UTBLobbySettings>();
			Pending->SelectedStage = Requested;
			TBLobbySettings::Restore(Mode);
			TestEqual(TEXT("Saved stage restores to a retained ID"), Pending->SelectedStage, ExpectedId);
			TestEqual(TEXT("Restored stage replicates the normalized selection"),
			          World->GetGameState<ATBGameState>()->SelectedStage, ExpectedId);
		}
	}
	TestTrue(TEXT("Host has a visible, clear lobby position"), Mode->PlaceInLobby(World->GetFirstPlayerController()));
	const auto OriginalRules = LobbyState->Settings;
	const auto OriginalStarts = Mode->SpawnPoints;
	const auto OriginalPreference = Host->GetPlayerState<ATBPlayerState>()->Preference;
	const bool OriginalReady = Host->GetPlayerState<ATBPlayerState>()->bReady;
	TArray<APlayerController*> Added;
	TArray<AActor*> FixtureActors;
	TArray<AController*> Participants = {Host};
	ON_SCOPE_EXIT
	{
		Mode->SpawnPoints = OriginalStarts;
		if (LobbyState->Phase == ETBPhase::Playing) Mode->Finish(ETBWinner::None, TEXT("Lobby placement cleanup"), true);
		if (!TBLobbySettings::IsLobby(World) && (LobbyState->Phase == ETBPhase::Results || LobbyState->Phase == ETBPhase::Aborted))
			Mode->ReturnToLobby(Host);
		Mode->RemoveNPCs();
		for (auto* Actor : FixtureActors) if (IsValid(Actor)) Actor->Destroy();
		for (auto* PC : Added)
		{
			if (IsValid(PC)) UGameplayStatics::RemovePlayer(PC, true);
		}
		LobbyState->Settings = OriginalRules;
		Host->GetPlayerState<ATBPlayerState>()->Preference = OriginalPreference;
		Host->GetPlayerState<ATBPlayerState>()->bReady = OriginalReady;
		Mode->SyncLobbyCounts();
	};
	auto CheckParticipants = [&]()
	{
		for (int32 Index = 0; Index < Participants.Num(); ++Index)
		{
			auto* Body = Cast<ATBCharacter>(Participants[Index]->GetPawn());
			if (!TestNotNull(TEXT("Placed participant has a possessed character"), Body)) continue;
			TestTrue(TEXT("Placement retains possession"), Body->GetController() == Participants[Index]);
			const auto* Capsule = Body->GetCapsuleComponent();
			const FVector Position = Capsule->GetComponentLocation();
			const float Radius = Capsule->GetScaledCapsuleRadius(), Half = Capsule->GetScaledCapsuleHalfHeight();
			FCollisionQueryParams Query(SCENE_QUERY_STAT(LobbyPlacementFixture), false, Body);
			FHitResult Floor;
			TestTrue(TEXT("Placed capsule retains nearby walkable floor"),
			    World->SweepSingleByChannel(Floor, Position + FVector(0, 0, 1), Position - FVector(0, 0, 8), FQuat::Identity,
			        ECC_Pawn, FCollisionShape::MakeCapsule(Radius + 4, Half), Query) && !Floor.bStartPenetrating && Floor.ImpactNormal.Z >= .7f);
			TestFalse(TEXT("Placed capsule retains static clearance"), World->OverlapBlockingTestByChannel(Position,
			    FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius + 4, Half), Query));
			bool Visible = false;
			for (TActorIterator<ACameraActor> Camera(World); Camera; ++Camera)
			{
				if (!Camera->ActorHasTag(TEXT("TBLobbyCamera"))) continue;
				const auto* View = Camera->GetCameraComponent();
				const FVector Local = View->GetComponentTransform().InverseTransformPosition(Position);
				const float TanHalf = FMath::Tan(FMath::DegreesToRadians(View->FieldOfView * .5f));
				FCollisionQueryParams VisibilityQuery = Query; VisibilityQuery.bTraceComplex = true;
				Visible |= Local.X >= 100 && FMath::Abs(Local.Y) + Radius <= Local.X * TanHalf * .8f &&
				    FMath::Abs(Local.Z) + Half <= Local.X * TanHalf / View->AspectRatio * .8f &&
				    !World->LineTraceTestByChannel(View->GetComponentLocation(), Position, ECC_Visibility, VisibilityQuery);
			}
			TestTrue(TEXT("Placed capsule retains camera FOV and line of sight"), Visible);
			for (int32 OtherIndex = 0; OtherIndex < Index; ++OtherIndex)
			{
				const auto* OtherBody = Cast<ATBCharacter>(Participants[OtherIndex]->GetPawn());
				if (!OtherBody) continue;
				const auto* OtherCapsule = OtherBody->GetCapsuleComponent();
				const float OtherRadius = OtherCapsule->GetScaledCapsuleRadius();
				const FVector Axis(0, 0, FMath::Max(0.f, Half - Radius));
				const FVector OtherAxis(0, 0, FMath::Max(0.f, OtherCapsule->GetScaledCapsuleHalfHeight() - OtherRadius));
				const FVector OtherPosition = OtherCapsule->GetComponentLocation();
				FVector Closest, OtherClosest;
				FMath::SegmentDistToSegmentSafe(Position - Axis, Position + Axis, OtherPosition - OtherAxis,
				    OtherPosition + OtherAxis, Closest, OtherClosest);
				TestTrue(TEXT("Possessed capsules are geometrically separated independently of Pawn responses"),
				    FVector::DistSquared(Closest, OtherClosest) > FMath::Square(Radius + OtherRadius + 1.f));
			}
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
		Participants.Add(PC);
		TestTrue(TEXT("Joining player fits in camera-visible lobby ground"), Mode->PlaceInLobby(PC));
		if (I == 0)
		{
			CheckParticipants(); // Two real local players before adding further occupancy.
			TestTrue(TEXT("Existing player can be repositioned without occupying the joiner's capsule"), Mode->PlaceInLobby(Host));
			CheckParticipants();
		}
	}
	// Preserve the four-local-player coverage; use possessed native NPC bodies for eight-capsule capacity.
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = Participants.Num(); Index < 8; ++Index)
	{
		auto* Body = World->SpawnActor<ATBCharacter>(FVector(Index * 1000, 50000, 20000), FRotator::ZeroRotator, Spawn);
		auto* AI = World->SpawnActor<ATBNPCController>();
		if (Body) FixtureActors.Add(Body);
		if (AI) FixtureActors.Add(AI);
		if (!TestNotNull(TEXT("Capacity fixture character"), Body) || !TestNotNull(TEXT("Capacity fixture controller"), AI)) return false;
		AI->Possess(Body);
		auto* Info = AI->GetPlayerState<ATBPlayerState>();
		if (!TestNotNull(TEXT("Capacity fixture player state"), Info)) return false;
		Info->SetIsABot(true); Info->Team = ETBTeam::Human; Body->ApplyState();
		Participants.Add(AI);
		TestTrue(TEXT("Additional possessed capsule fits without relying on blocking Pawn response"), Mode->PlaceInLobby(AI));
	}
	TestEqual(TEXT("Capacity fixture covers eight possessed capsules, not eight connections"), Participants.Num(), 8);
	CheckParticipants();
	for (auto* Participant : Participants)
		TestTrue(TEXT("Repositioning all eight retains a free camera-visible candidate"), Mode->PlaceInLobby(Participant));
	CheckParticipants();
	// Fill every camera's finite search area with a temporary static obstacle; no actor is moved by the refusal.
	TArray<UBoxComponent*> Blockers;
	for (TActorIterator<ACameraActor> Camera(World); Camera; ++Camera)
	{
		if (!Camera->ActorHasTag(TEXT("TBLobbyCamera"))) continue;
		auto* Wall = World->SpawnActor<AActor>(); FixtureActors.Add(Wall);
		auto* Collision = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Collision);
		Collision->SetBoxExtent(FVector(4000)); Collision->SetCollisionProfileName(TEXT("BlockAll")); Collision->RegisterComponent();
		Wall->SetActorLocation(Camera->GetCameraComponent()->GetComponentLocation()); Blockers.Add(Collision);
	}
	if (!TestTrue(TEXT("Exhaustion fixture blocks an actual lobby camera search area"), !Blockers.IsEmpty())) return false;
	auto* HostBody = CastChecked<ATBCharacter>(Host->GetPawn());
	auto* Movement = HostBody->GetCharacterMovement();
	const FTransform BeforeRefusal = HostBody->GetActorTransform();
	const FVector BeforeVelocity = Movement->Velocity;
	const EMovementMode BeforeMode = Movement->MovementMode;
	const uint8 BeforeCustomMode = Movement->CustomMovementMode;
	TestFalse(TEXT("Exhausted finite candidates refuse placement"), Mode->PlaceInLobby(Host));
	TestTrue(TEXT("Ordinary refusal preserves original transform"), HostBody->GetActorTransform().Equals(BeforeRefusal, .1f));
	TestTrue(TEXT("Ordinary refusal preserves original velocity"), Movement->Velocity.Equals(BeforeVelocity, .01f));
	TestEqual(TEXT("Ordinary refusal preserves movement mode"), Movement->MovementMode, BeforeMode);
	TestEqual(TEXT("Ordinary refusal preserves custom movement mode"), Movement->CustomMovementMode, BeforeCustomMode);
	for (auto* Blocker : Blockers) Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Dedicated-lobby starts travel; verify the synchronous start/rollback/rematch contract on retained Demo only.
	if (!TBLobbySettings::IsLobby(World) && TBLobbySettings::StageIndex(World) == 1 && !TBLobbySettings::ShouldTravelToStage(World))
	{
		Mode->RemoveNPCs();
		while (Added.Num() > 1) UGameplayStatics::RemovePlayer(Added.Pop(), true);
		if (Added.IsEmpty()) return false;
		auto* Second = CastChecked<ATBController>(Added[0]);
		auto Rules = LobbyState->Settings; Rules.Humans = Rules.Toys = 1; Rules.NPCHumans = Rules.NPCToys = 0; Rules.RequiredItems = 1;
		Mode->SetRules(Host, Rules);
		Host->GetPlayerState<ATBPlayerState>()->Preference = ETBTeam::Human;
		Second->GetPlayerState<ATBPlayerState>()->Preference = ETBTeam::Toy;
		Participants = {Host, Second};
		const FTransform BeforeFailedStart = Host->GetPawn()->GetActorTransform();
		const FTransform SecondBeforeFailedStart = Second->GetPawn()->GetActorTransform();
		Mode->SpawnPoints = {nullptr}; // Nonempty: do not fall back to UE's world PlayerStart search.
		Host->GetPlayerState<ATBPlayerState>()->bReady = Second->GetPlayerState<ATBPlayerState>()->bReady = true;
		Mode->StartRound(Host);
		Mode->SpawnPoints = OriginalStarts;
		if (!TestEqual(TEXT("Invalid start candidate retains Lobby"), LobbyState->Phase, ETBPhase::Lobby)) return false;
		TestTrue(TEXT("Failed start retains the separated lobby transform"), Host->GetPawn()->GetActorTransform().Equals(BeforeFailedStart, .1f));
		TestTrue(TEXT("Failed start also retains the second participant's transform"), Second->GetPawn()->GetActorTransform().Equals(SecondBeforeFailedStart, .1f));
		CheckParticipants();
		Host->GetPlayerState<ATBPlayerState>()->bReady = Second->GetPlayerState<ATBPlayerState>()->bReady = true;
		Mode->StartRound(Host);
		if (!TestTrue(TEXT("Separated lobby participants can start normally"), LobbyState->Phase == ETBPhase::Playing && LobbyState->IsPreparing())) return false;
		Mode->AdvancePreparationClockForTest(0);
		if (!TestTrue(TEXT("Normal server release precedes rematch return"), LobbyState->Phase == ETBPhase::Playing && !LobbyState->IsPreparing())) return false;
		Mode->Finish(ETBWinner::None, TEXT("Lobby placement rematch"), true);
		Mode->ReturnToLobby(Host);
		TestEqual(TEXT("Rematch return reaches Lobby"), LobbyState->Phase, ETBPhase::Lobby);
		Participants = {Host, Second};
		CheckParticipants();
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
				PC->ServerSelectStage(3);
			}
			Step = 1;
		}
		else if (Step == 1 && State && State->SelectedStage == 3 && PC)
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
			if (State->Phase == ETBPhase::Playing && !State->IsPreparing())
			{
				Test->TestTrue(TEXT("Start travels to selected stage"),
				               World->GetMapName().EndsWith(TEXT("TB_StylizedHouse")));
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
			Test->TestEqual(TEXT("Selected stage survives return"), State->SelectedStage, 3);
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
