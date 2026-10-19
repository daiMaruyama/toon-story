// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBHUD.h"
#include "TBPaperStyle.h"
#include "Core/TBLobbySettings.h"
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

bool ATBHUD::HandleLobbyPanelClick(FName Name)
{
	if (Name == TEXT("OpenSettings"))
	{
		bLobbySettingsOpen = true;
		bStagePickerOpen = false;
		return true;
	}
	if (Name == TEXT("CloseSettings"))
	{
		bLobbySettingsOpen = false;
		bStagePickerOpen = false;
		return true;
	}
	if (Name == TEXT("Ready"))
	{
		return false;
	}
	if (!bLobbySettingsOpen)
	{
		return true;
	}
	if (Name == TEXT("ChangeStage"))
	{
		bStagePickerOpen = true;
		return true;
	}
	if (Name == TEXT("CancelStage"))
	{
		bStagePickerOpen = false;
		return true;
	}
	const int32 Stage = Name == TEXT("StageKidsRoom")   ? 0
	                    : Name == TEXT("StageExpanded") ? 1
	                    : Name == TEXT("StageArchViz")  ? 2
	                                                    : INDEX_NONE;
	if (bStagePickerOpen && Stage != INDEX_NONE)
	{
		if (auto* PC = Cast<ATBController>(GetOwningPlayerController()))
		{
			PC->ServerSelectStage(Stage);
		}
		bStagePickerOpen = false;
		return true;
	}
	return bStagePickerOpen || Stage != INDEX_NONE || Name == TEXT("StageDemo");
}

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
	const FLinearColor Ink = TBPaper::Ink, Muted = TBPaper::Muted, Accent = TBPaper::Accent;
	auto Text =
	    [&](const FString& Value, float PX, float PY, float Scale = 1.f, FLinearColor Color = FLinearColor::White)
	{ DrawText(Value, Color, X + PX * S, Y + PY * S, PaperFont, Scale * S); };
	auto Panel = [&](float PX, float PY, float W, float H, FLinearColor Color)
	{
		if (H > 20)
		{
			PaperPanel(Color, X + PX * S, Y + PY * S, W * S, H * S);
		}
		else
		{
			DrawRect(Color, X + PX * S, Y + PY * S, W * S, H * S);
		}
	};
	auto Button =
	    [&](FName Id, const FString& Label, float PX, float PY, float W, bool Enabled = true, bool Selected = false, float Height = 42)
	{
		float MX = 0, MY = 0;
		const bool Hovered = PC->GetMousePosition(MX, MY) && MX >= X + PX * S && MX < X + (PX + W) * S &&
		                     MY >= Y + PY * S && MY < Y + (PY + Height) * S;
		const FLinearColor Fill = Selected ? TBPaper::Cream : Enabled && Hovered ? TBPaper::Cream : FLinearColor::White;
		const float Age = GetWorld()->GetTimeSeconds() - LastButtonClickTime;
		const float Lift = Enabled ? (Id == LastClickedButton && Age >= 0 && Age < .25f
		    ? 7.f * (1.f - Age / .25f) : Hovered ? 3.f : 0.f) : 0.f;
		Panel(PX + 2, PY + 4, W, Height, FLinearColor(0, 0, 0, .18f));
		Panel(PX, PY - Lift, W, Height, Fill);
		Panel(PX, PY + Height - 2 - Lift, W, 2, Selected ? Accent : FLinearColor(.20f, .31f, .32f, .8f));
		float TextWidth = 0, TextHeight = 0;
		GetTextSize(Label, TextWidth, TextHeight, PaperFont, 1.05f);
		Text(Label, PX + 14, PY + 5 - Lift, FMath::Min(1.05f, 1.05f * (W - 28) / FMath::Max(TextWidth, 1.f)),
		     Enabled ? Ink : Muted);
		if (Enabled)
		{
			AddHitBox(FVector2D(X + PX * S, Y + PY * S), FVector2D(W * S, Height * S), Id, true);
		}
	};
	const auto* Local = PC->GetPlayerState<ATBPlayerState>();
	int32 ReadyCount = 0;
	for (APlayerState* Player : State->PlayerArray)
	{
		if (const auto* P = Cast<ATBPlayerState>(Player))
		{
			ReadyCount += P->bReady ? 1 : 0;
		}
	}
	if (!bLobbySettingsOpen)
	{
		const float Bottom = (Canvas->ClipY - Y) / S - 84;
		Panel(216, Bottom - 12, 768, 72, TBPaper::Cream);
		Button(TEXT("Ready"), Local && Local->bReady ? TEXT("準備をやめる [R]") : TEXT("準備OK [R]"), 232, Bottom, 240,
		       Local != nullptr, Local && Local->bReady);
		Text(FString::Printf(TEXT("%d / %d人 準備OK"), ReadyCount, State->Settings.Humans + State->Settings.Toys), 496,
		     Bottom + 8, 1.05f, Ink);
		Button(TEXT("OpenSettings"), TEXT("設定を開く"), 752, Bottom, 216);
		return;
	}
	DrawRect(FLinearColor(.009f, .018f, .027f, .48f), 0, 0, Canvas->ClipX, Canvas->ClipY);
	if (bStagePickerOpen)
	{
		Panel(280, 150, 640, 470, FLinearColor::White);
		Text(TEXT("ステージを選ぶ"), 312, 194, 1.6f, Accent);
		Text(TEXT("試合開始時に、選んだステージへ移動します。"), 312, 241, .95f, Muted);
		const int32 Current = State->SelectedStage;
		Button(TEXT("StageExpanded"), TEXT("Demo ＋ こども部屋"), 312, 284, 576, PC->HasAuthority(), Current == 1);
		Text(TEXT("リビングと、ドアでつながる子ども部屋"), 328, 330, .85f, Muted);
		Button(TEXT("StageKidsRoom"), TEXT("Kids' Rooms"), 312, 369, 576, PC->HasAuthority(), Current == 0);
		Text(TEXT("家具付きの4部屋と廊下"), 328, 415, .85f, Muted);
		Button(TEXT("StageArchViz"), TEXT("ArchViz Apartment"), 312, 454, 576, PC->HasAuthority(), Current == 2);
		Text(TEXT("アパートの4部屋と通路"), 328, 500, .85f, Muted);
		Button(TEXT("CancelStage"), TEXT("設定にもどる"), 654, 557, 234);
		return;
	}
	Text(TEXT("TOON STORY"), 28, 22, 2.3f, TBPaper::Cream);
	Text(TEXT("おもちゃたちの、ひみつの時間。"), 30, 70, 1.0f, TBPaper::Cream);
	Button(TEXT("CloseSettings"), TEXT("設定を閉じる"), 946, 34, 226);
	Panel(28, 106, 1144, 2, Accent);
	Panel(28, 130, 690, 360, FLinearColor::White);
	Text(TEXT("いっしょにあそぶ仲間"), 48, 146, 1.25f, Accent);
	int32 Ready = 0, Row = 0;
	for (APlayerState* Player : State->PlayerArray)
	{
		const auto* P = Cast<ATBPlayerState>(Player);
		if (!P)
		{
			continue;
		}
		Ready += P->bReady ? 1 : 0;
		const FString PreferredRole = P->Preference == ETBTeam::Human ? TEXT("にんげん")
		                              : P->Preference == ETBTeam::Toy ? TEXT("おもちゃ")
		                                                              : TEXT("どちらでも");
		const float PY = 186 + Row++ * 34;
		Panel(40, PY - 4, 666, 30, FLinearColor(.10f, .15f, .18f, Row % 2 ? .42f : .18f));
		Panel(48, PY + 4, 4, 12, P->bReady ? Accent : Muted);
		Text(P->GetPlayerName().Left(18), 62, PY - 5, 1.f, Ink);
		Text(PreferredRole, 320, PY - 5, 1.1f, Muted);
		Text(P->bReady ? TEXT("準備OK") : TEXT("準備中"), 455, PY - 5, 1.1f, P->bReady ? Accent : Muted);
		if (PC->HasAuthority() && P != Local)
		{
			Button(FName(*FString::Printf(TEXT("Kick_%d"), P->GetPlayerId())), TEXT("キック"),
			       610, PY - 4, 88, !State->bTravelPending, false, 28);
		}
	}
	Panel(738, 130, 434, 360, FLinearColor::White);
	Text(TEXT("今回のあそび"), 758, 146, 1.25f, Accent);
	const bool Host = PC->HasAuthority();
	Button(TEXT("ChangeStage"), State->StageName + TEXT("  ／ 選択"), 758, 180, 392, Host);
	Text(FString::Printf(TEXT("制限時間  %d分"), FMath::RoundToInt(State->Settings.Duration / 60)), 758, 245, 1.1f,
	     Ink);
	Button(TEXT("LessTime"), TEXT("-"), 1032, 235, 52, Host && State->Settings.Duration > 60);
	Button(TEXT("MoreTime"), TEXT("+"), 1098, 235, 52, Host && State->Settings.Duration < 3600);
	Text(FString::Printf(TEXT("集める電池  %d個"), State->Settings.RequiredItems), 758, 301, 1.1f, Ink);
	Button(TEXT("FewerBatteries"), TEXT("-"), 1032, 291, 52, Host && State->Settings.RequiredItems > 1);
	Button(TEXT("MoreBatteries"), TEXT("+"), 1098, 291, 52,
	       Host && State->Settings.RequiredItems < TBLobbySettings::AvailableItems(GetWorld()));
	Button(TEXT("FewerToys"), TEXT("- おもちゃ"), 758, 350, 184, Host && State->Settings.Toys > 1);
	Button(TEXT("MoreToys"), TEXT("+ おもちゃ"), 956, 350, 194,
	       Host && State->Settings.Humans > 1);
	Text(FString::Printf(TEXT("にんげん %d人 ／ おもちゃ %d体"), State->Settings.Humans, State->Settings.Toys), 758,
	     405, .95f, Ink);
	Text(FString::Printf(TEXT("%d / %d人 準備OK"), Ready, State->Settings.Humans + State->Settings.Toys), 758, 430,
	     1.25f, Accent);
	Text(TEXT("どちらであそぶ？"), 28, 514, 1.f, TBPaper::Cream);
	Button(TEXT("Child"), TEXT("にんげん"), 28, 542, 190, true, Local && Local->Preference == ETBTeam::Human);
	Button(TEXT("Toy"), TEXT("おもちゃ"), 230, 542, 190, true, Local && Local->Preference == ETBTeam::Toy);
	Button(TEXT("Either"), TEXT("どちらでも"), 432, 542, 180, true, Local && Local->Preference == ETBTeam::None);
	Button(TEXT("Ready"), Local && Local->bReady ? TEXT("準備をやめる [R]") : TEXT("準備OK [R]"), 632, 542, 240,
	       Local != nullptr, Local && Local->bReady);
	const bool EveryoneReady = Ready == State->Settings.Humans + State->Settings.Toys && Ready == Row;
	const bool EnoughPlayers = State->Settings.Humans >= 1 && State->Settings.Toys >= 1;
	const bool CanStart = Host && EveryoneReady && EnoughPlayers;
	Button(TEXT("Start"), Host ? TEXT("あそびはじめる [F5]") : TEXT("ホストを待っています"), 890, 542, 282, CanStart,
	       CanStart);
	if (!EveryoneReady || !EnoughPlayers)
	{
		Text(!EnoughPlayers
		         ? TEXT("にんげんとおもちゃ、2人以上で遊べます。")
		         : TEXT("全員の準備ができたら、はじめよう。"),
		     48, 456, .85f, Muted);
	}
	Button(TEXT("Leave"), Host ? TEXT("部屋を閉じてタイトルへ") : TEXT("部屋を出てタイトルへ"), 922, 600, 250);
	if (auto* Session = PC->GetGameInstance()->GetSubsystem<UTBSession>())
	{
		Text(TBPaper::Status(Session->Status).Left(115), 28, 660, .95f, TBPaper::Cream);
	}
	Text(PC->Notice.Left(115), 28, 688, 1.f, Accent);
	Text(TEXT("おもちゃ：電池を集めて仲間を救助 ／ にんげん：おもちゃを見つけて箱へ"), 28, 722, .9f, TBPaper::Cream);
}

