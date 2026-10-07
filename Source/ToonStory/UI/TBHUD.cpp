#include "TBHUD.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Core/TBSession.h"
#include "Core/TBGameHelpers.h"
#include "Character/TBCharacter.h"
#include "GamePlay/TBBox.h"
#include "Components/SceneComponent.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarTBDebugHUD(TEXT("tb.DebugHUD"), 0, TEXT("Show detailed match diagnostics."));

void ATBHUD::DrawLobby()
{
	auto* PC = Cast<ATBController>(GetOwningPlayerController());
	auto* State = TB::GS(GetWorld());
	if (!PC || !State)
	{
		return;
	}
	const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	const float X = (Canvas->ClipX - 1200 * S) * .5f;
	const float Y = (Canvas->ClipY - 740 * S) * .5f;
	const FLinearColor Ink(.97f, .94f, .86f), Muted(.65f, .70f, .73f), Accent(.95f, .65f, .29f);
	auto Text =
	    [&](const FString& Value, float PX, float PY, float Scale = 1.f, FLinearColor Color = FLinearColor::White)
	{ DrawText(Value, Color, X + PX * S, Y + PY * S, nullptr, Scale * S); };
	auto Panel = [&](float PX, float PY, float W, float H, FLinearColor Color)
	{ DrawRect(Color, X + PX * S, Y + PY * S, W * S, H * S); };
	auto Button =
	    [&](FName Id, const FString& Label, float PX, float PY, float W, bool Enabled = true, bool Selected = false)
	{
		float MX = 0, MY = 0;
		const bool Hovered = PC->GetMousePosition(MX, MY) && MX >= X + PX * S && MX < X + (PX + W) * S &&
		                     MY >= Y + PY * S && MY < Y + (PY + 42) * S;
		const FLinearColor Fill = Selected             ? FLinearColor(.36f, .24f, .12f, .95f)
		                          : Enabled && Hovered ? FLinearColor(.16f, .32f, .34f, .98f)
		                                               : FLinearColor(.045f, .10f, .12f, .93f);
		Panel(PX, PY, W, 42, Fill);
		Panel(PX, PY + 40, W, 2, Selected ? Accent : FLinearColor(.20f, .31f, .32f, .8f));
		float TextWidth = 0, TextHeight = 0;
		GetTextSize(Label, TextWidth, TextHeight, nullptr, 1.05f);
		Text(Label, PX + 14, PY + 12, FMath::Min(1.05f, 1.05f * (W - 28) / FMath::Max(TextWidth, 1.f)),
		     Enabled ? Ink : Muted);
		if (Enabled)
		{
			AddHitBox(FVector2D(X + PX * S, Y + PY * S), FVector2D(W * S, 42 * S), Id, true);
		}
	};
	DrawRect(FLinearColor(.009f, .018f, .027f, .48f), 0, 0, Canvas->ClipX, Canvas->ClipY);
	Text(TEXT("TOON STORY"), 28, 22, 2.8f, Ink);
	Text(TEXT("A quiet house. A very lively toy box."), 30, 70, 1.15f, Muted);
	Panel(28, 106, 1144, 2, Accent);
	Panel(28, 130, 690, 360, FLinearColor(.018f, .032f, .043f, .87f));
	Text(bBrowsingRooms ? TEXT("AVAILABLE ROOMS") : TEXT("PLAYERS"), 48, 146, 1.25f, Accent);
	int32 Ready = 0, Row = 0;
	for (APlayerState* Player : State->PlayerArray)
	{
		const auto* P = Cast<ATBPlayerState>(Player);
		if (!P)
		{
			continue;
		}
		Ready += P->bReady ? 1 : 0;
		const FString PreferredRole = P->Preference == ETBTeam::Human ? TEXT("Child")
		                              : P->Preference == ETBTeam::Toy ? TEXT("Toy")
		                                                              : TEXT("Either");
		const float PY = 186 + Row++ * 34;
		if (bBrowsingRooms)
		{
			continue;
		}
		Panel(40, PY - 4, 666, 30, FLinearColor(.10f, .15f, .18f, Row % 2 ? .42f : .18f));
		Panel(48, PY + 4, 4, 12, P->bReady ? Accent : Muted);
		Text(P->GetPlayerName().Left(28), 62, PY, 1.1f, Ink);
		Text(PreferredRole, 365, PY, 1.1f, Muted);
		Text(P->bReady ? TEXT("READY") : TEXT("Preparing"), 535, PY, 1.1f, P->bReady ? Accent : Muted);
	}
	Panel(738, 130, 434, 360, FLinearColor(.018f, .032f, .043f, .87f));
	Text(TEXT("MATCH"), 758, 146, 1.25f, Accent);
	Text(State->StageName, 758, 182, 1.4f, Ink);
	Text(TEXT("Stage drawn randomly when hosting"), 758, 216, 1.f, Muted);
	Text(FString::Printf(TEXT("Children %d   /   Toys %d"), State->Settings.Humans, State->Settings.Toys), 758, 254,
	     1.2f, Ink);
	Text(FString::Printf(TEXT("%02d:%02d   /   %d collectibles"), FMath::CeilToInt(State->Settings.Duration) / 60,
	                     FMath::CeilToInt(State->Settings.Duration) % 60, State->Settings.RequiredItems),
	     758, 284, 1.1f, Ink);
	Text(TEXT("Doors stay open. Explore every room."), 758, 322, 1.f, Muted);
	const bool Host = PC->HasAuthority();
	Button(TEXT("FewerToys"), TEXT("- Toy slot"), 758, 362, 184, Host && State->Settings.Toys > 1);
	Button(TEXT("MoreToys"), TEXT("+ Toy slot"), 956, 362, 194,
	       Host && State->Settings.Humans + State->Settings.Toys < State->PlayerLimit);
	Text(FString::Printf(TEXT("%d / %d ready"), Ready, State->Settings.Humans + State->Settings.Toys), 758, 430, 1.25f,
	     Accent);
	Text(TEXT("YOUR PREFERENCE"), 28, 514, 1.f, Muted);
	const auto* Local = PC->GetPlayerState<ATBPlayerState>();
	Button(TEXT("Child"), TEXT("Play as child"), 28, 542, 190, true, Local && Local->Preference == ETBTeam::Human);
	Button(TEXT("Toy"), TEXT("Play as toy"), 230, 542, 190, true, Local && Local->Preference == ETBTeam::Toy);
	Button(TEXT("Either"), TEXT("Either team"), 432, 542, 180, true, Local && Local->Preference == ETBTeam::None);
	Button(TEXT("Ready"), Local && Local->bReady ? TEXT("Cancel ready [R]") : TEXT("Ready [R]"), 632, 542, 240,
	       Local != nullptr, Local && Local->bReady);
	const bool EveryoneReady = Ready == State->Settings.Humans + State->Settings.Toys && Ready == Row;
	const bool CanStart = Host && EveryoneReady;
	Button(TEXT("Start"), Host ? TEXT("Start match [F5]") : TEXT("Waiting for host"), 890, 542, 282, CanStart,
	       CanStart);
	if (!EveryoneReady)
	{
		Text(Row < State->Settings.Humans + State->Settings.Toys ? TEXT("Invite players or lower the toy count.")
		                                                         : TEXT("Everyone must be ready to start."),
		     28, 465, .9f, Muted);
	}
	Button(TEXT("Host"), TEXT("Host room"), 28, 600, 180);
	Button(TEXT("Find"), TEXT("Find rooms"), 222, 600, 180);
	Button(TEXT("Leave"), TEXT("Leave room"), 990, 600, 182);
	if (auto* Session = PC->GetGameInstance()->GetSubsystem<UTBSession>())
	{
		if (bBrowsingRooms)
		{
			RoomPage = FMath::Clamp(RoomPage, 0, FMath::Max(0, (Session->Rooms.Num() - 1) / 5));
			for (int32 I = RoomPage * 5; I < FMath::Min(RoomPage * 5 + 5, Session->Rooms.Num()); ++I)
			{
				Button(FName(*FString::Printf(TEXT("Join%d"), I)), Session->Rooms[I].Left(65), 48, 186 + (I % 5) * 50,
				       650);
			}
			if (Session->Rooms.IsEmpty())
			{
				Text(TEXT("No rooms yet. Refresh with Find rooms."), 48, 200, 1.f, Muted);
			}
			Button(TEXT("Players"), TEXT("Back to players"), 418, 600, 190);
			Button(TEXT("PreviousRooms"), TEXT("Previous"), 622, 600, 164, RoomPage > 0);
			Button(TEXT("NextRooms"), TEXT("Next"), 800, 600, 164, (RoomPage + 1) * 5 < Session->Rooms.Num());
		}
		Text(Session->Status.Left(115), 28, 660, .95f, Muted);
	}
	Text(PC->Notice.Left(115), 28, 688, 1.f, Accent);
	Text(TEXT("TOYS: collect and rescue     CHILDREN: catch and return toys     F10: console"), 28, 722, .9f, Muted);
}

