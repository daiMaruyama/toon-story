#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/TBTitleGameMode.h"
#include "Core/TBSession.h"
#include "Core/TBGameMode.h"
#include "Core/TBGameState.h"
#include "Core/TBPlayerState.h"
#include "Core/TBController.h"
#include "UI/TBTitleMenu.h"
#include "UI/TBHUD.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineSessionInterface.h"

namespace
{
	bool HasSession(UWorld* World)
	{
		const auto* Online = Online::GetSubsystem(World);
		const auto Sessions = Online ? Online->GetSessionInterface() : nullptr;
		return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
	}

	// Exercises UI event bindings and a real timed match. Two local players do not
	// substitute for the separate-process/PIE network acceptance test.
	class FTitleFlow : public IAutomationLatentCommand
	{
	public:
		explicit FTitleFlow(FAutomationTestBase* InTest, UGameInstance* Instance)
		    : Test(InTest), GI(Instance), Deadline(FPlatformTime::Seconds() + 180)
		{
		}

		bool Update() override
		{
			if (!GI.IsValid())
			{
				Test->AddError(TEXT("Game instance lost during travel"));
				return true;
			}
			if (FPlatformTime::Seconds() > Deadline)
			{
				Test->AddError(FString::Printf(TEXT("Title flow timed out at step %d"), Step));
				return true;
			}
			UWorld* World = GI->GetWorld();
			if (!World || !World->HasBegunPlay())
			{
				return false;
			}
			auto* Session = GI->GetSubsystem<UTBSession>();
			auto* State = World->GetGameState<ATBGameState>();
			auto* Host = Cast<ATBController>(World->GetFirstPlayerController());
			auto* HUD = Host ? Cast<ATBHUD>(Host->GetHUD()) : nullptr;

			switch (Step)
			{
				case 0:
					if (!CheckTitle(World) || !ClickTitle(TEXT("Find")))
					{
						return true;
					}
					++Step;
					break;
				case 1:
					if (Session->IsBusy())
					{
						return false;
					}
					if (!CheckTitle(World))
					{
						return true;
					}
					{
						auto* Menu = GetTitle();
						auto* Rooms = Cast<UComboBoxString>(Menu->WidgetTree->FindWidget(TEXT("Rooms")));
						auto* Join = Cast<UButton>(Menu->WidgetTree->FindWidget(TEXT("Join")));
						if (!Rooms || !Join)
						{
							Test->AddError(TEXT("Room selection UI missing"));
							return true;
						}
						Test->TestEqual(TEXT("Room list reflects session results"), Rooms->GetOptionCount(),
						                Session->Rooms.Num());
						Test->TestEqual(TEXT("Join requires a selected result"), Join->GetIsEnabled(),
						                !Session->Rooms.IsEmpty());
					}
					if (!ClickTitle(TEXT("Host")))
					{
						return true;
					}
					++Step;
					break;
				case 2:
					if (!World->GetAuthGameMode<ATBGameMode>() || Session->IsBusy())
					{
						return false;
					}
					if (!Host || !HUD)
					{
						Test->AddError(TEXT("Stage controller/HUD missing"));
						return true;
					}
					Test->TestNull(TEXT("Title UI removed on entering the stage"), GetTitle());
					Test->TestTrue(TEXT("Hosted session survives Title -> stage"), HasSession(World));
					Second = Cast<ATBController>(UGameplayStatics::CreatePlayer(World, -1, true));
					if (!Second.IsValid())
					{
						Test->AddError(TEXT("Second local controller missing"));
						return true;
					}
					RoundWorld = World;
					Host->TBRules(1, 1, 10.f, 5);
					HUD->NotifyHitBoxClick(TEXT("Ready"));
					Second->TBReady();
					HUD->NotifyHitBoxClick(TEXT("Start"));
					if (!Test->TestEqual(TEXT("Start action begins match"), State->Phase, ETBPhase::Playing))
					{
						return true;
					}
					Host->ToggleLeaveMenu();
					++Step;
					break;
				case 3:
					if (!Host || !HUD)
					{
						Test->AddError(TEXT("Host lost during match"));
						return true;
					}
					Test->TestTrue(TEXT("Leave menu opens during play"), Host->IsLeaveMenuOpen());
					Test->TestTrue(TEXT("Menu blocks movement and look"),
					               Host->IsMoveInputIgnored() && Host->IsLookInputIgnored());
					Test->TestFalse(TEXT("Menu does not pause the match"), UGameplayStatics::IsGamePaused(World));
					HUD->NotifyHitBoxClick(TEXT("Resume"));
					++Step;
					break;
				case 4:
					if (!Host)
					{
						Test->AddError(TEXT("Host lost after resume"));
						return true;
					}
					Test->TestFalse(TEXT("Resume closes menu"), Host->IsLeaveMenuOpen());
					Test->TestFalse(TEXT("Resume restores movement and look"),
					                Host->IsMoveInputIgnored() || Host->IsLookInputIgnored());
					++Step;
					break;
				case 5:
					if (!State || State->Phase != ETBPhase::Results)
					{
						return false;
					}
					if (!HUD || !Second.IsValid())
					{
						Test->AddError(TEXT("Players/HUD lost at results"));
						return true;
					}
					Test->TestEqual(TEXT("Match ends through the game timer"), State->Reason,
					                FString(TEXT("Time expired")));
					HUD->NotifyHitBoxClick(TEXT("ReturnToLobby"));
					Test->TestEqual(TEXT("Rematch action returns to lobby"), State->Phase, ETBPhase::Lobby);
					Test->TestTrue(TEXT("Rematch retains world, players and session"),
					               World == RoundWorld.Get() && State->PlayerArray.Num() == 2 && HasSession(World));
					Test->TestFalse(TEXT("Host ready reset"), Host->GetPlayerState<ATBPlayerState>()->bReady);
					Test->TestFalse(TEXT("Second ready reset"), Second->GetPlayerState<ATBPlayerState>()->bReady);
					HUD->NotifyHitBoxClick(TEXT("Ready"));
					Second->TBReady();
					HUD->NotifyHitBoxClick(TEXT("Start"));
					if (!Test->TestEqual(TEXT("Can start the rematch"), State->Phase, ETBPhase::Playing))
					{
						return true;
					}
					Host->ToggleLeaveMenu();
					++Step;
					break;
				case 6:
					if (!Host || !HUD)
					{
						Test->AddError(TEXT("Host lost before leaving"));
						return true;
					}
					Test->TestTrue(TEXT("In-match leave menu open"), Host->IsLeaveMenuOpen());
					HUD->NotifyHitBoxClick(TEXT("Leave"));
					++Step;
					break;
				case 7:
					if (!World->GetAuthGameMode<ATBTitleGameMode>() || Session->IsBusy())
					{
						return false;
					}
					// Remove the synthetic split-screen participant after travel, so the
					// tested Leave action still originates in Playing, not in Results.
					if (auto* Player = GI->GetLocalPlayerByIndex(1))
					{
						UGameplayStatics::RemovePlayer(Player->GetPlayerController(World), true);
					}
					if (!CheckTitle(World))
					{
						return true;
					}
					Test->TestFalse(TEXT("In-match leave destroys session"), HasSession(World));
					if (!ClickTitle(TEXT("Host")))
					{
						return true;
					}
					++Step;
					break;
				case 8:
					if (!World->GetAuthGameMode<ATBGameMode>() || Session->IsBusy())
					{
						return false;
					}
					if (!HUD)
					{
						Test->AddError(TEXT("Lobby HUD missing on rehost"));
						return true;
					}
					Test->TestTrue(TEXT("Can host again"), HasSession(World));
					Test->TestEqual(TEXT("Rehost begins in lobby"), State->Phase, ETBPhase::Lobby);
					HUD->NotifyHitBoxClick(TEXT("Leave"));
					++Step;
					break;
				case 9:
					if (!World->GetAuthGameMode<ATBTitleGameMode>() || Session->IsBusy())
					{
						return false;
					}
					CheckTitle(World);
					Test->TestFalse(TEXT("Lobby leave destroys session"), HasSession(World));
					return true;
			}
			return false;
		}

