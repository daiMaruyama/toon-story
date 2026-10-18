#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBSession.h"
#include "Character/TBCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

namespace
{
	UWorld* SafetyWorld()
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game)
			{
				return Context.World();
			}
		}
		return nullptr;
	}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBSafetyGuards, "ToonStory.Safety.GuardsAndCollision",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBSafetyGuards::RunTest(const FString& Parameters)
{
	auto* World = SafetyWorld();
	if (!World || World->GetNetMode() != NM_Standalone)
	{
		return false;
	}
	auto* PC = Cast<ATBController>(World->GetFirstPlayerController());
	auto* Body = PC ? Cast<ATBCharacter>(PC->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Character"), Body))
	{
		return false;
	}
	auto* State = World->GetGameState<ATBGameState>();
	auto* Info = Body->TBPS();
	const auto OriginalTeam = Info->Team;
	const auto OriginalPhase = State->Phase;
	const FTransform OriginalTransform = Body->GetActorTransform();
	TArray<AActor*> Fixtures;
	ON_SCOPE_EXIT
	{
		State->Phase = OriginalPhase;
		State->bTravelPending = false;
		Info->Team = OriginalTeam;
		Info->ToyState = ETBToyState::Free;
		Info->bFrozen = false;
		Body->Carrier = nullptr;
		Body->CarriedToy = nullptr;
		Body->bHoldingInteract = false;
		Body->ApplyState();
		Body->SetActorTransform(OriginalTransform);
		PC->bStuckHint = false;
		PC->RecoveryEnds = 0;
		for (auto* Actor : Fixtures)
		{
			Actor->Destroy();
		}
	};
	State->Phase = ETBPhase::Lobby;
	Info->Team = ETBTeam::Human;
	Body->ApplyState();
	auto Box = [&](FVector Location, FVector Extent)
	{
		auto* Actor = World->SpawnActor<AActor>();
		Fixtures.Add(Actor);
		auto* Collision = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Collision);
		Actor->AddInstanceComponent(Collision);
		Collision->SetBoxExtent(Extent);
		Collision->SetCollisionProfileName(TEXT("BlockAll"));
		Collision->RegisterComponent();
		Actor->SetActorLocation(Location);
	};
	Box(FVector(0, 0, 3000), FVector(1000, 1000, 10));
	const float Half = Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius = Body->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Origin(0, 0, 3012.5f + Half);
	Body->SetActorLocation(Origin);
	TestTrue(TEXT("Free human eligible"), PC->CanRecover());
	PC->ServerUnstuck();
	TestEqual(TEXT("Standing still is not a stuck signal"), PC->RecoveryEnds, 0.0);
	FVector Destination;
	TestTrue(TEXT("Safe nearby floor found"), PC->FindRecoveryLocation(Destination));
	TestTrue(TEXT("Recovery remains within 110cm"), FVector::Dist(Origin, Destination) <= 110);
	Info->bFrozen = true;
	TestFalse(TEXT("Frozen denied"), PC->CanRecover());
	Info->bFrozen = false;
	Info->ToyState = ETBToyState::Boxed;
	TestFalse(TEXT("Boxed denied"), PC->CanRecover());
	Info->ToyState = ETBToyState::Free;
	Body->Carrier = Body;
	TestFalse(TEXT("Carried denied"), PC->CanRecover());
	Body->Carrier = nullptr;
	Body->CarriedToy = Body;
	TestFalse(TEXT("Carrier denied"), PC->CanRecover());
	Body->CarriedToy = nullptr;
	Body->bHoldingInteract = true;
	TestFalse(TEXT("Interacting denied"), PC->CanRecover());
	Body->bHoldingInteract = false;
	State->bTravelPending = true;
	TestFalse(TEXT("Travel denied"), PC->CanRecover());
	State->bTravelPending = false;
	State->Phase = ETBPhase::Results;
	TestFalse(TEXT("Results denied"), PC->CanRecover());
	State->Phase = ETBPhase::Lobby;
	PC->ServerKickPlayer(Info->GetPlayerId());
	TestTrue(TEXT("Host cannot kick itself"), IsValid(PC) && PC->GetPawn() == Body);
	PC->ServerKickPlayer(-999);
	TestTrue(TEXT("Missing target harmless"), IsValid(PC));
	PC->TickActor(.01f, LEVELTICK_All, PC->PrimaryActorTick);
	PC->bStuckHint = true;
	PC->ServerUnstuck();
	TestTrue(TEXT("Verified stuck request starts windup"), PC->RecoveryEnds > World->GetTimeSeconds());
	Info->bFrozen = true;
	PC->TickActor(.01f, LEVELTICK_All, PC->PrimaryActorTick);
	TestEqual(TEXT("Capture during windup cancels"), PC->RecoveryEnds, 0.0);
	Info->bFrozen = false;
	PC->bStuckHint = true;
	PC->ServerUnstuck();
	PC->RecoveryEnds = World->GetTimeSeconds() - .01;
	PC->TickActor(.01f, LEVELTICK_All, PC->PrimaryActorTick);
	TestTrue(TEXT("Confirmed recovery moves to nearby floor"), FVector::Dist(Origin, Body->GetActorLocation()) > 20);
	TestTrue(TEXT("Confirmed recovery is local"), FVector::Dist(Origin, Body->GetActorLocation()) <= 110);
	PC->bStuckHint = true;
	PC->ServerUnstuck();
	TestEqual(TEXT("Cooldown prevents repeated relocation"), PC->RecoveryEnds, 0.0);
	Body->SetActorLocation(Origin);
	Box(Origin + FVector(Radius + 7, 0, 0), FVector(5, 200, 200));
	Box(Origin - FVector(Radius + 7, 0, 0), FVector(5, 200, 200));
	Box(Origin + FVector(0, Radius + 7, 0), FVector(200, 5, 200));
	Box(Origin - FVector(0, Radius + 7, 0), FVector(200, 5, 200));
	TestFalse(TEXT("Cannot teleport through enclosure"), PC->FindRecoveryLocation(Destination));
	return true;
}

