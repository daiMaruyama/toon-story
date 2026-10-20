#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/TBLightSwitch.h"
#include "GamePlay/TBRoomLighting.h"
#include "GamePlay/Items/TBInteractionTarget.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBAuthoredLightingTest, "ToonStory.Lighting.AuthoredStage",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBAuthoredLightingTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Entry : GEngine->GetWorldContexts())
	{
		if (Entry.WorldType == EWorldType::Game && Entry.World() && Entry.World()->GetGameState<ATBGameState>())
		{ World = Entry.World(); break; }
	}
	if (!TestNotNull(TEXT("Standalone game world"), World) || World->GetNetMode() != NM_Standalone) return false;
	auto* Controller = World->GetFirstPlayerController();
	auto* Character = Controller ? Cast<ATBCharacter>(Controller->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Player state"), Character->TBPS())) return false;
	auto* Player = Character->TBPS();
	auto* State = World->GetGameState<ATBGameState>();
	const auto OldPhase = State->Phase;
	const auto OldTeam = Player->Team;
	const auto OldToyState = Player->ToyState;
	const bool bOldFrozen = Player->bFrozen;
	const FTransform OldTransform = Character->GetActorTransform();
	const FRotator OldRotation = Controller->GetControlRotation();
	ON_SCOPE_EXIT
	{
		State->Phase = OldPhase;
		Player->Team = OldTeam;
		Player->ToyState = OldToyState;
		Player->bFrozen = bOldFrozen;
		Character->ApplyState();
		Character->SetActorTransform(OldTransform, false, nullptr, ETeleportType::TeleportPhysics);
		Controller->SetControlRotation(OldRotation);
	};
	State->Phase = ETBPhase::Playing;
	Player->ToyState = ETBToyState::Free;
	Player->bFrozen = false;
	TSet<ALight*> AssignedLights;
	int32 RoomCount = 0, SwitchCount = 0;
	for (TActorIterator<ATBRoomLighting> It(World); It; ++It)
	{
		++RoomCount;
		TestTrue(TEXT("Room has lights"), !It->Lights.IsEmpty());
		for (ALight* Light : It->Lights)
		{
			if (!TestNotNull(TEXT("Saved light reference resolves"), Light)) continue;
			TestFalse(TEXT("Each light belongs to one room"), AssignedLights.Contains(Light));
			AssignedLights.Add(Light);
			TestEqual(TEXT("Light can change at runtime"), Light->GetLightComponent()->Mobility.GetValue(), EComponentMobility::Movable);
		}
	}
	for (TActorIterator<ATBLightSwitch> It(World); It; ++It)
	{
		++SwitchCount;
		auto* Switch = *It;
		auto* Room = Switch->Room.Get();
		if (!TestNotNull(TEXT("Saved room reference resolves"), Room)) continue;
		const bool bInitial = Room->bLightsOn;
		ON_SCOPE_EXIT { Room->SetLightsOn(bInitial); };
		for (ETBTeam Team : {ETBTeam::Human, ETBTeam::Toy})
		{
			Player->Team = Team;
			Character->ApplyState();
			const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
			FCollisionQueryParams Query(SCENE_QUERY_STAT(TBLightingReach), false, Character);
			Query.AddIgnoredActor(Switch);
			FTBInteractionContext Use;
			Use.InstigatorCharacter = Character;
			Use.ActionId = TEXT("Toggle");
			bool bReachable = false;
			for (float Distance : {65.f, 90.f, 115.f})
			{
				for (float Side : {0.f, -40.f, 40.f})
				{
					FVector Position = Switch->GetActorLocation() + Switch->GetActorForwardVector() * Distance + Switch->GetActorRightVector() * Side;
					Position.Z = Switch->GetActorLocation().Z - 55.f + HalfHeight + 2.f;
					if (!Room->Contains(Position) || World->OverlapBlockingTestByChannel(Position, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Query)) continue;
					FHitResult Floor;
					if (!World->LineTraceSingleByChannel(Floor, Position, Position - FVector(0, 0, HalfHeight + 10), ECC_Pawn, Query)) continue;
					Character->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
					Controller->SetControlRotation((Switch->GetInteractionLocation() - Character->GetPawnViewLocation()).Rotation());
					if (Switch->CanInteract(Use) && TB::FindInteractionTarget(Character).Actor == Switch)
					{ bReachable = true; break; }
				}
				if (bReachable) break;
			}
			if (!TestTrue(*FString::Printf(TEXT("%s reachable by team %d from clear floor"), *Switch->GetName(), static_cast<int32>(Team)), bReachable)) continue;
			Room->SetLightsOn(true);
			TestTrue(TEXT("Interact turns room off"), Switch->TryInteract(Use) && !Room->bLightsOn);
			for (ALight* Light : Room->Lights)
				if (Light) TestFalse(TEXT("Real light visibility turns off"), Light->GetLightComponent()->IsVisible());
			TestTrue(TEXT("Interact restores room light"), Switch->TryInteract(Use) && Room->bLightsOn);
		}
	}
	TestEqual(TEXT("Expected furnished room count"), RoomCount, World->GetMapName().Contains(TEXT("ArchViz")) ? 4 : 2);
	TestEqual(TEXT("One switch per room"), SwitchCount, RoomCount);
	return true;
}
#endif
