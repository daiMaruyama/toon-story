#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/TBTestMatch.h"
#include "GamePlay/Items/TBItemBase.h"
#include "Tests/TBItemTestActor.h"
#include "GamePlay/Items/TBInteractionTarget.h"
#include "GamePlay/TBDummyToy.h"
#include "Components/CapsuleComponent.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBItemConditionsTest, "ToonStory.Items.Conditions",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBItemConditionsTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Entry : GEngine->GetWorldContexts())
	{
		if (Entry.WorldType == EWorldType::Game && Entry.World() &&
			Entry.World()->GetGameState<ATBGameState>()) { World = Entry.World(); break; }
	}
	if (!TestNotNull(TEXT("Game world"), World)) return false;
	if (!TestEqual(TEXT("Standalone only"), World->GetNetMode(), NM_Standalone)) return false;
	APlayerController* Controller = World->GetFirstPlayerController();
	ATBCharacter* Character = Controller ? Cast<ATBCharacter>(Controller->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Character"), Character)) return false;
	ATBPlayerState* Player = Character->TBPS();
	if (!TestNotNull(TEXT("Player state"), Player)) return false;
	ATBGameState* State = World->GetGameState<ATBGameState>();
	const ETBTeam OldTeam = Player->Team;
	const ETBToyState OldToyState = Player->ToyState;
	const bool bOldFrozen = Player->bFrozen;
	TBTest::FPlayingScope Playing(*State);
	Player->Team = ETBTeam::Human;
	Player->ToyState = ETBToyState::Free;
	Player->bFrozen = false;

	ATBItemTestActor* Item = World->SpawnActor<ATBItemTestActor>();
	if (TestNotNull(TEXT("Placeable base"), Item))
	{
		FTBInteractionContext Context;
		Context.InstigatorCharacter = Character;
		const FVector Eye = Character->GetPawnViewLocation();
		Item->SetActorLocation(Eye + FVector(100, 0, 0));
		Item->Actions[0].bRequiresLineOfSight = false;
		TestTrue(TEXT("Human can use nearby item"), Item->CanInteract(Context));
		TestTrue(TEXT("Valid request executes success fixture"), Item->TryInteract(Context));
		TestEqual(TEXT("Effect executed once"), Item->Executions, 1);
		Context.ActionId = TEXT("Missing");
		TestFalse(TEXT("Unknown action rejected"), Item->CanInteract(Context));
		Context.ActionId = TEXT("Use");
		Item->Actions.AddDefaulted();
		TestFalse(TEXT("Ambiguous action IDs rejected"), Item->CanInteract(Context));
		Item->Actions.Pop();
		Item->Actions[0].AllowedTeams = ETBInteractionTeams::ToyOnly;
		TestFalse(TEXT("Human denied toy action"), Item->CanInteract(Context));
		Player->Team = ETBTeam::Toy;
		TestTrue(TEXT("Toy permitted"), Item->CanInteract(Context));
		Item->Actions[0].AllowedTeams = ETBInteractionTeams::HumanOnly;
		TestFalse(TEXT("Toy denied human action"), Item->CanInteract(Context));
		Item->Actions[0].AllowedTeams = ETBInteractionTeams::Both;
		TestTrue(TEXT("Both permits toy"), Item->CanInteract(Context));
		Player->Team = ETBTeam::None;
		TestFalse(TEXT("Unassigned team rejected"), Item->CanInteract(Context));
		Player->Team = ETBTeam::Toy;
		Player->ToyState = ETBToyState::Carried;
		TestFalse(TEXT("Carried toy cannot act"), Item->CanInteract(Context));
		Player->ToyState = ETBToyState::Free;
		Player->bFrozen = true;
		TestFalse(TEXT("Frozen player rejected"), Item->TryInteract(Context));
		TestEqual(TEXT("Frozen request never reaches effect"), Item->Executions, 1);
		Player->bFrozen = false;
		TestTrue(TEXT("Same request succeeds after thaw"), Item->TryInteract(Context));
		State->Phase = ETBPhase::Lobby;
		TestFalse(TEXT("Lobby interaction rejected"), Item->CanInteract(Context));
		State->Phase = ETBPhase::Playing;
		Item->SetInteractionEnabled(false);
		TestFalse(TEXT("Disabled state applied"), Item->IsInteractionEnabled());
		TestFalse(TEXT("Disabled item rejected"), Item->CanInteract(Context));
		Item->SetInteractionEnabled(true);
		Item->SetActorLocation(Eye + FVector(201, 0, 0));
		TestFalse(TEXT("Out of reach rejected"), Item->CanInteract(Context));
		Item->SetActorLocation(Eye + FVector(100, 0, 0));
		Item->Actions[0].MaxDistance = 0;
		TestFalse(TEXT("Invalid distance rejected"), Item->CanInteract(Context));
		Item->Actions[0].MaxDistance = 200;
		Context.InstigatorCharacter = nullptr;
		TestFalse(TEXT("Null user rejected"), Item->TryInteract(Context));
		TestEqual(TEXT("Null request never reaches effect"), Item->Executions, 2);
		Context.InstigatorCharacter = Character;
		TestTrue(TEXT("Same request succeeds with valid user"), Item->TryInteract(Context));

		AActor* Wall = World->SpawnActor<AActor>();
		if (TestNotNull(TEXT("Test obstacle"), Wall))
		{
			UBoxComponent* Blocker = NewObject<UBoxComponent>(Wall);
			Wall->SetRootComponent(Blocker);
			Blocker->SetBoxExtent(FVector(5, 100, 100));
			Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Blocker->SetCollisionResponseToAllChannels(ECR_Block);
			Blocker->RegisterComponent();
			Wall->SetActorLocation(Eye + FVector(50, 0, 0));
			Item->Actions[0].bRequiresLineOfSight = true;
			TestFalse(TEXT("Wall blocks interaction"), Item->CanInteract(Context));
			Item->Actions[0].bRequiresLineOfSight = false;
			TestTrue(TEXT("Explicit no-LOS action allowed"), Item->CanInteract(Context));
			Wall->Destroy();
		}
		Item->Destroy();
		const FTransform Placement(FVector(0, 0, 10000));
		auto* InitiallyDisabled = World->SpawnActorDeferred<ATBItemTestActor>(ATBItemTestActor::StaticClass(), Placement);
		if (TestNotNull(TEXT("Deferred item"), InitiallyDisabled))
		{
			InitiallyDisabled->SetInteractionEnabled(false);
			InitiallyDisabled->VisualUpdates = 0;
			InitiallyDisabled->FinishSpawning(Placement);
			TestTrue(TEXT("BeginPlay refreshes an initially disabled item"), InitiallyDisabled->VisualUpdates > 0);
			TestFalse(TEXT("Initial visual receives disabled state"), InitiallyDisabled->bLastVisualEnabled);
			InitiallyDisabled->SetInteractionEnabled(true);
			TestTrue(TEXT("Later state change refreshes visual"), InitiallyDisabled->bLastVisualEnabled);
			InitiallyDisabled->Destroy();
		}
	}
	Player->Team = OldTeam;
	Player->ToyState = OldToyState;
	Player->bFrozen = bOldFrozen;
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBItemAimTest, "ToonStory.Items.AimSelection",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBItemAimTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Entry : GEngine->GetWorldContexts())
	{
		if (Entry.WorldType == EWorldType::Game && Entry.World() && Entry.World()->GetGameState<ATBGameState>())
		{ World = Entry.World(); break; }
	}
	if (!TestNotNull(TEXT("Game world"), World) || World->GetNetMode() != NM_Standalone) return false;
	auto* PC = World->GetFirstPlayerController();
	auto* Human = PC ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Human"), Human) || !TestNotNull(TEXT("Human state"), Human->TBPS())) return false;
	auto* State = World->GetGameState<ATBGameState>();
	auto* Player = Human->TBPS();
	const ETBTeam OldTeam = Player->Team;
	const bool bOldFrozen = Player->bFrozen;
	const FTransform OldTransform = Human->GetActorTransform();
	const FRotator OldAim = PC->GetControlRotation();
	TBTest::FPlayingScope Playing(*State);
	Player->Team = ETBTeam::Human;
	Player->bFrozen = false;
	Human->SetActorLocation(FVector(0, 0, 10000), false, nullptr, ETeleportType::TeleportPhysics);
	const FVector Eye = Human->GetPawnViewLocation();
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Item = World->SpawnActor<ATBItemTestActor>(Eye + FVector(100, 60, 0), FRotator::ZeroRotator, Spawn);
	auto* Toy = World->SpawnActor<ATBCharacter>(Eye + FVector(120, 0, 0), FRotator::ZeroRotator, Spawn);
	auto* ToyPlayer = World->SpawnActor<ATBPlayerState>();
	auto* Dummy = World->SpawnActor<ATBDummyToy>(Eye + FVector(110, -60, 0), FRotator::ZeroRotator, Spawn);
	if (TestNotNull(TEXT("Item"), Item) && TestNotNull(TEXT("Toy"), Toy) &&
	    TestNotNull(TEXT("Toy player"), ToyPlayer) && TestNotNull(TEXT("Dummy"), Dummy))
	{
		ToyPlayer->Team = ETBTeam::Toy;
		ToyPlayer->ToyState = ETBToyState::Free;
		Toy->SetPlayerState(ToyPlayer);
		Dummy->SetActorLocation(Dummy->GetActorLocation() + Eye + FVector(110, -60, 0) - Dummy->Collision->GetComponentLocation());
		PC->SetControlRotation(FRotator::ZeroRotator);
		TestTrue(TEXT("Aimed toy wins over nearer off-axis item"), TB::FindInteractionTarget(Human).Actor == Toy);
		PC->SetControlRotation((Item->GetInteractionLocation() - Eye).Rotation());
		TestTrue(TEXT("Turning toward item selects item"), TB::FindInteractionTarget(Human).Actor == Item);
		PC->SetControlRotation((Dummy->Collision->GetComponentLocation() - Eye).Rotation());
		TestTrue(TEXT("Dummy competes using the same aim rule"), TB::FindInteractionTarget(Human).Actor == Dummy);
		PC->SetControlRotation((Item->GetInteractionLocation() - Eye).Rotation());
		Item->SetInteractionEnabled(false);
		TestTrue(TEXT("Unusable item does not consume toy pickup"), TB::FindInteractionTarget(Human).Actor == Toy);
		Item->SetInteractionEnabled(true);
		PC->SetControlRotation(FRotator(0, 180, 0));
		TestNull(TEXT("Targets behind player are excluded"), TB::FindInteractionTarget(Human).Actor);
		Toy->Destroy();
		Dummy->Destroy();
		Item->SetActorLocation(Eye + FVector(600, 0, 0));
		PC->SetControlRotation(FRotator::ZeroRotator);
		Item->Actions[0].MaxDistance = 650.f;
		TestTrue(TEXT("Configured range beyond 350 is reachable"), TB::FindInteractionTarget(Human).Actor == Item);
		Item->Actions[0].MaxDistance = 550.f;
		TestNull(TEXT("Action range remains authoritative"), TB::FindInteractionTarget(Human).Actor);
		Item->Actions[0].MaxDistance = 650.f;
		auto* Wall = World->SpawnActor<AActor>();
		if (TestNotNull(TEXT("Wall"), Wall))
		{
			auto* Blocker = NewObject<UBoxComponent>(Wall);
			Wall->SetRootComponent(Blocker);
			Blocker->SetBoxExtent(FVector(5, 100, 100));
			Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Blocker->SetCollisionResponseToAllChannels(ECR_Block);
			Blocker->RegisterComponent();
			Wall->SetActorLocation(Eye + FVector(300, 0, 0));
			TestNull(TEXT("Wall blocks selected item"), TB::FindInteractionTarget(Human).Actor);
			Wall->Destroy();
		}
		Human->RequestInteract();
		TestEqual(TEXT("E executes action beyond former trace cap"), Item->Executions, 1);
	}
	if (IsValid(Item)) Item->Destroy();
	if (IsValid(Toy)) Toy->Destroy();
	if (IsValid(ToyPlayer)) ToyPlayer->Destroy();
	if (IsValid(Dummy)) Dummy->Destroy();
	Human->ReleaseInteract();
	Human->SetActorTransform(OldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(OldAim);
	Player->Team = OldTeam;
	Player->bFrozen = bOldFrozen;
	return !HasAnyErrors();
}
#endif