class FTBSafetyNetwork : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	TWeakObjectPtr<UGameInstance> Instance;
	bool Host;
 FVector RemoteOrigin = FVector::ZeroVector;
	int32 Step = 0;
	double Began = FPlatformTime::Seconds(), Changed = 0;

public:
	FTBSafetyNetwork(FAutomationTestBase* InTest, UGameInstance* GI, bool InHost)
	    : Test(InTest), Instance(GI), Host(InHost)
	{
	}
	bool Update() override
	{
		if (FPlatformTime::Seconds() - Began > 90)
		{
			Test->AddError(TEXT("Kick network test timed out"));
			return true;
		}
		auto* GI = Instance.Get();
		auto* World = GI ? GI->GetWorld() : nullptr;
		if (!World)
		{
			return false;
		}
		auto* PC = Cast<ATBController>(GI->GetFirstLocalPlayerController(World));
		auto* State = World->GetGameState<ATBGameState>();
		auto* Session = GI->GetSubsystem<UTBSession>();
		if (!Host && Step > 0 && World->GetMapName().EndsWith(TEXT("Title")) && !Session->IsBusy())
		{
			Test->TestTrue(TEXT("Kicked client sees explicit reason"),
			               Session->Status.Contains(TEXT("ホストによって")));
			Test->TestEqual(TEXT("Kicked client is offline"), World->GetNetMode(), NM_Standalone);
			return true;
		}
		if (!PC || !State || State->Phase != ETBPhase::Lobby)
		{
			return false;
		}
		if (Host)
		{
			if (Step == 0 && State->PlayerArray.Num() == 2)
			{
				for (APlayerState* P : State->PlayerArray)
				{
					if (auto* Info = Cast<ATBPlayerState>(P); Info && Info != PC->PlayerState && Info->bReady)
					{
						auto* Remote = Cast<ATBController>(Info->GetOwner());
                        if (!Remote || !Remote->GetPawn()) return false;
                        RemoteOrigin = Remote->GetPawn()->GetActorLocation();
                        Remote->bStuckHint = true;
                        Remote->ServerUnstuck();
                        Test->TestTrue(TEXT("Remote recovery started"), Remote->RecoveryEnds > 0);
                        Step = 1;
                        Changed = FPlatformTime::Seconds();
					}
				}
			}
			else if (Step == 1 && FPlatformTime::Seconds() - Changed > 5)
			{
				Test->TestEqual(TEXT("Client cannot kick host"), State->PlayerArray.Num(), 2);
				for (APlayerState* P : State->PlayerArray)
				{
					if (P != PC->PlayerState)
					{
						auto* Remote = Cast<ATBController>(P->GetOwner());
                        Test->TestTrue(TEXT("Remote recovery completes without PlayerInput"), Remote && Remote->RecoveryEnds == 0 &&
                            Remote->GetPawn() && FVector::Dist(RemoteOrigin, Remote->GetPawn()->GetActorLocation()) > 20);
                        PC->ServerKickPlayer(P->GetPlayerId());
                        break;
					}
				}
				Step = 2;
				Changed = FPlatformTime::Seconds();
			}
			else if (Step == 2 && FPlatformTime::Seconds() - Changed > 4)
			{
				Test->TestEqual(TEXT("Only kicked peer removed"), State->PlayerArray.Num(), 1);
				Test->TestEqual(TEXT("Host keeps room open"), World->GetNetMode(), NM_ListenServer);
				Test->TestNotNull(TEXT("Host pawn preserved"), PC->GetPawn().Get());
				return true;
			}
		}
		else if (Step == 0 && State->PlayerArray.Num() == 2)
		{
			for (APlayerState* P : State->PlayerArray)
			{
				if (P != PC->PlayerState)
				{
					PC->ServerKickPlayer(P->GetPlayerId());
					break;
				}
			}
			PC->ServerReady();
			Step = 1;
		}
		return false;
	}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBSafetyHost, "ToonStory.SafetyNetwork.Host",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBSafetyHost::RunTest(const FString& Parameters)
{
	auto* World = SafetyWorld();
	if (!World)
	{
		return false;
	}
	World->GetGameInstance()->GetSubsystem<UTBSession>()->Host();
	ADD_LATENT_AUTOMATION_COMMAND(FTBSafetyNetwork(this, World->GetGameInstance(), true));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBSafetyClient, "ToonStory.SafetyNetwork.Client",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBSafetyClient::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("UEngine::BroadcastNetworkFailure: FailureType = (FailureReceived|ConnectionLost)"),
	                 EAutomationExpectedErrorFlags::Contains, 0);
	auto* World = SafetyWorld();
	if (!World)
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FTBSafetyNetwork(this, World->GetGameInstance(), false));
	return true;
}
#endif
