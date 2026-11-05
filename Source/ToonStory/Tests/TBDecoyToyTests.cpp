#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Items/TBDecoyToy.h"
#include "Character/TBCharacter.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Audio/TBCharacterAudioComponent.h"
#include "Data/TBCharacterData.h"
#include "Data/TBDecoyToyData.h"
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
	// 床、小さな段差、崖を独立したテスト空間で確認する。
	auto SpawnBox=[&](const FVector& Center,const FVector& Extent)
	{
		auto* Actor=World->SpawnActor<AActor>(Center,FRotator::ZeroRotator,Params);
		if (!Actor) return static_cast<AActor*>(nullptr);
		auto* Box=NewObject<UBoxComponent>(Actor); Actor->SetRootComponent(Box); Box->SetBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent(); Actor->SetActorLocation(Center);
		return Actor;
	};
	const FVector Base(10000,10000,10000);
	auto* Floor=SpawnBox(Base+FVector(0,0,-10),FVector(100,100,10));
	auto* Carpet=SpawnBox(Base+FVector(40,0,2),FVector(20,40,2));
	if (TestNotNull(TEXT("Floor fixture"),Floor) && TestNotNull(TEXT("Carpet fixture"),Carpet))
	{
		Decoy->bWalking=false; Decoy->SetActorLocation(Base+FVector(0,0,.5)); Decoy->SetActorRotation(FRotator::ZeroRotator);
		Decoy->Tick(.1f);
		TestTrue(TEXT("Settles onto floor"),Decoy->bGrounded);
		Decoy->bWalking=true;
		for (int32 I=0;I<10;++I) Decoy->Tick(.1f);
		TestTrue(TEXT("Walks onto small step"),Decoy->GetActorLocation().X>Base.X+25 && Decoy->GetActorLocation().Z>Base.Z+3);
		TestTrue(TEXT("Stays grounded on step"),Decoy->bGrounded);
		// 空中から開始しても停止中のまま重力が働き、床で止まる。
		Decoy->bWalking=false; Decoy->bGrounded=false; Decoy->SetActorLocation(Base+FVector(-50,0,80));
		for (int32 I=0;I<12;++I) Decoy->Tick(.1f);
		TestTrue(TEXT("Stationary airborne decoy lands"),Decoy->bGrounded && FMath::Abs(Decoy->GetActorLocation().Z-Base.Z)<2);
		// 床の外に出ると高さを維持せず落下する。
		Decoy->SetActorLocation(Base+FVector(99,70,.5)); Decoy->SetActorRotation(FRotator::ZeroRotator); Decoy->bWalking=true;
		for (int32 I=0;I<8;++I) Decoy->Tick(.1f);
		TestTrue(TEXT("Walks off ledge and falls"),Decoy->GetActorLocation().Z<Base.Z-10 && !Decoy->bGrounded);
	}
	if (Floor) Floor->Destroy();
	if (Carpet) Carpet->Destroy();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBDecoyFootstepTest,"ToonStory.Items.DecoyFootstepRule",EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBDecoyFootstepTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for (const auto& Entry:GEngine->GetWorldContexts())
		if (Entry.WorldType==EWorldType::Game && Entry.World() && Entry.World()->GetNetMode()==NM_Standalone) { World=Entry.World(); break; }
	if (!TestNotNull(TEXT("Standalone world"),World)) return false;
	auto* Decoy=World->SpawnActor<ATBDecoyToy>(FVector(0,0,10000),FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Decoy fixture"),Decoy)) return false;
	// A simulated proxy cannot destroy itself; restore authority before cleaning up the fixture.
	ON_SCOPE_EXIT { Decoy->SetRole(ROLE_Authority); Decoy->Destroy(); };
	// Inject the displacement seen by a remote listener without server movement or a real audio device.
	Decoy->SetRole(ROLE_SimulatedProxy); Decoy->SetActorTickEnabled(false);
	Decoy->bWalking=true; Decoy->bGrounded=true; Decoy->Tick(.05f);
	const auto& Toy=TB::CharacterData().Toy;
	const float Threshold=(Toy.WalkSpeed+Toy.SprintSpeed)*.5f;
	const float Interval=GetDefault<UTBCharacterAudioComponent>()->ToyStepDistance;
	Decoy->LoadedData=NewObject<UTBDecoyToyData>(Decoy);
	Decoy->LoadedData->WalkSpeed=Threshold+1;
	Decoy->FootstepSampleSeconds=0;
	auto Observe=[&](float Speed,float Delta=.05f)
	{
		Decoy->SetActorLocation(Decoy->GetActorLocation()+FVector(Speed*Delta,0,0));
		Decoy->Tick(Delta);
	};
	Decoy->StepDistance=Interval-.5f;
	Observe(Threshold-1);
	TestEqual(TEXT("Slow movement neither emits nor retains a partial footstep"),Decoy->StepDistance,0.f);
	Observe(Threshold+1);
	TestTrue(TEXT("Running movement accumulates footstep distance"),Decoy->StepDistance>0.f);
	Decoy->StepDistance=Interval-.5f; Observe(Threshold+1);
	TestTrue(TEXT("Running consumes one footstep at the stride boundary"),Decoy->StepDistance<Interval && Decoy->StepDistance>0.f);
	for (int32 I=0;I<6;++I) Observe(0);
	TestEqual(TEXT("Blocked decoy resets stride despite the configured speed"),Decoy->StepDistance,0.f);
	Decoy->StepDistance=10; Decoy->bWalking=false; Observe(Threshold+1);
	TestEqual(TEXT("Stopped decoy is silent even if its position changes"),Decoy->StepDistance,0.f);
	Decoy->bWalking=true; Decoy->StepDistance=0; Decoy->FootstepSampleSeconds=0;
	Observe(0); Observe(2*(Threshold+1));
	TestTrue(TEXT("Batched replicated running still accumulates a step"),Decoy->StepDistance>0.f);
	Decoy->bGrounded=false; Decoy->StepDistance=Interval-.5f; Observe(Threshold+1);
	TestEqual(TEXT("Airborne running does not emit or retain a footstep"),Decoy->StepDistance,0.f);
	Decoy->bGrounded=true;
	Decoy->LoadedData->WalkSpeed=Threshold-1; Decoy->StepDistance=Interval-.5f;
	Observe(2*(Threshold+1));
	TestEqual(TEXT("A walking decoy never sounds like a runner after a network update"),Decoy->StepDistance,0.f);
	Decoy->StepDistance=10; Observe(4000);
	TestEqual(TEXT("Teleport does not produce a footstep"),Decoy->StepDistance,0.f);
	return !HasAnyErrors();
}
#endif