void ATBHUD::NotifyHitBoxClick(FName Name)
{
	Super::NotifyHitBoxClick(Name);
	auto* PC = Cast<ATBController>(GetOwningPlayerController());
	const auto* State = TB::GS(GetWorld());
	if (!PC || !State)
	{
		return;
	}
	if (Name == TEXT("Leave") && State->Phase != ETBPhase::Playing)
	{
		PC->TBLeave();
		return;
	}
	if (Name == TEXT("ReturnToLobby"))
	{
		PC->TBLobby();
		return;
	}
	if (State->Phase != ETBPhase::Lobby)
	{
		return;
	}
	if (Name == TEXT("Host"))
	{
		PC->TBHost();
	}
	else if (Name == TEXT("Find"))
	{
		bBrowsingRooms = true;
		RoomPage = 0;
		PC->TBFind();
	}
	else if (Name == TEXT("Players"))
	{
		bBrowsingRooms = false;
	}
	else if (Name == TEXT("PreviousRooms"))
	{
		--RoomPage;
	}
	else if (Name == TEXT("NextRooms"))
	{
		++RoomPage;
	}
	else if (Name == TEXT("Ready"))
	{
		PC->TBReady();
	}
	else if (Name == TEXT("Start"))
	{
		PC->TBStart();
	}
	else if (Name == TEXT("Child"))
	{
		PC->TBPrefer(1);
	}
	else if (Name == TEXT("Toy"))
	{
		PC->TBPrefer(2);
	}
	else if (Name == TEXT("Either"))
	{
		PC->TBPrefer(0);
	}
	else if (Name.ToString().StartsWith(TEXT("Join")))
	{
		bBrowsingRooms = false;
		PC->TBJoin(FCString::Atoi(*Name.ToString().Mid(4)));
	}
	else if (Name == TEXT("MoreToys") || Name == TEXT("FewerToys"))
	{
		PC->TBRules(State->Settings.Humans, State->Settings.Toys + (Name == TEXT("MoreToys") ? 1 : -1),
		            State->Settings.Duration, State->Settings.RequiredItems);
	}
}