void ATBHUD::DrawResults(const ATBGameState& MatchState, ATBController& Controller)
{
	const auto* State = &MatchState;
	auto* PC = &Controller;
	const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const FLinearColor Ink = TBPaper::Ink, Accent = TBPaper::Accent, Panel = FLinearColor::White;
	DrawRect(FLinearColor(.01f, .02f, .03f, .72f), 0, 0, W, H);
	const float X = W * .5f - 300 * S, Y = H * .5f - 140 * S;
	PaperPanel(Panel, X, Y, 600 * S, 280 * S);
	const FString Title = MatchState.Phase == ETBPhase::Aborted  ? TEXT("あそびはおしまい")
	                      : MatchState.Winner == ETBWinner::Toys ? TEXT("おもちゃの勝ち！")
	                                                             : TEXT("にんげんの勝ち！");
	DrawText(Title, Accent, X + 32 * S, Y + 30 * S, PaperFont, 2.4f * S);
	DrawText(MatchState.Reason.Left(70), Ink, X + 32 * S, Y + 94 * S, PaperFont, 1.05f * S);
	DrawText(FString::Printf(TEXT("電池 %d / %d個 ／ 箱のなか %d体"), MatchState.Collected,
	                         MatchState.Settings.RequiredItems, MatchState.BoxedCount),
	         Ink, X + 32 * S, Y + 128 * S, PaperFont, 1.1f * S);
	const bool Host = Controller.HasAuthority();
	DrawRect(TBPaper::Cream, X + 32 * S, Y + 194 * S, 326 * S, 48 * S);
	DrawText(Host ? TEXT("みんなでロビーへ") : TEXT("ホストの操作を待っています"), Ink, X + 46 * S, Y + 208 * S,
	         PaperFont, 1.f * S);
	if (Host)
	{
		AddHitBox(FVector2D(X + 32 * S, Y + 194 * S), FVector2D(326 * S, 48 * S), TEXT("ReturnToLobby"), true);
	}
	DrawRect(TBPaper::Cream, X + 374 * S, Y + 194 * S, 194 * S, 48 * S);
	DrawText(Host ? TEXT("部屋を閉じる") : TEXT("部屋を出る"), Ink, X + 390 * S, Y + 208 * S, PaperFont, 1.f * S);
	AddHitBox(FVector2D(X + 374 * S, Y + 194 * S), FVector2D(194 * S, 48 * S), TEXT("Leave"), true);
}

