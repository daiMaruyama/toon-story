#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/TBRoomLighting.h"
#include "GamePlay/TBLightSwitch.h"
#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/Items/TBInteractionTarget.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRoomLightingTest, "ToonStory.Lighting.RoomRules",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBRoomLightingTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false));
	auto* Room = World->SpawnActor<ATBRoomLighting>();
	auto* Light = World->SpawnActor<APointLight>();
	Room->Lights.Add(Light);
	Room->Bounds->SetBoxExtent(FVector(100, 100, 100));
	const FVector Toy(0, 0, 0), FarHuman(400, 0, 0), NearHuman(299, 0, 0);
	Room->SetLightsOn(true);
	TestFalse(TEXT("Lit room retains normal freeze"), ATBRoomLighting::ProtectsFromGaze(World, Toy, FarHuman));
	Room->SetLightsOn(false);
	TestFalse(TEXT("Actual light is off"), Light->GetLightComponent()->IsVisible());
	TestTrue(TEXT("Dark toy protected from observer outside room"), ATBRoomLighting::ProtectsFromGaze(World, Toy, FarHuman));
	TestFalse(TEXT("Nearby observer still freezes"), ATBRoomLighting::ProtectsFromGaze(World, Toy, NearHuman));
	TestTrue(TEXT("Exactly three meters protects"), ATBRoomLighting::ProtectsFromGaze(World, Toy, FVector(300, 0, 0)));
	TestFalse(TEXT("Outside any room retains normal freeze"), ATBRoomLighting::ProtectsFromGaze(World, FVector(200, 0, 0), FarHuman));
	auto* OtherRoom = World->SpawnActor<ATBRoomLighting>();
	TestFalse(TEXT("Lit overlap wins independent of iteration order"), ATBRoomLighting::ProtectsFromGaze(World, Toy, FarHuman));
	OtherRoom->Destroy();
	Room->SetActorRotation(FRotator(0, 90, 0));
	Room->SetActorScale3D(FVector(2, 1, 1));
	TestTrue(TEXT("Rotated scaled bounds contain point"), Room->Contains(FVector(0, 150, 0)));
	TestFalse(TEXT("Rotated scaled bounds exclude point"), Room->Contains(FVector(150, 0, 0)));
	Room->ResetLighting();
	TestTrue(TEXT("Rematch restores initial state"), Room->bLightsOn && Light->GetLightComponent()->IsVisible());
	Room->bInitiallyLightsOn = false;
	Room->ResetLighting();
	TestFalse(TEXT("Initially dark rooms reset to dark"), Room->bLightsOn);
	auto* Switch = World->SpawnActor<ATBLightSwitch>();
	TestTrue(TEXT("Switch inherits shared item foundation"), Switch->IsA<ATBItemBase>());
	TestFalse(TEXT("Missing user rejects interaction"), Switch->CanInteract(FTBInteractionContext()));
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLightSwitchItemTest, "ToonStory.Items.LightSwitch",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBLightSwitchItemTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Entry : GEngine->GetWorldContexts())
	{
		if (Entry.WorldType == EWorldType::Game && Entry.World() && Entry.World()->GetGameState<ATBGameState>())
		{ World = Entry.World(); break; }
	}
	if (!TestNotNull(TEXT("Standalone world"), World) || World->GetNetMode() != NM_Standalone) return false;
	auto* Controller = World->GetFirstPlayerController();
	auto* Character = Controller ? Cast<ATBCharacter>(Controller->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Player"), Character->TBPS())) return false;
	auto* Player = Character->TBPS();
	auto* State = World->GetGameState<ATBGameState>();
	const auto OldPhase = State->Phase;
	const auto OldTeam = Player->Team;
	const auto OldToyState = Player->ToyState;
	const bool bOldFrozen = Player->bFrozen;
	auto* Room = World->SpawnActor<ATBRoomLighting>();
	auto* Switch = World->SpawnActor<ATBLightSwitch>();
	ON_SCOPE_EXIT
	{
		State->Phase = OldPhase;
		Player->Team = OldTeam;
		Player->ToyState = OldToyState;
		Player->bFrozen = bOldFrozen;
		if (Switch) Switch->Destroy();
		if (Room) Room->Destroy();
	};
	if (!TestNotNull(TEXT("Room"), Room) || !TestNotNull(TEXT("Switch"), Switch)) return false;
	State->Phase = ETBPhase::Playing;
	Player->Team = ETBTeam::Toy;
	Player->ToyState = ETBToyState::Free;
	Player->bFrozen = false;
	const FVector Eye = Character->GetPawnViewLocation();
	const FVector Aim = Character->GetControlRotation().Vector();
	Switch->SetActorLocation(Eye + Aim * 100.f);
	Switch->Actions[0].bRequiresLineOfSight = false; // Shared Conditions test covers walls.
	FTBInteractionContext Use;
	Use.InstigatorCharacter = Character;
	Use.ActionId = TEXT("Toggle");
	TestFalse(TEXT("Unlinked switch rejects valid user"), Switch->TryInteract(Use));
	Switch->Room = Room;
	TestTrue(TEXT("Common targeting finds switch"), TB::FindInteractionTarget(Character).Actor == Switch);
	TestTrue(TEXT("Toy toggles through item base"), Switch->TryInteract(Use));
	TestFalse(TEXT("Room turns off"), Room->bLightsOn);
	Player->Team = ETBTeam::Human;
	TestTrue(TEXT("Human toggles through item base"), Switch->TryInteract(Use));
	TestTrue(TEXT("Room turns on"), Room->bLightsOn);
	Switch->SetInteractionEnabled(false);
	TestFalse(TEXT("Base disabled flag blocks toggle"), Switch->TryInteract(Use));
	Switch->SetInteractionEnabled(true);
	Player->bFrozen = true;
	TestFalse(TEXT("Frozen user cannot toggle"), Switch->TryInteract(Use));
	Player->bFrozen = false;
	Switch->SetActorLocation(Eye + Aim * 181.f);
	TestFalse(TEXT("Base reach blocks toggle"), Switch->TryInteract(Use));
	Switch->SetActorLocation(Eye + Aim * 100.f);
	Use.ActionId = TEXT("Missing");
	TestFalse(TEXT("Unknown action cannot toggle"), Switch->TryInteract(Use));
	TestTrue(TEXT("Rejected interactions preserve lighting"), Room->bLightsOn);
	return true;
}
#endif
