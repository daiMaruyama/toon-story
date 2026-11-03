#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Items/TBDecoyToy.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Tests/TBTestMatch.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBDecoyToyTest,"ToonStory.Items.DecoyToy",EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBDecoyToyTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for (const auto& Entry:GEngine->GetWorldContexts())
		if (Entry.WorldType==EWorldType::Game && Entry.World() && Entry.World()->GetGameState<ATBGameState>()) { World=Entry.World(); break; }
	if (!World || World->GetNetMode()!=NM_Standalone) return false;
	auto* Controller=World->GetFirstPlayerController();
	auto* Character=Controller ? Cast<ATBCharacter>(Controller->GetPawn()) : nullptr;
	if (!TestNotNull(TEXT("Character"),Character) || !TestNotNull(TEXT("Player state"),Character->TBPS())) return false;
	auto* Player=Character->TBPS();
	const auto OldTeam=Player->Team; const auto OldState=Player->ToyState; const bool OldFrozen=Player->bFrozen;
	ON_SCOPE_EXIT { Player->Team=OldTeam; Player->ToyState=OldState; Player->bFrozen=OldFrozen; };
	TBTest::FPlayingScope Playing(*World->GetGameState<ATBGameState>());
	Player->Team=ETBTeam::Toy; Player->ToyState=ETBToyState::Free; Player->bFrozen=false;
	FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Decoy=World->SpawnActor<ATBDecoyToy>(Character->GetPawnViewLocation()+FVector(100,0,-12),FRotator::ZeroRotator,Params);
	if (!TestNotNull(TEXT("Decoy"),Decoy)) return false;
	ON_SCOPE_EXIT { Decoy->Destroy(); };
	TestNotNull(TEXT("Appearance selected from toy catalogue"),Decoy->VisualClass.Get());
	TestNotNull(TEXT("Selected appearance spawned"),Decoy->Visual->GetChildActor());
	for (auto& Action:Decoy->Actions) Action.bRequiresLineOfSight=false;
	FTBInteractionContext Context; Context.InstigatorCharacter=Character; Context.ActionId=TEXT("Start");
	TestFalse(TEXT("Initially stopped"),Decoy->bWalking);
	TestTrue(TEXT("Toy starts decoy"),Decoy->TryInteract(Context));
	TestTrue(TEXT("Active after start"),Decoy->bWalking);
	TestFalse(TEXT("Toy cannot stop it"),([&] { Context.ActionId=TEXT("Stop"); return Decoy->TryInteract(Context); })());
	Player->Team=ETBTeam::Human;
	TestTrue(TEXT("Human stops decoy"),Decoy->TryInteract(Context));
	TestFalse(TEXT("Stopped after human interaction"),Decoy->bWalking);
	// 建物から離れたテスト空間に壁を作り、斜め入射のX成分だけが反転することを検証する。
	const FVector TestOrigin(0,0,10000);
	auto* Wall=World->SpawnActor<AActor>(TestOrigin+FVector(11,0,10),FRotator::ZeroRotator,Params);
	if (TestNotNull(TEXT("Wall fixture"),Wall))
	{
		auto* Box=NewObject<UBoxComponent>(Wall);
		Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(1,100,100));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent(); Wall->SetActorLocation(TestOrigin+FVector(11,0,10));
		Decoy->SetActorLocation(TestOrigin); Decoy->SetActorRotation(FRotator(0,45,0)); Decoy->bWalking=true;
		Decoy->Tick(.1f);
		const FVector Reflected=Decoy->GetActorForwardVector();
		TestTrue(TEXT("Wall reflects horizontal direction"),Reflected.X < -.5 && Reflected.Y > .5);
		Wall->Destroy();
	}
	return true;
}
#endif
