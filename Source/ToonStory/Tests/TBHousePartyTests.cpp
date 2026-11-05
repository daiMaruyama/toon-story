#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBController.h"
#include "Character/TBCharacter.h"
#include "Character/TBMovement.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Core/TBLobbySettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBHousePartyTest, "ToonStory.Playroom.HouseParty", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBHousePartyTest::RunTest(const FString&)
{
 UWorld* World=nullptr;
 for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game) World=C.World();
 if(!World || World->GetNetMode()!=NM_Standalone || !World->GetMapName().Contains(TEXT("StylizedHouse"))) return false;
 auto* Mode=World->GetAuthGameMode<ATBGameMode>(); auto* State=World->GetGameState<ATBGameState>();
 auto* Host=Cast<ATBController>(World->GetFirstPlayerController());
 if(!Mode || !State || !Host || State->Phase!=ETBPhase::Lobby) return false;
 TArray<APlayerController*> Guests;
 ON_SCOPE_EXIT {
  if(State->Phase==ETBPhase::Playing) Mode->Finish(ETBWinner::Humans,TEXT("test cleanup"));
  if(State->Phase==ETBPhase::Results || State->Phase==ETBPhase::Aborted) Mode->ReturnToLobby(Host);
  for(auto* PC:Guests) UGameplayStatics::RemovePlayer(PC,true);
 };
 // Reproduce the authored marker that stranded the third Steam participant.
 for(int32 Index=0;Index<Mode->SpawnPoints.Num();++Index)
  if(Mode->SpawnPoints[Index]->GetActorLocation().Equals(FVector(-670,180,111.704456),1.f)) Mode->SpawnCursor=Index;
 for(int32 I=0;I<2;++I) {
  auto* PC=UGameplayStatics::CreatePlayer(World,-1,true);
  if(!TestNotNull(TEXT("Three participants can join"),PC)) return false;
  Guests.Add(PC);
  if(!TestNotNull(TEXT("Blocked arrival marker is skipped and guest has a pawn"),PC->GetPawn<APawn>())) return false;
 }
 Host->GetPlayerState<ATBPlayerState>()->Preference=ETBTeam::Human;
 for(auto* PC:Guests) PC->GetPlayerState<ATBPlayerState>()->Preference=ETBTeam::Toy;
 Mode->SyncLobbyCounts();
 auto Rules=State->Settings; Rules.Humans=1; Rules.Toys=2; Rules.NPCHumans=0; Rules.NPCToys=0; Rules.RequiredItems=1;
 Mode->SetRules(Host,Rules);
 for(APlayerState* P:State->PlayerArray) CastChecked<ATBPlayerState>(P)->bReady=true;
 Mode->StartRound(Host);
 TestEqual(*FString::Printf(TEXT("Three-player house round starts: %s / %s"),*State->Reason,*Host->Notice),State->Phase,ETBPhase::Playing);
 if(State->Phase!=ETBPhase::Playing) return false;

 auto* Toy=Guests[0]->GetPawn<ATBCharacter>();
 auto* Move=CastChecked<UTBMovement>(Toy->GetCharacterMovement());

 const FTransform OriginalToy=Toy->GetActorTransform();
 auto* Floor=World->SpawnActor<AActor>();
 auto* Surface=NewObject<UBoxComponent>(Floor);
 Floor->SetRootComponent(Surface); Surface->SetBoxExtent(FVector(100,100,10));
 Surface->SetCollisionProfileName(TEXT("BlockAll")); Surface->RegisterComponent();
 Floor->SetActorLocation(FVector(0,0,10000));
 Toy->SetActorLocation(FVector(0,0,10010+Toy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.15));
 Move->Velocity=FVector::ZeroVector;
 Move->SetMovementMode(MOVE_Walking);
 Move->FindFloor(Toy->GetActorLocation(),Move->CurrentFloor,false);
 TestTrue(TEXT("Freeze fixture has a walkable floor"),Move->CurrentFloor.IsWalkableFloor());
 const double Z=Toy->GetActorLocation().Z;
 Toy->SetFrozen(true);
 for(int I=0;I<120;++I) Move->PhysCustom(1.f/60.f,0);
 TestTrue(TEXT("Frozen toy retains its standing height"),FMath::IsNearlyEqual(Z,Toy->GetActorLocation().Z,.1));
 Toy->SetFrozen(false);
 Toy->SetActorTransform(OriginalToy); Floor->Destroy();

 Mode->Finish(ETBWinner::Humans,TEXT("test victory"));
 TestFalse(TEXT("Winner can move"),Host->GetPawn<ATBCharacter>()->IsMovementLocked());
 for(auto* PC:Guests) TestTrue(TEXT("Losers cannot move"),PC->GetPawn<ATBCharacter>()->IsMovementLocked());
 TestTrue(TEXT("Ten-second celebration"),FMath::IsNearlyEqual(State->EndTime-State->GetServerWorldTimeSeconds(),10.,.1));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBHouseNetworkTest, "ToonStory.Playroom.HouseNetwork", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBHouseNetworkTest::RunTest(const FString&)
{
 UWorld* World=nullptr;
 for(const auto& C:GEngine->GetWorldContexts()) if(C.WorldType==EWorldType::Game) World=C.World();
 if(!World || World->GetNetMode()!=NM_ListenServer) return false;
 const double Deadline=FPlatformTime::Seconds()+150;
 auto* Instance=World->GetGameInstance();
 auto Step=MakeShared<int32>(0);
 ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this,Instance,Deadline,Step]() {
  auto* Current=Instance->GetWorld();
  auto* State=Current ? Current->GetGameState<ATBGameState>() : nullptr;
  auto* Mode=Current ? Current->GetAuthGameMode<ATBGameMode>() : nullptr;
  if(FPlatformTime::Seconds()>Deadline) { AddError(TEXT("Network start/return timed out")); return true; }
  if(!State || !Mode || Current->IsInSeamlessTravel()) return false;
  if(*Step==0) {
   if(State->PlayerArray.Num()<3) return false;
   auto* Host=Cast<ATBController>(Current->GetFirstPlayerController());
   for(APlayerState* P:State->PlayerArray) {
    auto* Info=CastChecked<ATBPlayerState>(P);
    if(!Info->GetPawn()) return false;
    Info->bReady=true; Info->Preference=Info==Host->PlayerState ? ETBTeam::Human : ETBTeam::Toy;
   }
   Host->ServerSelectStage(TBLobbySettings::HouseStageId);
   Mode->SyncLobbyCounts(); Mode->StartRound(Host); *Step=1; return false;
  }
  if(*Step==1) {
   if(State->Phase!=ETBPhase::Playing || State->IsPreparing()) return false;
   TestTrue(TEXT("House loaded for three connected players"),Current->GetMapName().Contains(TEXT("StylizedHouse")) && State->PlayerArray.Num()==3);
   Mode->Finish(ETBWinner::Humans,TEXT("network victory test")); *Step=2; return false;
  }
  if(*Step==2 && State->Phase==ETBPhase::Lobby && Current->GetMapName().Contains(TEXT("TB_Lobby"))) {
   TestEqual(TEXT("All three remain connected after automatic return"),State->PlayerArray.Num(),3);
   return true;
  }
  return false;
 }));
 return true;
}
#endif