void ATBHUD::DrawMatch(const ATBGameState& MatchState, ATBController& Controller)
{
	const auto* State = &MatchState;
	auto* PC = &Controller;
	const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const FLinearColor Ink = TBPaper::Ink, Accent = TBPaper::Accent, Panel = FLinearColor::White;
	const auto* Player = Controller.GetPlayerState<ATBPlayerState>();
	const bool Child = Player && Player->Team == ETBTeam::Human;
	PaperPanel(Panel, 24 * S, 24 * S, 310 * S, 77 * S);
	DrawText(Child ? TEXT("にんげん") : TEXT("おもちゃ"), Accent, 40 * S, 35 * S, PaperFont, 1.4f * S);
	DrawText(Child ? TEXT("おもちゃを見つけて、箱へ。") : TEXT("電池を集めて、仲間を救助。"), Ink, 40 * S, 67 * S,
	         PaperFont, .95f * S);
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(MatchState.Remaining()));
	PaperPanel(Panel, W * .5f - 64 * S, 24 * S, 128 * S, 56 * S);
	DrawText(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60), Seconds < 60 ? Accent : Ink,
	         W * .5f - 42 * S, 36 * S, PaperFont, 2.f * S);
	PaperPanel(Panel, W - 286 * S, 24 * S, 262 * S, 77 * S);
	DrawText(FString::Printf(TEXT("電池  %d / %d"), MatchState.Collected, MatchState.Settings.RequiredItems), Ink,
	         W - 270 * S, 36 * S, PaperFont, 1.35f * S);
	DrawText(FString::Printf(TEXT("箱のなか  %d / %d"), MatchState.BoxedCount, MatchState.Settings.Toys), Accent,
	         W - 270 * S, 68 * S, PaperFont, .95f * S);
	PaperPanel(Panel, 24 * S, H - 62 * S, (Child ? 900 : 700) * S, 38 * S);
	DrawText(Child ? TEXT("WASD 移動 ／ Ctrl しゃがむ ／ Space ジャンプ ／ E つかむ・収納 ／ Q おろす")
	               : TEXT("WASD 移動 ／ Shift ダッシュ ／ Space ジャンプ ／ E 救助"),
	         Ink, 40 * S, H - 51 * S, PaperFont, .95f * S);
	if (Player && Player->bFrozen)
	{
		DrawRect(FLinearColor(.28f, .045f, .035f, .9f), W * .5f - 165 * S, H * .7f, 330 * S, 44 * S);
		DrawText(TEXT("見られている！ 動けない…"), Ink, W * .5f - 146 * S, H * .7f + 12 * S, PaperFont, 1.15f * S);
	}
	if (auto* Character = Cast<ATBCharacter>(Controller.GetPawn()); Character && Character->ContactItem)
	{
		PaperPanel(Panel, W * .5f - 150 * S, H * .78f, 300 * S, 38 * S);
		DrawRect(Accent, W * .5f - 150 * S, H * .78f + 34 * S,
		         300 * S * FMath::Clamp(Character->ItemProgress, 0.f, 1.f), 4 * S);
		DrawText(Character->IsGazeFrozen() ? TEXT("見られている間は集められません") : TEXT("電池を集めています…"), Ink,
		         W * .5f - 130 * S, H * .78f + 10 * S, PaperFont, 1.f * S);
	}
	if (Player && Player->Team == ETBTeam::Toy && MatchState.Box && MatchState.Box->Rescuers > 0)
	{
		DrawText(FString::Printf(TEXT("救助中  %.0f%%"), MatchState.Box->RescueProgress * 100), Accent,
		         W * .5f - 70 * S, 108 * S, PaperFont, 1.2f * S);
	}
	if (auto* Character = Cast<ATBCharacter>(Controller.GetPawn());
	    Child && Character && Character->HasCarriedToy() && MatchState.Box)
	{
		const bool NearBox = MatchState.Box->CanStoreFrom(Character);
		FVector2D BoxScreen;
		const FVector BoxLocation = MatchState.Box->InteractionPoint->GetComponentLocation();
		if (Controller.ProjectWorldLocationToScreen(BoxLocation + FVector(0, 0, 90), BoxScreen) &&
		    BoxScreen.X > 70 * S && BoxScreen.X < W - 70 * S && BoxScreen.Y > 150 * S && BoxScreen.Y < H * .7f)
		{
			DrawText(FString::Printf(TEXT("おもちゃ箱  %.1f m"),
			                         FVector::Distance(Character->GetActorLocation(), BoxLocation) / 100.f),
			         Accent, BoxScreen.X - 60 * S, BoxScreen.Y, PaperFont, 1.f * S);
		}
		const bool Storing = Character->StoreStarted >= 0;
		const float Duration = FMath::Max(MatchState.Box->StoreSeconds, .01f);
		const float Elapsed =
		    Storing
		        ? FMath::Clamp(float(MatchState.GetServerWorldTimeSeconds() - Character->StoreStarted), 0.f, Duration)
		        : 0.f;
		const FString Prompt = Storing
		                           ? FString::Printf(TEXT("E 長押し  しまっています… %.1f / %.0f秒"), Elapsed, Duration)
		                           : (NearBox ? FString::Printf(TEXT("Eを%.0f秒長押しして箱にしまう"), Duration)
		                                      : TEXT("おもちゃを運搬中。箱の正面へ。"));
		PaperPanel(Panel, W * .5f - 235 * S, H * .76f, 470 * S, 64 * S);
		DrawText(Prompt, Ink, W * .5f - 220 * S, H * .76f + 9 * S, PaperFont, 1.f * S);
		DrawRect(FLinearColor(.1f, .14f, .16f, 1), W * .5f - 220 * S, H * .76f + 37 * S, 440 * S, 7 * S);
		DrawRect(Accent, W * .5f - 220 * S, H * .76f + 37 * S, 440 * S * Elapsed / Duration, 7 * S);
		DrawText(TEXT("Eを離すと中止 ／ Qでおろす"), Ink, W * .5f - 220 * S, H * .76f + 49 * S, PaperFont, .75f * S);
	}
	if (Controller.bStuckHint)
	{
		DrawText(TEXT("動けない場合：Esc → スタック解除"), TBPaper::Accent, 24 * S, H * .65f, PaperFont, S);
	}
	if (!Controller.Notice.IsEmpty())
	{
		PaperPanel(FLinearColor::White, W - 450 * S, H - 155 * S, 426 * S, 70 * S);
		DrawText(Controller.Notice.Left(80), TBPaper::Ink, W - 428 * S, H - 132 * S, PaperFont, .72f * S);
	}
	DrawRect(Ink, W * .5f - 3 * S, H * .5f - 1 * S, 6 * S, 2 * S);
	DrawRect(Ink, W * .5f - 1 * S, H * .5f - 3 * S, 2 * S, 6 * S);
	if (CVarTBDebugHUD.GetValueOnGameThread() != 0)
	{
		DrawDebugHUD();
	}
}