// HUDは状態の表示だけを行う。勝敗や取得の判定はサーバー側で行う。
void ATBHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	const auto* State = TB::GS(GetWorld());
	auto* PC = Cast<ATBController>(GetOwningPlayerController());
	if (!State || !PC)
	{
		return;
	}
	if (State->Phase == ETBPhase::Lobby)
	{
		DrawLobby();
		return;
	}
	if (State->Phase == ETBPhase::Results || State->Phase == ETBPhase::Aborted)
	{
		DrawResults(*State, *PC);
	}
	else
	{
		DrawMatch(*State, *PC);
	}
}

void ATBHUD::DrawResults(const ATBGameState& MatchState, ATBController& Controller)
{
	const auto* State = &MatchState;
	auto* PC = &Controller;
	const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const FLinearColor Ink(.97f, .94f, .86f), Accent(.95f, .65f, .29f), Panel(.018f, .032f, .043f, .85f);
	DrawRect(FLinearColor(.01f, .02f, .03f, .72f), 0, 0, W, H);
	const float X = W * .5f - 300 * S, Y = H * .5f - 140 * S;
	DrawRect(Panel, X, Y, 600 * S, 280 * S);
	const FString Title = MatchState.Phase == ETBPhase::Aborted  ? TEXT("MATCH ENDED")
	                      : MatchState.Winner == ETBWinner::Toys ? TEXT("THE TOYS WIN")
	                                                             : TEXT("THE CHILDREN WIN");
	DrawText(Title, Accent, X + 32 * S, Y + 30 * S, nullptr, 2.4f * S);
	DrawText(MatchState.Reason.Left(70), Ink, X + 32 * S, Y + 94 * S, nullptr, 1.05f * S);
	DrawText(FString::Printf(TEXT("Collected %d / %d     Toys in box %d"), MatchState.Collected,
	                         MatchState.Settings.RequiredItems, MatchState.BoxedCount),
	         Ink, X + 32 * S, Y + 128 * S, nullptr, 1.1f * S);
	const bool Host = Controller.HasAuthority();
	DrawRect(FLinearColor(.12f, .28f, .29f, .95f), X + 32 * S, Y + 194 * S, 326 * S, 48 * S);
	DrawText(Host ? TEXT("Return together to lobby") : TEXT("Waiting for host to return"), Ink, X + 46 * S, Y + 208 * S,
	         nullptr, 1.f * S);
	if (Host)
	{
		AddHitBox(FVector2D(X + 32 * S, Y + 194 * S), FVector2D(326 * S, 48 * S), TEXT("ReturnToLobby"), true);
	}
	DrawRect(FLinearColor(.16f, .08f, .07f, .95f), X + 374 * S, Y + 194 * S, 194 * S, 48 * S);
	DrawText(Host ? TEXT("Close room") : TEXT("Leave room"), Ink, X + 390 * S, Y + 208 * S, nullptr, 1.f * S);
	AddHitBox(FVector2D(X + 374 * S, Y + 194 * S), FVector2D(194 * S, 48 * S), TEXT("Leave"), true);
}

