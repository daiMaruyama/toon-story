#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlay/Items/TBMirror.h"
#include "GamePlay/TBRoomLighting.h"
#include "Data/TBMirrorData.h"
#include "Character/TBCharacter.h"
#include "Character/TBNPCController.h"
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameHelpers.h"
#include "Character/TBCharacterGeometry.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/TBTestMatch.h"
#include "Misc/ScopeExit.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/TextRenderActor.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMirrorGeometryTest,"ToonStory.Mirror.Geometry",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMirrorGeometryTest::RunTest(const FString&)
{
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
	if (!TestNotNull(TEXT("Geometry world"),World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Mirror=World->SpawnActor<ATBMirror>();
	FVector Hit;
	TestTrue(TEXT("Symmetric reflection hits center"),Mirror->ReflectionPoint(FVector(200,-80,0),FVector(200,80,0),Hit) && Hit.IsNearlyZero());
	TestFalse(TEXT("Back of mirror never reflects"),Mirror->ReflectionPoint(FVector(-200,0,0),FVector(200,0,0),Hit));
	TestFalse(TEXT("Target behind mirror never reflects"),Mirror->ReflectionPoint(FVector(200,0,0),FVector(-200,0,0),Hit));
	TestFalse(TEXT("Ray outside the finite glass is rejected"),Mirror->ReflectionPoint(FVector(200,300,0),FVector(200,300,0),Hit));
	TestFalse(TEXT("Ray above the finite glass is rejected"),Mirror->ReflectionPoint(FVector(200,0,200),FVector(200,0,200),Hit));
	TestFalse(TEXT("View distance applies to geometry too"),Mirror->ReflectionPoint(FVector(100000,0,0),FVector(200,0,0),Hit));
	Mirror->SetActorTransform(FTransform(FRotator(15,60,20),FVector(700,200,300),FVector(1,2,.5f)));
	const auto Transform=Mirror->GetActorTransform();
	TestTrue(TEXT("Rotated scaled mirror preserves reflection"),Mirror->ReflectionPoint(Transform.TransformPosition(FVector(200,-80,0)),Transform.TransformPosition(FVector(200,80,0)),Hit) && Hit.Equals(Transform.GetLocation(),.01f));
	TestTrue(TEXT("Mirror has no interact actions"),Mirror->GetInteractionActions().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMirrorGazeTest,"ToonStory.Mirror.Gaze",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMirrorGazeTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game) World=Context.World();
	if(!World || World->GetNetMode()!=NM_Standalone) return false;
	auto* Mode=World->GetAuthGameMode<ATBGameMode>(); auto* State=World->GetGameState<ATBGameState>();
	auto* PC=Cast<ATBController>(World->GetFirstPlayerController()); auto* Toy=PC ? PC->GetPawn<ATBCharacter>() : nullptr;
	if(!Mode || !State || !Toy || !Toy->TBPS()) return false;
	TBTest::FPlayingScope Playing(*State,World->GetTimeSeconds()+600);
	const auto OldTeam=Toy->TBPS()->Team; const auto OldState=Toy->TBPS()->ToyState;
	const bool OldFrozen=Toy->TBPS()->bFrozen; const auto OldTransform=Toy->GetActorTransform();
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* HumanPC=World->SpawnActor<ATBController>();
	auto* Human=World->SpawnActor<ATBCharacter>(FVector(200,-80,10000),FRotator::ZeroRotator,Spawn);
	auto* HumanPS=HumanPC->GetPlayerState<ATBPlayerState>();
	if(!HumanPS) { HumanPS=World->SpawnActor<ATBPlayerState>(); HumanPC->PlayerState=HumanPS; State->AddPlayerState(HumanPS); }
	HumanPS->Team=ETBTeam::Human; HumanPC->Possess(Human); Human->ApplyState();
	Human->SetActorLocation(FVector(200,-80,10000)-FVector(0,0,Human->BaseEyeHeight));
	Toy->TBPS()->Team=ETBTeam::Toy; Toy->TBPS()->ToyState=ETBToyState::Free; Toy->TBPS()->bFrozen=false; Toy->ApplyState();
	Toy->SetActorLocation(FVector(200,80,10000));
	auto* Mirror=World->SpawnActor<ATBMirror>(FVector(0,0,10000),FRotator::ZeroRotator,Spawn);
	auto* Blocker=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Blocker);
	Blocker->SetRootComponent(Box); Box->SetBoxExtent(FVector(12,12,80)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
	auto* Room=World->SpawnActor<ATBRoomLighting>(Toy->GetActorLocation(),FRotator::ZeroRotator,Spawn);
	Room->Bounds->SetBoxExtent(FVector(30,30,30));
	ON_SCOPE_EXIT {
		Room->Destroy(); Blocker->Destroy(); Mirror->Destroy(); HumanPC->UnPossess(); Human->Destroy();
		State->RemovePlayerState(HumanPS); HumanPS->Destroy(); HumanPC->Destroy();
		Toy->TBPS()->Team=OldTeam; Toy->TBPS()->ToyState=OldState; Toy->TBPS()->bFrozen=OldFrozen; Toy->ApplyState(); Toy->SetActorTransform(OldTransform);
	};
	HumanPC->SetControlRotation((Mirror->GetActorLocation()-Human->GetPawnViewLocation()).Rotation());
	Blocker->SetActorLocation(FVector(200,0,10000));
	TestTrue(TEXT("Toy seen only through mirror"),Mode->Watched(Toy,Human));
	Mode->EvaluateGaze(); TestTrue(TEXT("Server freezes reflected toy"),Toy->TBPS()->bFrozen);
	Blocker->SetActorLocation(FVector(100,-40,10000));
	TestFalse(TEXT("Observer to mirror obstruction"),Mode->Watched(Toy,Human));
	Mode->EvaluateGaze(); TestFalse(TEXT("Looking away or obstruction thaws toy"),Toy->TBPS()->bFrozen);
	Blocker->SetActorLocation(FVector(100,40,10000));
	TestFalse(TEXT("Mirror to toy obstruction"),Mode->Watched(Toy,Human));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HumanPC->SetControlRotation(FRotator::ZeroRotator);
	TestFalse(TEXT("Mirror behind observer does not freeze"),Mode->Watched(Toy,Human));
	HumanPC->SetControlRotation((Mirror->GetActorLocation()-Human->GetPawnViewLocation()).Rotation());
	Mirror->SetActorHiddenInGame(true); TestFalse(TEXT("Hidden mirror cannot freeze"),Mode->Watched(Toy,Human)); Mirror->SetActorHiddenInGame(false);
	FTBInteractionContext Use; Use.InstigatorCharacter=Human; Use.ActionId=TEXT("Use");
	TestFalse(TEXT("Passive mirror never accepts interact"),Mirror->TryInteract(Use));
	auto* Data=NewObject<UTBMirrorData>(); Data->Width=500; Mirror->MirrorData=Data; Mirror->OnConstruction(Mirror->GetActorTransform());
	Human->SetActorLocation(FVector(200,-400,10000-Human->BaseEyeHeight));
	FVector Hit; Mirror->ReflectionPoint(Human->GetPawnViewLocation(),Toy->GetActorLocation(),Hit);
	HumanPC->SetControlRotation((Hit-Human->GetPawnViewLocation()).Rotation());
	Room->SetLightsOn(false); Mode->EvaluateGaze(); TestFalse(TEXT("Dark-room distance protection also applies through mirrors"),Toy->TBPS()->bFrozen);
	Room->SetLightsOn(true); Mode->EvaluateGaze(); TestTrue(TEXT("Lit toy freezes through same reflected path"),Toy->TBPS()->bFrozen);
	HumanPC->UnPossess();
	auto* AI=World->SpawnActor<ATBNPCController>(); AI->SetActorTickEnabled(false); AI->Possess(Human);
	auto* AIState=AI->GetPlayerState<ATBPlayerState>();
	if (TestNotNull(TEXT("NPC observer state"),AIState))
	{
		AIState->Team=ETBTeam::Human; Human->ApplyState();
		Human->SetActorLocation(FVector(200,-80,10000-Human->BaseEyeHeight));
		AI->SetControlRotation((Mirror->GetActorLocation()-Human->GetPawnViewLocation()).Rotation());
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Blocker->SetActorLocation(FVector(200,0,10000));
		TestTrue(TEXT("NPC can discover a reflected toy"),AI->CanSee(Toy));
		TestFalse(TEXT("Unnoticed reflected toy is not frozen by NPC"),Mode->Watched(Toy,Human));
		AI->SetFocus(Mirror); // Observe the mirror before the NPC's normal recognition step.
		AI->OverlookChance=0; AI->RecognitionSeconds=0; AI->Tick(.4f); AI->UpdateControlRotation(.016f);
		TestTrue(TEXT("NPC tracks the discovered reflected toy"),AI->IsTracking(Toy));
		TestTrue(TEXT("NPC looks at reflection and freezes toy"),Mode->Watched(Toy,Human));
		// 中心は鏡面の外、上半身だけが映る小さな鏡でも追跡を維持する。
		FVector BoundsOrigin,BoundsExtent; Toy->GetActorBounds(true,BoundsOrigin,BoundsExtent);
		const FVector Upper=BoundsOrigin+FVector(0,0,BoundsExtent.Z*.7f);
		Mirror->ReflectionPoint(Human->GetPawnViewLocation(),Upper,Hit);
		Data->Height=10; Data->FrameWidth=1; Mirror->SetActorLocation(Hit);
		Mirror->SetActorScale3D(FVector(1,1,.4f)); Mirror->OnConstruction(Mirror->GetActorTransform());
		TestFalse(TEXT("Center sample misses short mirror"),Mirror->ReflectionPoint(Human->GetPawnViewLocation(),BoundsOrigin,Hit));
		TestTrue(TEXT("Upper sample still reflects"),Mirror->ReflectionPoint(Human->GetPawnViewLocation(),Upper,Hit));
		AI->SetControlRotation((Hit-Human->GetPawnViewLocation()).Rotation());
		TestTrue(TEXT("NPC discovers upper-only reflection"),AI->CanSee(Toy));
		AI->SetFocus(Toy); AI->UpdateControlRotation(.016f);
		TestTrue(TEXT("NPC aims at accepted upper reflection"),AI->GetControlRotation().Vector().Equals((Hit-Human->GetPawnViewLocation()).GetSafeNormal(),.001f));
		TestTrue(TEXT("Upper reflection stays visible on next gaze check"),AI->CanSee(Toy));
		TestTrue(TEXT("Upper reflection keeps toy frozen"),Mode->Watched(Toy,Human));
	}
	AI->UnPossess(); HumanPC->Possess(Human); Human->ApplyState();
	if(AIState) { State->RemovePlayerState(AIState); AIState->Destroy(); } AI->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMirrorConeTest,"ToonStory.Mirror.ConeSelection",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMirrorConeTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game) World=Context.World();
	if(!World) return false;
	auto* Observer=World->SpawnActor<AActor>(); auto* Target=World->SpawnActor<AActor>();
	const FVector Eye(0,0,20000), Sample(0,100,20000);
	const FVector Outside(100,80,20080), Inside(100,0,20000);
	auto SpawnMirror=[&](FVector Point) {
		const FVector Normal=((Eye-Point).GetSafeNormal()+(Sample-Point).GetSafeNormal()).GetSafeNormal();
		return World->SpawnActor<ATBMirror>(Point,Normal.Rotation());
	};
	auto* First=SpawnMirror(Outside); auto* Second=SpawnMirror(Inside);
	ON_SCOPE_EXIT { First->Destroy(); Second->Destroy(); Observer->Destroy(); Target->Destroy(); };
	FVector Hit;
	TestTrue(TEXT("Square frustum accepts the first corner reflection"),ATBMirror::FindView(Observer,Target,Eye,FRotator::ZeroRotator,Sample,1,1,Hit) && Hit.Equals(Outside,.01f));
	TestTrue(TEXT("Circular cone skips first mirror and finds second"),ATBMirror::FindView(Observer,Target,Eye,FRotator::ZeroRotator,Sample,1,1,Hit,0,.70710678f) && Hit.Equals(Inside,.01f));
	Second->SetActorHiddenInGame(true);
	TestFalse(TEXT("Corner alone is outside circular cone"),ATBMirror::FindView(Observer,Target,Eye,FRotator::ZeroRotator,Sample,1,1,Hit,0,.70710678f));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMirrorVisualTest,"ToonStory.Mirror.Visual",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMirrorVisualTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(),TEXT("MirrorScreenshot"))) return true;
	UWorld* World=nullptr;
	for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game) World=Context.World();
	if (!World || World->GetNetMode()!=NM_Standalone) return false;
	auto* PC=Cast<ATBController>(World->GetFirstPlayerController()); auto* State=World->GetGameState<ATBGameState>(); auto* Mode=World->GetAuthGameMode();
	if (!PC || !State || !Mode) return false;
	auto* Player=PC->GetPlayerState<ATBPlayerState>(); if (!Player) return false;
	const auto OldTeam=Player->Team; const auto OldRound=State->HidePreparation.Round;
	auto Scope=MakeShared<TBTest::FPlayingScope>(*State,World->GetTimeSeconds()+600);
	Player->Team=ETBTeam::Human; ++State->HidePreparation.Round;
	PC->ClientPreparationRole(State->HidePreparation.Round,ETBTeam::Human);
	const bool ModeTick=Mode->IsActorTickEnabled(); Mode->SetActorTickEnabled(false);
	auto* PreviousView=PC->GetViewTarget();
	TArray<AActor*> Actors;
	const bool Authored=FParse::Param(FCommandLine::Get(),TEXT("MirrorHouseScreenshot"));
	ATBMirror* Mirror=nullptr;
	if (Authored) { for(TActorIterator<ATBMirror> It(World);It;++It) {Mirror=*It;break;} }
	else { Mirror=World->SpawnActor<ATBMirror>(FVector(0,0,10000),FRotator::ZeroRotator); Actors.Add(Mirror); }
	if (!TestNotNull(TEXT("Mirror to photograph"),Mirror)) { Mode->SetActorTickEnabled(ModeTick); Player->Team=OldTeam; return false; }
	const FVector CameraPosition=Authored ? FVector(260,-430,150) : FVector(220,15,10015);
	auto* Camera=World->SpawnActor<ACameraActor>(CameraPosition,(Mirror->GetActorLocation()-CameraPosition).Rotation()); Actors.Add(Camera);
	Camera->GetCameraComponent()->FieldOfView=65; Camera->GetCameraComponent()->AspectRatio=16.f/9;
	if (!Authored)
	{
	auto* Light=World->SpawnActor<APointLight>(FVector(150,0,10200),FRotator::ZeroRotator); Actors.Add(Light);
	Light->PointLightComponent->SetIntensity(4000); Light->PointLightComponent->SetAttenuationRadius(1000);
	for (int32 Index=0;Index<3;++Index)
	{
		auto* Prop=World->SpawnActor<AStaticMeshActor>(); Actors.Add(Prop);
		Prop->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Prop->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Index==1 ? TEXT("/Engine/BasicShapes/Sphere.Sphere") : TEXT("/Engine/BasicShapes/Cube.Cube")));
		Prop->SetActorLocation(Index==0 ? FVector(150,0,9914) : FVector(300,Index==1 ? -60 : 60,9990));
		Prop->SetActorScale3D(Index==0 ? FVector(10,8,.1f) : FVector(.5f));
	}
	auto* Label=World->SpawnActor<ATextRenderActor>(FVector(290,0,10050),FRotator(0,180,0)); Actors.Add(Label);
	Label->GetTextRender()->SetText(FText::FromString(TEXT("ABC 123"))); Label->GetTextRender()->SetWorldSize(24);
	Label->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
	}
	PC->SetViewTarget(Camera); PC->PlayerCameraManager->UpdateCamera(.016f);
	const double Started=World->GetTimeSeconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([=,this]() {
		Scope->Hold();
		PC->SetViewTarget(Camera); PC->PlayerCameraManager->UpdateCamera(.016f);
		if (World->GetTimeSeconds()<Started+3) return false;
		TestNotNull(TEXT("Mirror creates its local render target"),Mirror->ReflectionCamera->TextureTarget.Get());
		TestTrue(TEXT("Capture is on the reflected side"),FVector::DotProduct(Mirror->ReflectionCamera->GetComponentLocation()-Mirror->GetActorLocation(),Mirror->GetActorForwardVector())<0);
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/(Authored ? TEXT("MirrorPreview/house.png") : TEXT("MirrorPreview/mirror.png")),false,false);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([=]() { return World->GetTimeSeconds()>Started+5; }));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([=]() {
		Scope->Hold(); Player->Team=OldTeam; PC->ClientPreparationRole(OldRound,OldTeam);
		PC->SetViewTarget(PreviousView); for(auto* Actor:Actors) Actor->Destroy(); Mode->SetActorTickEnabled(ModeTick); return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBMirrorPlacementTest,"ToonStory.Mirror.Placement",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBMirrorPlacementTest::RunTest(const FString&)
{
	UWorld* World=nullptr;
	for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Game) World=Context.World();
	if (!World || !World->GetMapName().Contains(TEXT("TB_StylizedHouse"))) { AddInfo(TEXT("Placement requires TB_StylizedHouse")); return true; }
	int32 Count=0;
	for(TActorIterator<ATBMirror> It(World);It;++It)
	{
		++Count; TestNotNull(TEXT("Saved tuning asset"),It->MirrorData.LoadSynchronous());
		TestNotNull(TEXT("Reflective material"),It->GetData().MirrorMaterial.LoadSynchronous());
		TestTrue(TEXT("Saved mirror exposes no interaction"),It->GetInteractionActions().IsEmpty());
		FHitResult Floor; const FVector Front=It->GetActorLocation()+It->GetActorForwardVector()*150;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(MirrorPlacement));
		if(TestTrue(TEXT("Floor in front of authored mirror"),World->LineTraceSingleByChannel(Floor,Front,Front-FVector(0,0,250),ECC_Visibility,Query)))
		{
			const FVector Standing=Floor.ImpactPoint+FVector(0,0,TBCharacterGeometry::HumanHalfHeight+2);
			TestTrue(TEXT("Human can stand in front of mirror"),TB::SupportsCapsule(World,Standing,TBCharacterGeometry::HumanRadius,TBCharacterGeometry::HumanHalfHeight,ECC_Pawn,Query,.71f));
		}
	}
	TestEqual(TEXT("One authored hallway mirror"),Count,1);
	return true;
}
#endif
