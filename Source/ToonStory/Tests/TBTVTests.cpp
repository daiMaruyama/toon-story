// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Character/TBCharacter.h"
#include "Core/TBController.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/TBBoxTV.h"
#include "GamePlay/TBTVButton.h"
#include "GamePlay/TBTVCamera.h"
#include "GamePlay/TBTVRemote.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "Data/TBPresentationData.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTVTest, "ToonStory.Television.ChannelLifecycle",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTBTVTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Game && Context.World()->GetAuthGameMode<ATBGameMode>())
		{
			World = Context.World();
			break;
		}
	}
	if (!TestNotNull(TEXT("Run in a standalone game world"), World) ||
	    !TestEqual(TEXT("Isolated test requires standalone"), World->GetNetMode(), NM_Standalone))
	{
		return false;
	}
	auto* Mode = World->GetAuthGameMode<ATBGameMode>();
	auto* State = World->GetGameState<ATBGameState>();
	auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
	if (!TestNotNull(TEXT("Game state"), State) || !TestNotNull(TEXT("Host"), Host) ||
	    !TestNotNull(TEXT("Storage box"), State->Box.Get()))
	{
		return false;
	}
	// レベル・BPが設定する依存を、このテストの中だけで用意する。
	auto* FixtureMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Television/M_TBBoxTV.M_TBBoxTV"));
	if (!TestNotNull(TEXT("Screen material fixture"), FixtureMaterial))
	{
		return false;
	}
	auto* Overview = World->SpawnActor<ATBTVCamera>(); // 0番
	const FTransform Placement(State->Box->StorageRoom->GetComponentQuat(),
	                           State->Box->StorageRoom->GetComponentLocation() + FVector(0, 0, 100));
	auto* TV = World->SpawnActorDeferred<ATBBoxTV>(ATBBoxTV::StaticClass(), Placement, nullptr, nullptr,
	                                               ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TV || !Overview)
	{
		AddError(TEXT("Could not create television fixtures"));
		if (TV)
		{
			TV->Destroy();
		}
		if (Overview)
		{
			Overview->Destroy();
		}
		return false;
	}
	TV->ScreenMaterial = FixtureMaterial;
	TV->FinishSpawning(Placement);
	auto* ViewerController = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	auto* TargetController = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
	if (!ViewerController || !TargetController)
	{
		AddError(TEXT("Need three local players for this isolated test"));
		if (ViewerController)
		{
			UGameplayStatics::RemovePlayer(ViewerController, true);
		}
		if (TargetController)
		{
			UGameplayStatics::RemovePlayer(TargetController, true);
		}
		TV->Destroy();
		Overview->Destroy();
		return false;
	}
	const FTBSettings OriginalSettings = State->Settings;
	FTBSettings Rules;
	Rules.Humans = 1;
	Rules.Toys = 2;
	Rules.Duration = 600;
	Mode->SetRules(Host, Rules);
	auto* HumanInfo = Host->GetPlayerState<ATBPlayerState>();
	auto* ViewerInfo = ViewerController->GetPlayerState<ATBPlayerState>();
	auto* TargetInfo = TargetController->GetPlayerState<ATBPlayerState>();
	const ETBTeam OriginalPreference = HumanInfo->Preference;
	HumanInfo->Preference = ETBTeam::Human;
	ViewerInfo->Preference = TargetInfo->Preference = ETBTeam::Toy;
	HumanInfo->bReady = ViewerInfo->bReady = TargetInfo->bReady = true;
	Mode->StartRound(Host);
	TestEqual(TEXT("Three player match starts"), State->Phase, ETBPhase::Playing);
	auto* Viewer = ViewerController->GetPawn<ATBCharacter>();
	auto* Target = TargetController->GetPawn<ATBCharacter>();
	auto Cleanup = [Mode, State, Host, TargetController, ViewerController, OriginalSettings, HumanInfo,
	                OriginalPreference, TV, Overview]()
	{
		if (IsValid(Mode) && IsValid(State) && State->Phase == ETBPhase::Playing)
		{
			Mode->Finish(ETBWinner::Humans, TEXT("TV test cleanup"));
		}
		if (IsValid(Mode) && IsValid(State) && IsValid(Host)) Mode->ReturnToLobby(Host);
		if (IsValid(TargetController)) UGameplayStatics::RemovePlayer(TargetController, true);
		if (IsValid(ViewerController)) UGameplayStatics::RemovePlayer(ViewerController, true);
		if (IsValid(State)) State->Settings = OriginalSettings;
		if (IsValid(HumanInfo)) HumanInfo->Preference = OriginalPreference;
		if (IsValid(TV)) TV->Destroy();
		if (IsValid(Overview)) Overview->Destroy();
	};
	// Remote operation now requires the real server preparation release; Phase=Playing alone is insufficient.
	auto RunLifecycle = [=, this]() -> bool
	{
	if (Viewer && Target && State->Box)
	{
		// 視聴者が箱に入ると、番号は人間（Host）→残りのおもちゃ（Target）の順になる。
		const int32 HumanNumber = ATBBoxTV::CameraChannels;
		const int32 TargetNumber = ATBBoxTV::CameraChannels + 1;
		auto Noise = [TV]()
		{
			auto* Material = Cast<UMaterialInstanceDynamic>(TV->Screen->GetMaterial(0));
			return Material ? Material->K2_GetScalarParameterValue(TEXT("Noise")) : -1.f;
		};
		TV->Tick(0);
		TestFalse(TEXT("No capture when no local player is boxed"), TV->IsCapturing());
		TestFalse(TEXT("Free toy cannot change television"), TV->SelectChannel(Viewer, TargetNumber));
		TestTrue(TEXT("Viewer can be stored without ending match"), State->Box->Store(Viewer));
		TV->Tick(0);
		TestTrue(TEXT("Boxed local player starts capture scheduling"), TV->IsCapturing());
		TestEqual(TEXT("Initial channel is overview"), TV->GetChannel().Number, 0);
		TestFalse(TEXT("Human cannot change television"),
		          TV->SelectChannel(Host->GetPawn<ATBCharacter>(), TargetNumber));
		TestFalse(TEXT("Out of range digit is rejected"), TV->SelectChannel(Viewer, ATBBoxTV::MaxChannel + 1));
		TestTrue(TEXT("Digit selects human first"),
		         TV->SelectChannel(Viewer, HumanNumber) && TV->GetChannel().Target == HumanInfo);
		TestTrue(TEXT("Digit selects remaining toy after humans"),
		         TV->SelectChannel(Viewer, TargetNumber) && TV->GetChannel().Target == TargetInfo);
		// 箱の中の自分は番号を持たないので、映る番号のうち一番大きいのは残りのおもちゃ。
		TestTrue(TEXT("Next wraps to camera 0"), TV->StepChannel(Viewer, 1) && TV->GetChannel().Number == 0);
		TestTrue(TEXT("Prev wraps to last watchable"),
		         TV->StepChannel(Viewer, -1) && TV->GetChannel().Number == TargetNumber);
		TestTrue(TEXT("Prev visits human channel"), TV->StepChannel(Viewer, -1) && TV->GetChannel().Number == HumanNumber);
		TestTrue(TEXT("Digit can tune a channel without picture"),
		         TV->SelectChannel(Viewer, ATBBoxTV::MaxChannel) && !TV->GetChannel().Target);
		TestTrue(TEXT("Power can be turned off"), TV->TogglePower(Viewer) && !TV->IsPowerOn());
		TV->Tick(0);
		TestFalse(TEXT("Powered off television does not capture"), TV->IsCapturing());
		TestFalse(TEXT("Channel buttons do nothing while off"), TV->SelectChannel(Viewer, 0));
		TestFalse(TEXT("Human cannot toggle power"), TV->TogglePower(Host->GetPawn<ATBCharacter>()));
		TestTrue(TEXT("Power can be turned on"), TV->TogglePower(Viewer) && TV->IsPowerOn());
		TV->Tick(0);
		TestTrue(TEXT("Powered on television captures again"), TV->IsCapturing());
		// レベルに置いたリモコンで、先客がいるボタンは押されないことを確かめる。
		TActorIterator<ATBTVRemote> RemoteIt(World);
		if (RemoteIt && IsValid(RemoteIt->TV))
		{
			auto* Remote = *RemoteIt;
			TArray<UTBTVButton*> Buttons;
			Remote->GetComponents(Buttons);
			auto FindDigit = [&Buttons](int32 Digit) -> UTBTVButton*
			{
				auto* const* Found = Buttons.FindByPredicate([Digit](const UTBTVButton* Button)
				                                             { return Button->CurrentKey == ETBRemoteKey::Digit && Button->Digit == Digit; });
				return Found ? *Found : nullptr;
			};
			auto* One = FindDigit(1);
			auto* Two = FindDigit(2);
			if (TestNotNull(TEXT("Remote has digit 1"), One) && TestNotNull(TEXT("Remote has digit 2"), Two))
			{
				auto Put = [](ATBCharacter* Character, const FVector& Location)
				{
					Character->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
					Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
					Character->GetCharacterMovement()->Velocity = FVector::ZeroVector;
					Character->GetCapsuleComponent()->UpdateOverlaps();
				};
				auto SupportedPosition = [World, Remote, this](UTBTVButton* Button, ATBCharacter* Body, FVector& Position)
				{
					const FVector Center = Button->GetComponentLocation();
					FHitResult Hit;
					FCollisionQueryParams Query(SCENE_QUERY_STAT(TVRemoteFixture), false, Body);
					const bool Supported = World->LineTraceSingleByChannel(Hit, Center + FVector(0, 0, 20), Center - FVector(0, 0, 100),
					    Body->GetCapsuleComponent()->GetCollisionObjectType(), Query) && Hit.GetComponent() == Remote->Mesh;
					TestTrue(TEXT("Authored remote offers real mesh support beneath its key"), Supported);
					Position = Hit.ImpactPoint + FVector(0, 0, Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2);
					return Supported;
				};
				const FVector ViewerHome = Viewer->GetActorLocation();
				const FVector TargetHome = Target->GetActorLocation();
				const int32 Before = Remote->TV->GetChannel().Number;
				FVector OnePosition, TwoPosition;
				if (!SupportedPosition(One, Viewer, OnePosition) || !SupportedPosition(Two, Viewer, TwoPosition))
				{ Cleanup(); return false; }
				double RemoteNow = World->GetTimeSeconds();
				auto Observe = [Remote, &RemoteNow](int32 Count)
				{
					for (int32 Index = 0; Index < Count; ++Index) Remote->EvaluateContacts(RemoteNow += .05);
				};
				Put(Target, OnePosition);
				Put(Viewer, OnePosition);
				Observe(8);
				TestEqual(TEXT("Occupied button is not pressed again"), Remote->TV->GetChannel().Number, Before);
				Put(Target, TargetHome);
				Put(Viewer, ViewerHome);
				Observe(4);
				Put(Viewer, TwoPosition);
				Observe(8);
				TestEqual(TEXT("Stepping on an empty button presses it"), Remote->TV->GetChannel().Number, 2);
				Put(Viewer, ViewerHome);
			}
		}
		else
		{
			AddInfo(TEXT("No remote in this level; skipped remote button check"));
		}
		TV->SelectChannel(Viewer, 0);
		TestEqual(TEXT("Switching starts static"), Noise(), 1.f);
		// 実フレームでタイマーを動かす。同期テスト内のWorld::Tick連打ではタイマーは進まない。
		const double Deadline = FPlatformTime::Seconds() + 15;
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
		    [this, World, State, TV, Viewer, Target, TargetController, TargetNumber, Mode, Noise, Deadline, Cleanup,
		     WaitFrom = World->GetTimeSeconds(), Phase = 0]() mutable
		    {
			    if (World->GetTimeSeconds() - WaitFrom < .75 && FPlatformTime::Seconds() < Deadline)
			    {
				    return false;
			    }
			    WaitFrom = World->GetTimeSeconds();
			    switch (Phase++)
			    {
			    case 0:
				    TestEqual(TEXT("Camera channel replaces static"), Noise(), 0.f);
				    TV->SelectChannel(Viewer, TargetNumber);
				    return false;
			    case 1:
				    TestEqual(TEXT("Live target replaces static"), Noise(), 0.f);
				    Target->SetToyState(ETBToyState::Grabbed);
				    return false;
			    case 2:
				    TestEqual(TEXT("Grabbed target shows static"), Noise(), 1.f);
				    TestEqual(TEXT("Channel stays on grabbed target"), TV->GetChannel().Number, TargetNumber);
				    Target->SetToyState(ETBToyState::Free);
				    return false;
			    case 3:
				    TestEqual(TEXT("Released target returns on air"), Noise(), 0.f);
				    // 映す体が無くなったら砂嵐。両者を箱に入れると試合が終わるので、同じ判定を通るPawn喪失で確かめる。
				    TargetController->UnPossess();
				    return false;
			    case 4:
				    TestEqual(TEXT("Lost target shows static"), Noise(), 1.f);
				    TestEqual(TEXT("Channel stays on lost target"), TV->GetChannel().Number, TargetNumber);
				    TargetController->Possess(Target);
				    TV->SelectChannel(Viewer, ATBBoxTV::MaxChannel);
				    return false;
			    default:
				    TestEqual(TEXT("Empty channel shows static"), Noise(), 1.f);
				    State->Box->ReleasePrisoners();
				    Mode->CheckWin(); // 箱の中の人数はゲームモードの更新で反映される。
				    TV->Tick(0);
				    TestFalse(TEXT("Rescue stops capturing"), TV->IsCapturing());
				    TestEqual(TEXT("Empty box resets channel to camera 0"), TV->GetChannel().Number, 0);
				    TestTrue(TEXT("Empty box resets target"), !TV->GetChannel().Target);
				    // 箱の中の人が電源を切ったまま救助されても、次に入った人は電源ONから見る。
				    if (TestTrue(TEXT("Viewer can be stored again"), State->Box->Store(Viewer)))
				    {
					    TestTrue(TEXT("Boxed viewer turns power off"), TV->TogglePower(Viewer) && !TV->IsPowerOn());
					    State->Box->ReleasePrisoners();
					    Mode->CheckWin();
					    TV->Tick(0);
					    TestTrue(TEXT("Empty box turns power back on"), TV->IsPowerOn());
				    }
				    Mode->Finish(ETBWinner::Humans, TEXT("TV test cleanup"));
				    TV->Tick(0);
				    TestFalse(TEXT("Results do not capture"), TV->IsCapturing());
				    Cleanup();
				    return true;
			    }
		    }));
		return true;
	}
	else
	{
		AddError(TEXT("Match requires characters and storage box"));
	}
	Cleanup();
	return !HasAnyErrors();
	};
	const double PreparationDeadline = FPlatformTime::Seconds() + 20;
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, Viewer, Target, ViewerController, TargetController, RunLifecycle, Cleanup, PreparationDeadline]()
	{
		if (!TestTrue(TEXT("Preparation wait retains valid possessed participants"), IsValid(State) && IsValid(Viewer) && IsValid(Target) &&
		    IsValid(ViewerController) && IsValid(TargetController) && ViewerController->GetPawn() == Viewer && TargetController->GetPawn() == Target))
		{ Cleanup(); return true; }
		if (State->Phase == ETBPhase::Playing && State->IsPreparing() && FPlatformTime::Seconds() < PreparationDeadline) return false;
		if (!TestTrue(TEXT("Normal server preparation release precedes television interaction"),
		    State->Phase == ETBPhase::Playing && State->HidePreparation.bValid && !State->IsPreparing() && State->Remaining() > 0))
		{ Cleanup(); return true; }
		RunLifecycle();
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTVRemoteTest, "ToonStory.Television.RemoteSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTBTVRemoteTest::RunTest(const FString&)
{
	auto* AuthoredClass = LoadClass<ATBTVRemote>(nullptr, TEXT("/Game/Television/BP_TBTVRemote.BP_TBTVRemote_C"));
	if (!TestNotNull(TEXT("Authored remote class"), AuthoredClass)) return false;
	TestTrue(TEXT("Authored BP inherits replicated remote behavior"), AuthoredClass->GetDefaultObject<ATBTVRemote>()->GetIsReplicated());
	// Native geometry/real overlaps and traces; the match state, Walking mode and observation clock are fixture seams.
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).EnableTraceCollision(true);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Native remote world"), World)) return false;
	ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); if (World->IsRooted()) World->RemoveFromRoot(); };
	World->InitializeActorsForPlay(FURL());
	auto* State = World->SpawnActor<ATBGameState>();
	if (!TestNotNull(TEXT("Remote fixture state"), State)) return false;
	World->SetGameState(State);
	State->Phase = ETBPhase::Playing; State->HidePreparation.bValid = true; State->HidePreparation.Round = 1;
	State->EndTime = World->GetTimeSeconds() + 600;
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto MakeViewer = [&]() -> ATBCharacter*
	{
		auto* Body = World->SpawnActor<ATBCharacter>(FVector(-2000, 0, 100), FRotator::ZeroRotator, Spawn);
		auto* Controller = World->SpawnActor<AAIController>();
		auto* Player = World->SpawnActor<ATBPlayerState>();
		if (!Body || !Controller || !Player) return nullptr;
		Controller->SetPlayerState(Player); Player->SetOwner(Controller); Controller->Possess(Body); State->AddPlayerState(Player);
		Player->Team = ETBTeam::Toy; Player->ToyState = ETBToyState::Boxed; Body->ApplyState();
		return Body;
	};
	auto* Viewer = MakeViewer(); auto* Other = MakeViewer();
	auto* TV = World->SpawnActor<ATBBoxTV>();
	auto* CameraZero = World->SpawnActor<ATBTVCamera>();
	auto* CameraTwo = World->SpawnActor<ATBTVCamera>();
	if (!TestNotNull(TEXT("Watchable camera zero"), CameraZero) || !TestNotNull(TEXT("Watchable camera two"), CameraTwo)) return false;
	CameraZero->Channel = 0; CameraTwo->Channel = 2;
	const FTransform Placement(FRotator(0, 37, -8), FVector(1000, 0, 0), FVector(3));
	auto* Remote = World->SpawnActorDeferred<ATBTVRemote>(ATBTVRemote::StaticClass(), Placement, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Viewer"), Viewer) || !TestNotNull(TEXT("Other viewer"), Other) ||
	    !TestNotNull(TEXT("TV"), TV) || !TestNotNull(TEXT("Remote"), Remote) || !TestNotNull(TEXT("Native support cube"), Cube)) return false;
	Remote->Mesh->SetStaticMesh(Cube); Remote->Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Remote->Mesh->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Television/M_RemoteGlow.M_RemoteGlow")));
	Remote->TV = TV;
	TArray<UTBTVButton*> Keys;
	auto AddButton = [&](ETBRemoteKey Key, int32 Digit, FVector2D XY, FVector Extent)
	{
		auto* Button = NewObject<UTBTVButton>(Remote); Remote->AddInstanceComponent(Button);
		Button->SetupAttachment(Remote->Mesh); Button->SetRelativeLocation(FVector(XY.X, XY.Y, 50.5));
		Button->SetBoxExtent(Extent); Button->SetGenerateOverlapEvents(true); Button->CurrentKey = Key; Button->Digit = Digit;
		Button->RegisterComponent(); Keys.Add(Button); return Button;
	};
	// Same local XY spacing/extents as the authored 13-key remote, translated up to a native cube's support surface.
	for (int32 Digit = 1; Digit <= 9; ++Digit)
		AddButton(ETBRemoteKey::Digit, Digit, FVector2D(Digit % 3 == 1 ? 1.525 : Digit % 3 == 2 ? 0 : -1.49,
		    7.8825 - ((Digit - 1) / 3) * .96), FVector(.65, .3, .6));
	auto* Zero = AddButton(ETBRemoteKey::Digit, 0, FVector2D(0, 5.003), FVector(.65, .3, .6));
	auto* Power = AddButton(ETBRemoteKey::Power, 0, FVector2D(1.477, 8.792), FVector(.5, .3, .6));
	auto* Next = AddButton(ETBRemoteKey::Next, 0, FVector2D(1.505, -1.3325), FVector(.39, .3, .6));
	auto* Prev = AddButton(ETBRemoteKey::Prev, 0, FVector2D(1.505, -2.255), FVector(.39, .3, .6));
	Remote->FinishSpawning(Placement);
	World->GetWorldSettings()->NotifyBeginPlay();
	TestTrue(TEXT("World and overlap participants are initialized"), World->AreActorsInitialized() && Viewer->IsActorInitialized() && Remote->HasActorBegunPlay());
	TestEqual(TEXT("All thirteen visible keys retained"), Remote->Buttons.Num(), 13);
	double Now = World->GetTimeSeconds();
	const double Cooldown = TB::PresentationData().Remote.PressCooldown;
	if (!TestTrue(TEXT("Configured remote cooldown is finite and nonnegative"), FMath::IsFinite(Cooldown) && Cooldown >= 0 && Cooldown < 60)) return false;
	const int32 CooldownSteps = FMath::CeilToInt(Cooldown / .05) + 8;
	const int32 BeforeCooldownSteps = FMath::Max(1, FMath::FloorToInt(Cooldown / .05) - 1);
	auto Observe = [&](int32 Count = 8)
	{
		for (int32 Index = 0; Index < Count; ++Index) Remote->EvaluateContacts(Now += .05);
	};
	auto Move = [&](ATBCharacter* Body, FVector Position)
	{
		const bool Moved = Body->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
		Body->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Body->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		Body->GetCapsuleComponent()->UpdateOverlaps();
		for (auto* Key : Keys) Key->UpdateOverlaps();
		return TestTrue(TEXT("Fixture reaches exact requested position"), Moved && Body->GetActorLocation().Equals(Position, .01));
	};
	auto PositionFor = [&](UTBTVButton* Key)
	{
		FVector Local = Key->GetRelativeLocation(); Local.Z = 50;
		return Remote->Mesh->GetComponentTransform().TransformPosition(Local) + FVector(0, 0, Viewer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2);
	};
	auto Leave = [&]() { Move(Viewer, FVector(-2000, 0, 100)); Observe(CooldownSteps); };
	auto* One = Keys[0]; auto* Two = Keys[1];
	Move(Viewer, PositionFor(Two));
	if (!TestTrue(TEXT("Real small-key overlap includes actual capsule"), Two->IsOverlappingComponent(Viewer->GetCapsuleComponent()))) return false;
	FVector Supported;
	if (!TestTrue(TEXT("First blocking support hit is the tilted remote mesh"), Remote->SupportPoint(Viewer, Supported))) return false;
	TestEqual(TEXT("Multiple real overlaps select one projected key"), Remote->SelectButton(Viewer, Supported), Two);
	ATBCharacter* PreviewViewer = nullptr;
	TestEqual(TEXT("Stationary valid contact previews exactly one key before dispatch"), Remote->PreviewButton(PreviewViewer), Two);
	TestTrue(TEXT("Preview identifies the actual possessed viewer"), PreviewViewer == Viewer);
	Observe(3);
	TestEqual(TEXT("Short contact does not press"), TV->GetChannel().Number, 0);
	Observe();
	TestEqual(TEXT("Stationary dwell selects digit two only"), TV->GetChannel().Number, 2);
	TestTrue(TEXT("Remote replicates authoritative press state"), Remote->GetIsReplicated());
	TestEqual(TEXT("Accepted contact publishes the confirmed button"), Remote->ConfirmedButton(), static_cast<const UTBTVButton*>(Two));
	const auto AcceptedPress = Remote->PressState;
	auto* PressController = Viewer->GetController(); auto* PressPlayer = Viewer->TBPS();
	PressController->UnPossess(); Viewer->SetPlayerState(PressPlayer);
	Remote->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("No remote controller still forbids authority dispatch"), Remote->IsEligible(Viewer));
	TestEqual(TEXT("Remote player's controller is unnecessary for preview"), Remote->PreviewButton(PreviewViewer), Two);
	Viewer->GetCharacterMovement()->Velocity = FVector(20, 0, 0);
	TestNull(TEXT("Stale client motion suppresses only preview"), Remote->PreviewButton(PreviewViewer));
	Remote->PressState = AcceptedPress; Remote->OnRep_PressState();
	if (TestNotNull(TEXT("Authored glow material is available"), Remote->GlowMaterial.Get()))
	{
		TestEqual(TEXT("Confirmed client glow survives absent controller and stale velocity"),
		    Remote->GlowMaterial->K2_GetScalarParameterValue(Two->GetGlowParameterName()), 2.f);
		++State->HidePreparation.Round; Remote->UpdateGlow();
		TestEqual(TEXT("Previous round press cannot light a new round"),
		    Remote->GlowMaterial->K2_GetScalarParameterValue(Two->GetGlowParameterName()), 1.f);
		--State->HidePreparation.Round;
		Remote->PressState.Button = NAME_None; Remote->OnRep_PressState();
		TestEqual(TEXT("Replicated release clears the pressed appearance"),
		    Remote->GlowMaterial->K2_GetScalarParameterValue(Two->GetGlowParameterName()), 1.f);
	}
	Remote->PressState = AcceptedPress;
	Remote->SetRole(ROLE_Authority); PressController->Possess(Viewer);
	Viewer->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	const double FirstPress = Remote->LastPressTime;
	Observe(20);
	TestEqual(TEXT("Held contact is latched rather than repeated"), Remote->LastPressTime, FirstPress);
	Move(Viewer, FVector(-2000, 0, 100)); Observe(1);
	Now += .5; Remote->EvaluateContacts(Now);
	Move(Viewer, PositionFor(Two)); Observe();
	TestEqual(TEXT("Unobserved departure time cannot rearm a successful latch"), Remote->LastPressTime, FirstPress);
	// Ambiguous Voronoi boundary, unlike departure, must not rearm the old successful press.
	Move(Viewer, (PositionFor(One) + PositionFor(Two)) * .5);
	Observe();
	Move(Viewer, PositionFor(Two)); Observe();
	TestEqual(TEXT("Adjacent boundary jitter does not repeat a latched key"), Remote->LastPressTime, FirstPress);
	Leave();
	TestTrue(TEXT("Departure clears server confirmation"), Remote->PressState.Button.IsNone());
	Move(Viewer, PositionFor(Two)); Observe();
	TestTrue(TEXT("Clear departure rearms a new dwell"), Remote->LastPressTime > FirstPress);
	Leave();
	TV->SelectChannel(Viewer, 0);
	Move(Viewer, (PositionFor(One) + PositionFor(Two)) * .5); Observe();
	TestEqual(TEXT("Boundary projection is ambiguous, not a neighbouring press"), TV->GetChannel().Number, 0);
	TestNull(TEXT("Ambiguous projection has no selection preview"), Remote->PreviewButton(PreviewViewer));
	Move(Viewer, PositionFor(Two));
	Viewer->GetCharacterMovement()->Velocity = FVector(3, 0, 0); Observe();
	TestEqual(TEXT("Moving toy cannot dwell"), TV->GetChannel().Number, 0);
	TestNull(TEXT("Moving toy has no stationary selection preview"), Remote->PreviewButton(PreviewViewer));
	Viewer->GetCharacterMovement()->Velocity = FVector::ZeroVector; Observe(2);
	Leave(); Move(Viewer, PositionFor(Two)); Observe(3);
	TestEqual(TEXT("Departure cancels an incomplete dwell"), TV->GetChannel().Number, 0);
	Move(Viewer, PositionFor(Two) + Remote->Mesh->GetComponentTransform().GetUnitAxis(EAxis::X) * .6);
	Observe(3);
	TestEqual(TEXT("Projection drift restarts dwell instead of using the old anchor"), TV->GetChannel().Number, 0);
	Leave(); Move(Viewer, PositionFor(Two)); Observe(3);
	Move(Other, PositionFor(Prev));
	FVector OtherSupport;
	TestTrue(TEXT("Both competing pawns have actual independent mesh support"), Remote->SupportPoint(Viewer, Supported) && Remote->SupportPoint(Other, OtherSupport));
	Observe();
	TestEqual(TEXT("Two supported pawns defer even on different keys"), TV->GetChannel().Number, 0);
	TestNull(TEXT("Competing supported pawns have no selection preview"), Remote->PreviewButton(PreviewViewer));
	Move(Other, FVector(-2200, 0, 100)); Observe();
	TestEqual(TEXT("Unique supported pawn starts a fresh dwell"), TV->GetChannel().Number, 2);
	Leave(); TV->SelectChannel(Viewer, 0);
	Move(Viewer, PositionFor(One)); Observe(3);
	Viewer->TBPS()->ToyState = ETBToyState::Free; Observe();
	TestEqual(TEXT("Rescue invalidates a pending candidate"), TV->GetChannel().Number, 0);
	Viewer->TBPS()->ToyState = ETBToyState::Boxed;
	Leave(); Move(Viewer, PositionFor(One) + FVector(0, 0, 6)); Observe();
	TestFalse(TEXT("Being above a key is not actual nearby mesh support"), Remote->SupportPoint(Viewer, Supported));
	TestEqual(TEXT("Unsupported contact cannot press"), TV->GetChannel().Number, 0);
	Leave(); Move(Viewer, PositionFor(One)); Observe(3);
	Now += .5; Remote->EvaluateContacts(Now);
	TestEqual(TEXT("Missed observations cannot complete an old dwell"), TV->GetChannel().Number, 0);
	Observe(2);
	TestEqual(TEXT("Gap requires a new full dwell"), TV->GetChannel().Number, 0);
	Observe(); TestEqual(TEXT("New dwell after gap succeeds"), TV->GetChannel().Number, 1);
	Leave(); TV->SelectChannel(Viewer, 0); Move(Viewer, PositionFor(Two));
	Remote->LastPressTime = Now; Observe(BeforeCooldownSteps);
	TestEqual(TEXT("Cooldown defers a valid new contact"), TV->GetChannel().Number, 0);
	Leave(); Observe(CooldownSteps);
	TestEqual(TEXT("Cooldown expiry never fires a departed candidate"), TV->GetChannel().Number, 0);
	Move(Viewer, PositionFor(Two)); const double CooldownStart = Now; Remote->LastPressTime = Now; Observe(BeforeCooldownSteps);
	TestEqual(TEXT("Cooldown waiting does not extend last success"), Remote->LastPressTime, CooldownStart);
	Observe(CooldownSteps); TestEqual(TEXT("Continuously revalidated cooldown candidate eventually presses"), TV->GetChannel().Number, 2);
	Leave(); TV->SelectChannel(Viewer, 0); Move(Viewer, PositionFor(One)); Observe(3);
	auto* Controller = Viewer->GetController(); Controller->UnPossess(); Observe();
	TestEqual(TEXT("Lost possession cancels dwell"), TV->GetChannel().Number, 0);
	Controller->Possess(Viewer);
	Leave(); Move(Viewer, PositionFor(One)); Observe(3);
	State->EndTime = World->GetTimeSeconds() - 1; Observe();
	TestEqual(TEXT("Expired match never dispatches pending contact"), TV->GetChannel().Number, 0);
	State->EndTime = World->GetTimeSeconds() + 600;
	Leave(); Move(Viewer, PositionFor(One)); Observe(3);
	State->Phase = ETBPhase::Results; Observe();
	TestTrue(TEXT("Match ending clears pending candidates and latches"), Remote->Contacts.IsEmpty());
	TestNull(TEXT("Results has no interaction preview"), Remote->PreviewButton(PreviewViewer));
	State->Phase = ETBPhase::Playing;
	Leave(); Move(Viewer, PositionFor(One)); Observe(3);
	State->HidePreparation.Round++; Observe(2);
	TestEqual(TEXT("New round discards previous dwell"), TV->GetChannel().Number, 0);
	Leave(); Move(Viewer, PositionFor(One));
	Remote->SetRole(ROLE_SimulatedProxy); Observe();
	TestEqual(TEXT("Client evaluation cannot operate the television"), TV->GetChannel().Number, 0);
	TestEqual(TEXT("Client can show the same valid selection as preview only"), Remote->PreviewButton(PreviewViewer), One);
	Remote->SetRole(ROLE_Authority); Leave();
	// Every existing key still uses its original server TV operation; failed/off keys cannot attract selection to Power.
	for (int32 Digit = 1; Digit <= 9; ++Digit)
	{
		Move(Viewer, PositionFor(Keys[Digit - 1])); Observe(CooldownSteps);
		TestEqual(FString::Printf(TEXT("Digit %d keeps its exact value"), Digit), TV->GetChannel().Number, Digit); Leave();
	}
	Move(Viewer, PositionFor(Zero)); Observe(CooldownSteps); TestEqual(TEXT("Zero preserved"), TV->GetChannel().Number, 0); Leave();
	Move(Viewer, PositionFor(Prev)); Observe(CooldownSteps); TestEqual(TEXT("Prev wraps with existing TV rules"), TV->GetChannel().Number, 2); Leave();
	Move(Viewer, PositionFor(Next)); Observe(CooldownSteps); TestEqual(TEXT("Next wraps with existing TV rules"), TV->GetChannel().Number, 0); Leave();
	Move(Viewer, PositionFor(Power)); Observe(CooldownSteps); TestFalse(TEXT("Power key turns TV off"), TV->IsPowerOn()); Leave();
	Move(Viewer, PositionFor(One)); Observe(CooldownSteps); TestFalse(TEXT("Off digit never snaps to neighbouring Power"), TV->IsPowerOn()); Leave();
	Move(Viewer, PositionFor(Power)); Observe(CooldownSteps); TestTrue(TEXT("Power key remains usable while off"), TV->IsPowerOn());
	return !HasAnyErrors();
}
#endif