void ATBHUD::BeginPlay()
{
	Super::BeginPlay();
	PaperFont = TBPaper::Font(this);
	PaperTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/PaperClub/Panel.Panel"));
}

void ATBHUD::DrawText(const FString& Text, FLinearColor Color, float X, float Y, UFont* Font, float Scale,
                      bool ScalePosition)
{
	// Zen Maru's ascender padding is larger than the old debug font's. Align the ink to the authored rows.
	Super::DrawText(Text, Color, X, Y - 10.f * Scale, Font ? Font : PaperFont.Get(), Scale, ScalePosition);
}

void ATBHUD::PaperPanel(FLinearColor Tint, float X, float Y, float W, float H)
{
	if (!PaperTexture)
	{
		DrawRect(Tint, X, Y, W, H);
		return;
	}
	const float Edge = FMath::Min(20.f * Canvas->ClipY / 800.f, FMath::Min(W, H) * .35f);
	const float PX[] = {X, X + Edge, X + W - Edge, X + W};
	const float PY[] = {Y, Y + Edge, Y + H - Edge, Y + H};
	const float UV[] = {.04f, .25f, .70f, .93f};
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Col = 0; Col < 3; ++Col)
		{
			DrawTexture(PaperTexture, PX[Col], PY[Row], PX[Col + 1] - PX[Col], PY[Row + 1] - PY[Row], UV[Col], UV[Row],
			            UV[Col + 1] - UV[Col], UV[Row + 1] - UV[Row], Tint);
		}
	}
}
