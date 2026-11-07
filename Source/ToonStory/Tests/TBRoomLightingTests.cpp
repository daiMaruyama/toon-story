#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/TBTestMatch.h"
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
#include "Core/TBGameHelpers.h"
#include "GamePlay/Items/TBInteractionTarget.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include "Data/TBLightSwitchData.h"
#include "Data/TBRuleData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBRoomLightingTest, "ToonStory.Lighting.RoomRules",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBRoomLightingTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	// Isolate rule-boundary assertions from designers' authored values.
	auto& Vision = const_cast<FTBVisionRules&>(TB::Rules().Vision);
	const float OldProtectionDistance = Vision.DarkProtectionDistanceMeters;
	Vision.DarkProtectionDistanceMeters = 3.f;
	ON_SCOPE_EXIT { Vision.DarkProtectionDistanceMeters = OldProtectionDistance; };
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
	Vision.DarkProtectionDistanceMeters = 5.f;
	TestFalse(TEXT("Authored rule distance changes protection"), ATBRoomLighting::ProtectsFromGaze(World, Toy, FarHuman));
	Vision.DarkProtectionDistanceMeters = 3.f;
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
	const FTransform OldTransform = Character->GetActorTransform();
	const FRotator OldRotation = Controller->GetControlRotation();
	auto* Room = World->SpawnActor<ATBRoomLighting>();
	auto* Switch = World->SpawnActor<ATBLightSwitch>();
	ON_SCOPE_EXIT
	{
		State->Phase = OldPhase;
		Player->Team = OldTeam;
		Player->ToyState = OldToyState;
		Player->bFrozen = bOldFrozen;
		Character->SetActorTransform(OldTransform, false, nullptr, ETeleportType::TeleportPhysics);
		Controller->SetControlRotation(OldRotation);
		if (Switch) Switch->Destroy();
		if (Room) Room->Destroy();
	};
	if (!TestNotNull(TEXT("Room"), Room) || !TestNotNull(TEXT("Switch"), Switch)) return false;
	TBTest::FPlayingScope Playing(*State);
	Player->Team = ETBTeam::Toy;
	Player->ToyState = ETBToyState::Free;
	Player->bFrozen = false;
	const FVector Eye = Character->GetPawnViewLocation();
	const FVector Aim = Character->GetControlRotation().Vector();
	Switch->SetActorLocation(Eye + Aim * 100.f);
	Switch->Actions[0].bRequiresLineOfSight = false; // Shared Conditions test covers walls.
	auto* CustomData = NewObject<UTBLightSwitchData>(Switch);
	CustomData->InteractionDistance = 180.f;
	Switch->SwitchData = CustomData;
	Switch->OnConstruction(Switch->GetActorTransform());
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
	Switch->AddActorWorldOffset(Eye + Aim * 181.f - Switch->GetInteractionLocation());
	TestFalse(TEXT("Base reach blocks toggle"), Switch->TryInteract(Use));
	Switch->SetActorLocation(Eye + Aim * 100.f);
	Use.ActionId = TEXT("Missing");
	TestFalse(TEXT("Unknown action cannot toggle"), Switch->TryInteract(Use));
	TestTrue(TEXT("Rejected interactions preserve lighting"), Room->bLightsOn);
	CustomData->InteractionDistance = 250.f;
	Switch->OnConstruction(Switch->GetActorTransform());
	Switch->SetActorLocation(Eye + Aim * 220.f);
	Use.ActionId = TEXT("Toggle");
	TestTrue(TEXT("Selected data asset controls actual reach"), Switch->TryInteract(Use));
	Switch->SwitchData.Reset();
	Switch->OnConstruction(Switch->GetActorTransform());
	TestEqual(TEXT("Unset data falls back to original reach"), Switch->Actions[0].MaxDistance, 180.f);
	for (float Yaw : {0.f, 90.f})
	{
		Switch->SetActorTransform(FTransform(FRotator(0, Yaw, 0), FVector(0, 0, 10000), FVector(.4f, .7f, 1.2f)));
		Switch->InteractionPoint->SetRelativeLocation(FVector(30, 12, 8));
		TestTrue(TEXT("Hand-adjusted interaction point survives rotation and scale"),
		    Switch->GetInteractionLocation().Equals(Switch->InteractionPoint->GetComponentLocation(), .001));
	}
	Switch->InteractionPoint->SetRelativeLocation(FVector::ZeroVector);

	// Reproduce the Demo wall: visible plate, but simple collision extends 4 cm
	// beyond its face. The old pivot endpoint was inside that collision.
	auto* Wall = World->SpawnActor<AActor>();
	ON_SCOPE_EXIT { if (Wall) Wall->Destroy(); };
	auto* Collision = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Collision);
	Collision->SetBoxExtent(FVector(10, 100, 100));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Collision->RegisterComponent();
	Switch->Actions[0].bRequiresLineOfSight = true;
	Player->Team = ETBTeam::Human;
	const FVector EyeOffset = Character->GetPawnViewLocation() - Character->GetActorLocation();
	for (float Scale : {.3125f, .439453f, 1.f})
	{
		for (float Yaw : {0.f, 90.f, 180.f})
		{
			Switch->SetActorTransform(FTransform(FRotator(0, Yaw, 0), FVector(0, 0, 10000), FVector(Scale)));
			const FVector Forward = Switch->GetActorForwardVector();
			// Mesh bounds are independent of the interaction point under test.
			const FBox Bounds = Switch->GetComponentsBoundingBox(true);
			const FVector Face = Bounds.GetCenter() + Forward * FVector::DotProduct(Forward.GetAbs(), Bounds.GetExtent());
			Wall->SetActorLocationAndRotation(Face - Forward * 6.f, Switch->GetActorRotation());
			Character->SetActorLocation(Face + Forward * 90.f - EyeOffset);
			Controller->SetControlRotation((Face - Character->GetPawnViewLocation()).Rotation());
			TestFalse(TEXT("Fixture wall blocks original mounting pivot"), TB::ClearPath(Character, Switch, Character->GetPawnViewLocation(), Switch->GetActorLocation()));
			TestTrue(TEXT("Human can target visible wall-mounted switch"), TB::FindInteractionTarget(Character).Actor == Switch);
			TestTrue(TEXT("Human can use visible wall-mounted switch"), Switch->TryInteract(Use));
			Character->SetActorLocation(Face - Forward * 90.f - EyeOffset);
			TestFalse(TEXT("Wall still prevents using switch from behind"), Switch->CanInteract(Use));
			Character->SetActorLocation(Face + Forward * 90.f - EyeOffset);
			Wall->SetActorLocation(Face + Forward * 40.f);
			TestFalse(TEXT("Intervening obstacle still blocks switch"), Switch->CanInteract(Use));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBLightingDataTest, "ToonStory.Data.Lighting",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTBLightingDataTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Original protection default"), GetDefault<UTBRuleData>()->Vision.DarkProtectionDistanceMeters, 3.f);
	const auto& Selected = GetDefault<ATBLightSwitch>()->SwitchData;
	TestEqual(TEXT("Shared switch data path"), Selected.ToSoftObjectPath().ToString(), FString(TEXT("/Game/Data/DA_LightSwitch.DA_LightSwitch")));
	TestNotNull(TEXT("Shared switch data asset exists"), Selected.LoadSynchronous());
	return true;
}
#endif