	private:
		UTBTitleMenu* GetTitle()
		{
			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GI->GetWorld(), Widgets, UTBTitleMenu::StaticClass(), true);
			for (auto* Widget : Widgets)
			{
				if (Widget->GetOwningPlayer() == GI->GetWorld()->GetFirstPlayerController())
				{
					return Cast<UTBTitleMenu>(Widget);
				}
			}
			return nullptr;
		}
		bool ClickTitle(FName Name)
		{
			auto* Menu = GetTitle();
			auto* Button = Menu ? Cast<UButton>(Menu->WidgetTree->FindWidget(Name)) : nullptr;
			if (!Button || !Button->GetIsEnabled())
			{
				Test->AddError(FString::Printf(TEXT("Title button missing/disabled: %s"), *Name.ToString()));
				return false;
			}
			Button->OnClicked.Broadcast();
			return true;
		}
		bool CheckTitle(UWorld* World)
		{
			auto* PC = World->GetFirstPlayerController();
			bool Valid = Test->TestNotNull(TEXT("Title game mode"), World->GetAuthGameMode<ATBTitleGameMode>());
			Valid &= Test->TestTrue(TEXT("Title controller has no pawn"),
			                        PC && PC->IsA<ATBTitleController>() && !PC->GetPawn());
			TArray<UUserWidget*> Widgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UTBTitleMenu::StaticClass(), true);
			Valid &= Test->TestEqual(TEXT("Exactly one title menu"), Widgets.Num(), 1);
			return Valid;
		}
		FAutomationTestBase* Test;
		TWeakObjectPtr<UGameInstance> GI;
		TWeakObjectPtr<ATBController> Second;
		TWeakObjectPtr<UWorld> RoundWorld;
		double Deadline;
		int32 Step = 0;
	};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTBTitleFlowTest, "ToonStory.Title.RoundTrip",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FTBTitleFlowTest::RunTest(const FString& Parameters)
{
	for (const auto& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType != EWorldType::Game || !World || !World->GetAuthGameMode<ATBTitleGameMode>())
		{
			continue;
		}
		auto* Online = Online::GetSubsystem(World);
		if (!Online || Online->GetSubsystemName() != FName(TEXT("NULL")))
		{
			AddError(TEXT("Run Title.RoundTrip with the Null online subsystem, never a public Steam session."));
			return false;
		}
		ADD_LATENT_AUTOMATION_COMMAND(FTitleFlow(this, World->GetGameInstance()));
		return true;
	}
	AddError(TEXT("Run with /Game/Maps/Title -game and LAN/Null settings."));
	return false;
}
#endif
