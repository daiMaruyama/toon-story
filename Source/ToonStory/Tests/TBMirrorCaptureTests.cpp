#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Items/TBMirror.h"
#include "Core/TBController.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Tests/TBTestMatch.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/GameModeBase.h"
#include "Misc/CommandLine.h"
#include "EngineUtils.h"

// 実際のGPU遮蔽判定を使う。NullRHIの通常テストでは実行しない。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMirrorCaptureTest,"ToonStory.Mirror.CaptureOcclusion",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMirrorCaptureTest::RunTest(const FString&)
{
	if(!FParse::Param(FCommandLine::Get(),TEXT("MirrorCaptureTest"))) return true;
	UWorld* World=nullptr;
	for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game) World=Context.World();
	if(!World || World->GetNetMode()!=NM_Standalone) return false;
	auto* PC=Cast<ATBController>(World->GetFirstPlayerController());
	auto* State=World->GetGameState<ATBGameState>(); auto* Mode=World->GetAuthGameMode();
	if(!PC || !State || !Mode) return false;
	auto* Player=PC->GetPlayerState<ATBPlayerState>(); if(!Player) return false;
	const auto OldTeam=Player->Team; const auto OldRound=State->HidePreparation.Round;
	auto Scope=MakeShared<TBTest::FPlayingScope>(*State,World->GetTimeSeconds()+600);
	Player->Team=ETBTeam::Human; ++State->HidePreparation.Round;
	PC->ClientPreparationRole(State->HidePreparation.Round,ETBTeam::Human);
	const bool ModeTick=Mode->IsActorTickEnabled(); Mode->SetActorTickEnabled(false);
	auto* PreviousView=PC->GetViewTarget();
	ATBMirror* Mirror=nullptr;
	const bool Authored=World->GetMapName().Contains(TEXT("TB_StylizedHouse"));
	if(Authored) { for(TActorIterator<ATBMirror> It(World);It;++It) { Mirror=*It; break; } }
	else Mirror=World->SpawnActor<ATBMirror>(FVector(0,0,10000),FRotator::ZeroRotator);
	if(!Mirror) { Mode->SetActorTickEnabled(ModeTick); Player->Team=OldTeam; PC->ClientPreparationRole(OldRound,OldTeam); return false; }
	const FVector Eye=Authored ? FVector(260,-430,150) : FVector(220,15,10015);
	auto* Camera=World->SpawnActor<ACameraActor>(Eye,(Mirror->GetActorLocation()-Eye).Rotation());
	Camera->GetCameraComponent()->FieldOfView=65;
	auto* Wall=World->SpawnActor<AStaticMeshActor>();
	Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
	Wall->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wall->SetActorLocation((Eye+Mirror->GetActorLocation())*.5);
	Wall->SetActorScale3D(FVector(.1,4,5)); Wall->SetActorHiddenInGame(true);
	struct FMeasurement { int32 Phase=0,Frames=0; double Start=FPlatformTime::Seconds(),Capture=0; };
	auto Measurement=MakeShared<FMeasurement>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([=,this]() {
		Scope->Hold(); PC->SetViewTarget(Camera); PC->PlayerCameraManager->UpdateCamera(.016f);
		const double Elapsed=FPlatformTime::Seconds()-Measurement->Start;
		// 各状態は3秒待ってから5秒計測し、シェーダーや遮蔽クエリの準備を除く。
		if(Elapsed>3) ++Measurement->Frames;
		if(Elapsed<8) return false;
		AddInfo(FString::Printf(TEXT("MIRROR_PERF phase=%d fps=%.2f frame_ms=%.2f captures_active=%d"),
			Measurement->Phase,Measurement->Frames/(Elapsed-3),(Elapsed-3)*1000/FMath::Max(1,Measurement->Frames),Mirror->NextCapture>Measurement->Capture));
		if(Measurement->Phase==0) {
			TestNotNull(TEXT("Visible mirror starts capture"),Mirror->TargetTexture.Get());
			TestTrue(TEXT("Visible mirror continues capture"),Mirror->NextCapture>0);
			Wall->SetActorHiddenInGame(false);
		} else if(Measurement->Phase==1) {
			// 遮蔽クエリの遅延が収束してから、さらに8秒更新されないことを確認。
			Measurement->Capture=Mirror->NextCapture;
		} else if(Measurement->Phase==2) {
			TestEqual(TEXT("Fully occluded mirror stops capturing"),Mirror->NextCapture,Measurement->Capture);
			Wall->SetActorHiddenInGame(true);
		} else {
			TestTrue(TEXT("Uncovered mirror resumes capturing"),Mirror->NextCapture>Measurement->Capture);
			Player->Team=OldTeam; PC->ClientPreparationRole(OldRound,OldTeam);
			PC->SetViewTarget(PreviousView); Wall->Destroy(); Camera->Destroy();
			if(!Authored) Mirror->Destroy(); Mode->SetActorTickEnabled(ModeTick);
			return true;
		}
		++Measurement->Phase; Measurement->Frames=0; Measurement->Start=FPlatformTime::Seconds();
		return false;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMirrorServerTest,"ToonStory.Mirror.DedicatedServer",
 EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FTBMirrorServerTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game) World=Context.World();
	if(!World || World->GetNetMode()!=NM_DedicatedServer) { AddError(TEXT("Requires a dedicated server world")); return false; }
	auto* Mirror=World->SpawnActor<ATBMirror>();
	TestFalse(TEXT("Dedicated server mirror tick disabled"),Mirror->IsActorTickEnabled());
	TestFalse(TEXT("Tick never registers on dedicated server"),Mirror->PrimaryActorTick.bAllowTickOnDedicatedServer);
	TestNull(TEXT("Dedicated server has no render target"),Mirror->ReflectionCamera->TextureTarget.Get());
	FVector Hit;
	TestTrue(TEXT("Dedicated server still computes reflections"),Mirror->ReflectionPoint(FVector(200,-80,0),FVector(200,80,0),Hit));
	Mirror->Destroy();
	return true;
}
#endif
