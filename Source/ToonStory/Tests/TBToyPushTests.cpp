#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBCharacter.h"
#include "Character/TBMovement.h"
#include "Components/BoxComponent.h"
#include "Core/TBPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "Tests/TBTestMatch.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBToyPushTest, "ToonStory.Interaction.ToyPush",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBToyPushTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
		if (Context.WorldType == EWorldType::Game) World = Context.World();
	auto* State = World ? World->GetGameState<ATBGameState>() : nullptr;
	if (!TestNotNull(TEXT("Standalone match"), State) || World->GetNetMode() != NM_Standalone) return false;
	TBTest::FPlayingScope Playing(*State);
	TArray<AActor*> Fixtures;
	ON_SCOPE_EXIT { for (AActor* Actor : Fixtures) Actor->Destroy(); };
	auto MakeToy = [&](FVector Position)
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Toy = World->SpawnActor<ATBCharacter>(Position, FRotator::ZeroRotator, Spawn);
		auto* Info = World->SpawnActor<ATBPlayerState>();
		Fixtures.Add(Toy);
		Fixtures.Add(Info);
		Info->Team = ETBTeam::Toy;
		Info->ToyState = ETBToyState::Free;
		Toy->SetPlayerState(Info);
		Toy->ApplyState();
		return Toy;
	};
	auto MakeBox = [&](FVector Position, bool bProp)
	{
		auto* Actor = World->SpawnActor<AActor>();
		Fixtures.Add(Actor);
		auto* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(4, 12, 12));
		Box->SetWorldLocation(Position);
		Box->SetCollisionProfileName(bProp ? TEXT("PhysicsActor") : TEXT("BlockAll"));
		Box->RegisterComponent();
		if (bProp)
		{
			Box->ComponentTags.Add(TEXT("TBPushable"));
			Box->SetEnableGravity(false);
			Box->SetSimulatePhysics(true);
		}
		return Box;
	};
	const FVector Start(10000, 0, 10000);
	auto* Pusher = MakeToy(Start);
	auto* Ally = MakeToy(Start + FVector(25, 0, 0));
	Pusher->TBPS()->bFrozen = true;
	TestFalse(TEXT("Frozen toy cannot push"), Pusher->TryPush());
	Pusher->TBPS()->bFrozen = false;
	TestTrue(TEXT("Free toy can push its teammate"), Pusher->TryPush());
	TestTrue(TEXT("Teammate launched forward"), Ally->GetCharacterMovement()->PendingLaunchVelocity.X > 0);
	TestFalse(TEXT("Repeated push is rate limited"), Pusher->TryPush());

	const FVector PropLane = Start + FVector(0, 200, 0);
	auto* PropPusher = MakeToy(PropLane);
	auto* Prop = MakeBox(PropLane + FVector(32, 0, 0), true);
	auto* Passive = MakeBox(PropLane + FVector(0, 100, 0), true);
	FHitResult Impact(Passive->GetOwner(), Passive, Passive->Bounds.Origin, -FVector::ForwardVector);
	CastChecked<UTBMovement>(PropPusher->GetCharacterMovement())->ApplyImpactPhysicsForces(
	    Impact, FVector(500, 0, 0), FVector(200, 0, 0));
	TestTrue(TEXT("Tagged physics prop can be pushed"), PropPusher->TryPush());
	// Chaos may consume queued impulses on the next simulation step.
	Fixtures.Remove(Prop->GetOwner());
	Fixtures.Remove(Passive->GetOwner());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Prop, Passive]()
	{
		TestTrue(TEXT("Explicit push moves the prop forward"), Prop->GetPhysicsLinearVelocity().X > 0);
		TestTrue(TEXT("Ordinary toy contact leaves the prop still"), Passive->GetPhysicsLinearVelocity().IsNearlyZero());
		Prop->GetOwner()->Destroy();
		Passive->GetOwner()->Destroy();
		return true;
	}));

	const FVector WallLane = Start + FVector(0, 400, 0);
	auto* BehindWall = MakeToy(WallLane);
	MakeBox(WallLane + FVector(16, 0, 0), false);
	MakeBox(WallLane + FVector(36, 0, 0), true);
	TestFalse(TEXT("A wall blocks a nearby prop"), BehindWall->TryPush());
	return true;
}
#endif
