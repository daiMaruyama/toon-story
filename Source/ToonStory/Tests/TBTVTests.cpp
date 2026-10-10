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
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	TV->SourceBox = State->Box;
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
		if (State->Phase == ETBPhase::Playing)
		{
			Mode->Finish(ETBWinner::Humans, TEXT("TV test cleanup"));
		}
		Mode->ReturnToLobby(Host);
		UGameplayStatics::RemovePlayer(TargetController, true);
		UGameplayStatics::RemovePlayer(ViewerController, true);
		State->Settings = OriginalSettings;
		HumanInfo->Preference = OriginalPreference;
		TV->Destroy();
		Overview->Destroy();
	};
	if (Viewer && Target && TV->SourceBox)
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
		TestTrue(TEXT("Viewer can be stored without ending match"), TV->SourceBox->Store(Viewer));
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
				{ Character->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics); };
				const FVector ViewerHome = Viewer->GetActorLocation();
				const FVector TargetHome = Target->GetActorLocation();
				const int32 Before = Remote->TV->GetChannel().Number;
				Put(Target, One->GetComponentLocation());
				Put(Viewer, One->GetComponentLocation());
				TestEqual(TEXT("Occupied button is not pressed again"), Remote->TV->GetChannel().Number, Before);
				Put(Target, TargetHome);
				Put(Viewer, ViewerHome);
				Put(Viewer, Two->GetComponentLocation());
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
		    [this, World, TV, Viewer, Target, TargetController, TargetNumber, Mode, Noise, Deadline, Cleanup,
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
				    TV->SourceBox->ReleasePrisoners();
				    TV->Tick(0);
				    TestFalse(TEXT("Rescue stops capturing"), TV->IsCapturing());
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
}
#endif
