#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Items/TBItemBase.h"
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
	const ETBPhase OldPhase = State->Phase;
	const ETBTeam OldTeam = Player->Team;
	const ETBToyState OldToyState = Player->ToyState;
	const bool bOldFrozen = Player->bFrozen;
	State->Phase = ETBPhase::Playing;
	Player->Team = ETBTeam::Human;
	Player->ToyState = ETBToyState::Free;
	Player->bFrozen = false;

	ATBItemBase* Item = World->SpawnActor<ATBItemBase>();
	if (TestNotNull(TEXT("Placeable base"), Item))
	{
		FTBInteractionContext Context;
		Context.InstigatorCharacter = Character;
		const FVector Eye = Character->GetPawnViewLocation();
		Item->SetActorLocation(Eye + FVector(100, 0, 0));
		Item->Actions[0].bRequiresLineOfSight = false;
		TestTrue(TEXT("Human can use nearby item"), Item->CanInteract(Context));
		TestFalse(TEXT("Base has no successful effect by default"), Item->TryInteract(Context));
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
		Player->bFrozen = false;
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
		Context.InstigatorCharacter = Character;

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
	}
	State->Phase = OldPhase;
	Player->Team = OldTeam;
	Player->ToyState = OldToyState;
	Player->bFrozen = bOldFrozen;
	return true;
}
#endif