void ATBHUD::DrawMatch(const ATBGameState& MatchState, ATBController& Controller)
{
	const auto* State = &MatchState;
	auto* PC = &Controller;
	const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const FLinearColor Ink(.97f, .94f, .86f), Accent(.95f, .65f, .29f), Panel(.018f, .032f, .043f, .85f);
	const auto* Player = Controller.GetPlayerState<ATBPlayerState>();
	const bool Child = Player && Player->Team == ETBTeam::Human;
	DrawRect(Panel, 24 * S, 24 * S, 310 * S, 77 * S);
	DrawText(Child ? TEXT("CHILD") : TEXT("TOY"), Accent, 40 * S, 35 * S, nullptr, 1.4f * S);
	DrawText(Child ? TEXT("Find toys. Bring them to the box.") : TEXT("Collect batteries. Rescue your friends."), Ink,
	         40 * S, 67 * S, nullptr, .95f * S);
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(MatchState.Remaining()));
	DrawRect(Panel, W * .5f - 64 * S, 24 * S, 128 * S, 56 * S);
	DrawText(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60), Seconds < 60 ? Accent : Ink,
	         W * .5f - 42 * S, 36 * S, nullptr, 2.f * S);
	DrawRect(Panel, W - 286 * S, 24 * S, 262 * S, 77 * S);
	DrawText(FString::Printf(TEXT("BATTERIES  %d / %d"), MatchState.Collected, MatchState.Settings.RequiredItems), Ink,
	         W - 270 * S, 36 * S, nullptr, 1.35f * S);
	DrawText(FString::Printf(TEXT("TOYS IN BOX  %d / %d"), MatchState.BoxedCount, MatchState.Settings.Toys), Accent,
	         W - 270 * S, 68 * S, nullptr, .95f * S);
	DrawRect(Panel, 24 * S, H - 62 * S, 700 * S, 38 * S);
	DrawText(Child ? TEXT("WASD Move    Space Jump    E Grab / Store    Q Put down")
	               : TEXT("WASD Move    Shift Sprint    Space Jump    E Rescue    Touch batteries to collect"),
	         Ink, 40 * S, H - 51 * S, nullptr, .95f * S);
	if (Player && Player->bFrozen)
	{
		DrawRect(FLinearColor(.28f, .045f, .035f, .9f), W * .5f - 165 * S, H * .7f, 330 * S, 44 * S);
		DrawText(TEXT("YOU ARE BEING WATCHED"), Ink, W * .5f - 146 * S, H * .7f + 12 * S, nullptr, 1.15f * S);
	}
	if (auto* Character = Cast<ATBCharacter>(Controller.GetPawn()); Character && Character->ContactItem)
	{
		DrawRect(Panel, W * .5f - 150 * S, H * .78f, 300 * S, 38 * S);
		DrawRect(Accent, W * .5f - 150 * S, H * .78f + 34 * S,
		         300 * S * FMath::Clamp(Character->ItemProgress, 0.f, 1.f), 4 * S);
		DrawText(Character->IsGazeFrozen() ? TEXT("Collection paused") : TEXT("Collecting..."), Ink, W * .5f - 130 * S,
		         H * .78f + 10 * S, nullptr, 1.f * S);
	}
	if (Player && Player->Team == ETBTeam::Toy && MatchState.Box && MatchState.Box->Rescuers > 0)
	{
		DrawText(FString::Printf(TEXT("RESCUE  %.0f%%"), MatchState.Box->RescueProgress * 100), Accent,
		         W * .5f - 70 * S, 108 * S, nullptr, 1.2f * S);
	}
	if (auto* Character = Cast<ATBCharacter>(Controller.GetPawn());
	    Child && Character && Character->CarriedToy && MatchState.Box)
	{
		const bool NearBox = MatchState.Box->CanStoreFrom(Character);
		FVector2D BoxScreen;
		const FVector BoxLocation = MatchState.Box->InteractionPoint->GetComponentLocation();
		if (Controller.ProjectWorldLocationToScreen(BoxLocation + FVector(0, 0, 90), BoxScreen) &&
		    BoxScreen.X > 70 * S && BoxScreen.X < W - 70 * S && BoxScreen.Y > 150 * S && BoxScreen.Y < H * .7f)
		{
			DrawText(FString::Printf(TEXT("TOY BOX  %.1f m"),
			                         FVector::Distance(Character->GetActorLocation(), BoxLocation) / 100.f),
			         Accent, BoxScreen.X - 60 * S, BoxScreen.Y, nullptr, 1.f * S);
		}
		const bool Storing = Character->StoreStarted >= 0;
		const float Duration = FMath::Max(MatchState.Box->StoreSeconds, .01f);
		const float Elapsed =
		    Storing
		        ? FMath::Clamp(float(MatchState.GetServerWorldTimeSeconds() - Character->StoreStarted), 0.f, Duration)
		        : 0.f;
		const FString Prompt = Storing
		                           ? FString::Printf(TEXT("HOLD E  |  Storing toy... %.1f / %.0f s"), Elapsed, Duration)
		                           : (NearBox ? FString::Printf(TEXT("Hold E for %.0f seconds to store toy"), Duration)
		                                      : TEXT("Carrying toy - approach the front of the toy box"));
		DrawRect(Panel, W * .5f - 235 * S, H * .76f, 470 * S, 64 * S);
		DrawText(Prompt, Ink, W * .5f - 220 * S, H * .76f + 9 * S, nullptr, 1.f * S);
		DrawRect(FLinearColor(.1f, .14f, .16f, 1), W * .5f - 220 * S, H * .76f + 37 * S, 440 * S, 7 * S);
		DrawRect(Accent, W * .5f - 220 * S, H * .76f + 37 * S, 440 * S * Elapsed / Duration, 7 * S);
		DrawText(TEXT("Release E or step away to cancel. Q puts the toy down."), Ink, W * .5f - 220 * S,
		         H * .76f + 49 * S, nullptr, .75f * S);
	}
	DrawRect(Ink, W * .5f - 3 * S, H * .5f - 1 * S, 6 * S, 2 * S);
	DrawRect(Ink, W * .5f - 1 * S, H * .5f - 3 * S, 2 * S, 6 * S);
	if (CVarTBDebugHUD.GetValueOnGameThread() != 0)
	{
		DrawDebugHUD();
	}
}

// 通常HUDに内部状態だけを追加する。ロビー・結果画面の操作は置き換えない。
void ATBHUD::DrawDebugHUD()
{
	const auto* State = TB::GS(GetWorld());
	if (!State)
	{
		return;
	}
	const float Scale = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	float Y = 155.f * Scale;
	auto Line = [this, Scale, &Y](const FString& Text)
	{
		DrawText(Text, FLinearColor::Yellow, 24.f * Scale, Y, nullptr, Scale);
		Y += 22.f * Scale;
	};
	for (const APlayerState* Player : State->PlayerArray)
	{
		if (const auto* Info = Cast<ATBPlayerState>(Player))
		{
			Line(FString::Printf(TEXT("%s | Team:%d State:%d Frozen:%d"), *Info->GetPlayerName(), int32(Info->Team),
			                     int32(Info->ToyState), Info->bFrozen));
		}
	}
	const auto* Viewer =
	    GetOwningPlayerController() ? GetOwningPlayerController()->GetPlayerState<ATBPlayerState>() : nullptr;
	if (Viewer && Viewer->Team == ETBTeam::Toy && State->Box)
	{
		Line(FString::Printf(TEXT("Rescue: %.0f%% | Rescuers: %d"), State->Box->RescueProgress * 100,
		                     State->Box->Rescuers));
	}
}
