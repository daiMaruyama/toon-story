// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBHUD.h"
#include "TBPaperStyle.h"
#include "Core/TBLobbySettings.h"
#include "Core/TBChat.h"
#include "Core/TBPlayerState.h"
#include "Core/TBGameState.h"
#include "Core/TBController.h"
#include "Input/TBKeyLabels.h"
#include "Core/TBSession.h"
#include "Core/TBGameHelpers.h"
#include "Character/TBCharacter.h"
#include "Data/TBRuleData.h"
#include "GamePlay/TBBox.h"
#include "GamePlay/Events/TBMomArrivalZone.h"
#include "GamePlay/TBBoxEscapeCourse.h"
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
	if (Name == TEXT("Ready") || Name == TEXT("Start"))
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
	const int32 Stage = Name == TEXT("StageExpanded") ? TBLobbySettings::DemoStageId
	                    : Name == TEXT("StagePlayroom") ? TBLobbySettings::HouseStageId
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
	auto DrawNotice = [&](float PY, float Width, float AvailableHeight, bool IncludeStatus = false)
	{
		constexpr float FontScale = .85f;
		float TextWidth = 0, TextHeight = 0;
		GetTextSize(TEXT("あAg"), TextWidth, TextHeight, PaperFont, FontScale);
		const float LineHeight = FMath::Max(16.f, TextHeight) + 6.f;
		const int32 MaxLines = FMath::FloorToInt((AvailableHeight - 16.f) / LineHeight);
		if (MaxLines < 1) return;
		const float TextArea = Width - 28.f;
		auto Fits = [&](const FString& Value)
		{
			GetTextSize(Value, TextWidth, TextHeight, PaperFont, FontScale);
			return TextWidth <= TextArea;
		};
		auto Ellipsize = [&](FString Value)
		{
			while (!Value.IsEmpty() && !Fits(Value + TEXT("…"))) Value.LeftChopInline(1);
			return Value + TEXT("…");
		};
		TArray<FString> Lines;
		if (IncludeStatus)
		{
			if (auto* Session = PC->GetGameInstance()->GetSubsystem<UTBSession>())
			{
				FString Status = TBPaper::Status(Session->Status).Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
				// Keep space for the notice even when the session status is long.
				if (!Status.IsEmpty() && (PC->Notice.IsEmpty() || MaxLines > 1))
					Lines.Add(Fits(Status) ? Status : Ellipsize(Status));
			}
		}
		FString Line;
		const FString Message = PC->Notice.Replace(TEXT("\r"), TEXT(""));
		for (int32 Index = 0; Index < Message.Len(); ++Index)
		{
			const TCHAR Character = Message[Index] == TEXT('\t') ? TEXT(' ') : Message[Index];
			const bool NewLine = Character == TEXT('\n');
			if (NewLine || !Fits(Line + FString::Chr(Character)))
			{
				Lines.Add(Line);
				Line.Reset();
				if (Lines.Num() == MaxLines)
				{
					Lines.Last() = Ellipsize(Lines.Last());
					break;
				}
			}
			if (!NewLine) Line.AppendChar(Character);
		}
		if (!Line.IsEmpty() && Lines.Num() < MaxLines) Lines.Add(Line);
		if (Lines.IsEmpty()) return;
		Panel(28, PY, Width, 16.f + Lines.Num() * LineHeight, TBPaper::Cream);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
			Text(Lines[Index], 42, PY + 8.f + 10.f * FontScale + Index * LineHeight, FontScale, Ink);
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
	int32 ReadyCount = 0, PlayerCount = 0;
	for (APlayerState* Player : State->PlayerArray)
	{
		if (const auto* P = Cast<ATBPlayerState>(Player); P && !P->IsABot())
		{
			ReadyCount += P->bReady ? 1 : 0;
			++PlayerCount;
		}
	}
	const bool EveryoneReady = PlayerCount > 0 && ReadyCount == PlayerCount &&
	    PlayerCount == State->Settings.Humans + State->Settings.Toys;
	const bool EnoughPlayers = State->Settings.Humans + State->Settings.NPCHumans >= 1 &&
	    State->Settings.Toys + State->Settings.NPCToys >= 1;
	const bool CanStart = PC->HasAuthority() && EveryoneReady && EnoughPlayers && !State->bTravelPending;
	if (!bLobbySettingsOpen)
	{
		const float Bottom = (Canvas->ClipY - Y) / S - 84;
		Panel(28, Bottom - 40, 1144, 100, TBPaper::Cream);
		Text(!EnoughPlayers ? TEXT("にんげんとおもちゃの両方が必要です。設定で陣営かNPCを変更してください。")
		     : !EveryoneReady ? TEXT("全員の準備ができたら、はじめよう。")
		     : State->bTravelPending ? TEXT("ステージへ移動中…") : TEXT("準備がそろいました！"),
		     48, Bottom - 30, .9f, Ink);
		const FString ReadyKey = TB::KeyLabel(PC, TB::Keys::LobbyReady);
		const FString StartKey = TB::KeyLabel(PC, TB::Keys::LobbyStart);
		Button(TEXT("Ready"),
		       Local && Local->bReady ? FString::Printf(TEXT("準備をやめる [%s]"), *ReadyKey) : FString::Printf(TEXT("準備OK [%s]"), *ReadyKey),
		       48, Bottom, 240, Local != nullptr && !State->bTravelPending, Local && Local->bReady);
		Text(FString::Printf(TEXT("%d / %d人 準備OK"), ReadyCount, PlayerCount), 308, Bottom + 8, 1.05f, Ink);
		Button(TEXT("OpenSettings"), TEXT("設定 [Esc]"), 622, Bottom, 216);
		Button(TEXT("Start"),
		       PC->HasAuthority() ? FString::Printf(TEXT("あそびはじめる [%s]"), *StartKey) : FString(TEXT("ホストを待っています")),
		       858, Bottom, 294, CanStart, CanStart);
		DrawNotice(Bottom - 166, 1144, 110);
		const FString HairHint = Local && Local->Team == ETBTeam::Human
		    ? FString::Printf(TEXT("・%s"), *TB::KeyLabel(PC, TB::Keys::EmoteHairDisappear)) : FString();
		Text(FString::Printf(TEXT("移動キーで歩く / エモート %s・%s・%s・%s%s / Escで設定"),
		    *TB::KeyLabel(PC, TB::Keys::EmoteWave), *TB::KeyLabel(PC, TB::Keys::EmoteCheer),
		    *TB::KeyLabel(PC, TB::Keys::EmoteDance), *TB::KeyLabel(PC, TB::Keys::EmoteSurprise), *HairHint), 48, Bottom - 76, .8f, Ink);
		return;
	}
	DrawRect(FLinearColor(.009f, .018f, .027f, .48f), 0, 0, Canvas->ClipX, Canvas->ClipY);
	if (bStagePickerOpen)
	{
		Panel(280, 150, 640, 470, FLinearColor::White);
		Text(TEXT("ステージを選ぶ"), 312, 194, 1.6f, Accent);
		Text(TEXT("試合開始時に、選んだステージへ移動します。"), 312, 241, .95f, Muted);
		const int32 Current = State->SelectedStage;
		Button(TEXT("StagePlayroom"), TEXT("Stylized House"), 312, 284, 576, PC->HasAuthority(), Current == 3);
		Text(TEXT("家具のおうち"), 328, 330, .85f, Muted);
		Button(TEXT("StageExpanded"), TEXT("Demo ＋ こども部屋"), 312, 352, 576, PC->HasAuthority(), Current == 1);
		Text(TEXT("リビングと子ども部屋"), 328, 398, .85f, Muted);
		Button(TEXT("CancelStage"), TEXT("設定にもどる"), 654, 557, 234);
		DrawNotice(640, 1144, 100);
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
		if (!P || P->IsABot())
		{
			continue;
		}
		Ready += P->bReady ? 1 : 0;
		const FString PreferredRole = P->Preference == ETBTeam::Human ? TEXT("にんげん")
		                              : P->Preference == ETBTeam::Toy ? TEXT("おもちゃ")
		                                                              : TEXT("どちらでも");
		const float PY = 182 + Row++ * 21;
		Panel(40, PY - 4, 666, 20, FLinearColor(.10f, .15f, .18f, Row % 2 ? .42f : .18f));
		Panel(48, PY + 4, 4, 12, P->bReady ? Accent : Muted);
		Text(TB::Chat::DisplayName(P), 62, PY - 5, .8f, Ink);
		Text(PreferredRole, 320, PY - 5, .8f, Muted);
		Text(P->bReady ? TEXT("準備OK") : TEXT("準備中"), 455, PY - 5, .8f, P->bReady ? Accent : Muted);
		if (PC->HasAuthority() && P != Local)
		{
			Button(FName(*FString::Printf(TEXT("Kick_%d"), P->GetPlayerId())), TEXT("キック"),
			       610, PY - 4, 88, !State->bTravelPending, false, 20);
		}
	}
	const bool CanEditNPC = PC->HasAuthority() && !State->bTravelPending;
	const int32 NPCCount = State->Settings.NPCHumans + State->Settings.NPCToys;
	const bool CanAddNPC = State->Settings.Humans + State->Settings.Toys + NPCCount < State->PlayerLimit;
	Text(FString::Printf(TEXT("NPC（0人で参加なし・参加者と合わせて%d人まで）"), State->PlayerLimit), 48, 356, .85f, Muted);
	Text(FString::Printf(TEXT("にんげんNPC  %d人"), State->Settings.NPCHumans), 48, 390, .95f, Ink);
	Button(TEXT("FewerNPCHumans"), TEXT("-"), 250, 382, 50, CanEditNPC && State->Settings.NPCHumans > 0, false, 36);
	Button(TEXT("MoreNPCHumans"), TEXT("+"), 308, 382, 50, CanEditNPC && CanAddNPC, false, 36);
	Text(FString::Printf(TEXT("おもちゃNPC  %d体"), State->Settings.NPCToys), 380, 390, .95f, Ink);
	Button(TEXT("FewerNPCToys"), TEXT("-"), 580, 382, 50, CanEditNPC && State->Settings.NPCToys > 0, false, 36);
	Button(TEXT("MoreNPCToys"), TEXT("+"), 638, 382, 50, CanEditNPC && CanAddNPC, false, 36);
	Text(TEXT("NPCは自動で準備完了になります。"), 48, 430, .8f, Muted);
	Panel(738, 130, 434, 360, FLinearColor::White);
	Text(TEXT("今回のあそび"), 758, 146, 1.25f, Accent);
	const bool Host = PC->HasAuthority();
	Button(TEXT("ChangeStage"), State->StageName + TEXT("  ／ 選択"), 758, 180, 392, Host);
	// 刻みはマスターデータで変えられるので、分で割り切れないときは秒も出す。
	const int32 Seconds = FMath::RoundToInt(State->Settings.Duration);
	const FString Limit = Seconds % 60 == 0 ? FString::Printf(TEXT("制限時間  %d分"), Seconds / 60)
	                                        : FString::Printf(TEXT("制限時間  %d分%d秒"), Seconds / 60, Seconds % 60);
	Text(Limit, 758, 245, 1.1f, Ink);
	const auto& Match = TB::Rules().Match;
	Button(TEXT("LessTime"), TEXT("-"), 1032, 235, 52, Host && State->Settings.Duration > Match.MinDuration);
	Button(TEXT("MoreTime"), TEXT("+"), 1098, 235, 52, Host && State->Settings.Duration < Match.MaxDuration);
	Text(FString::Printf(TEXT("集める電池  %d個"), State->Settings.RequiredItems), 758, 301, 1.1f, Ink);
	Button(TEXT("FewerBatteries"), TEXT("-"), 1032, 291, 52, Host && State->Settings.RequiredItems > 1);
	Button(TEXT("MoreBatteries"), TEXT("+"), 1098, 291, 52,
	       Host && State->Settings.RequiredItems < TBLobbySettings::AvailableItems(GetWorld()));
	Button(TEXT("FewerToys"), TEXT("- おもちゃ"), 758, 350, 184, Host && State->Settings.Toys > (State->Settings.NPCToys > 0 ? 0 : 1));
	Button(TEXT("MoreToys"), TEXT("+ おもちゃ"), 956, 350, 194,
	       Host && State->Settings.Humans > (State->Settings.NPCHumans > 0 ? 0 : 1));
	Text(FString::Printf(TEXT("参加者：にんげん %d人 ／ おもちゃ %d体"), State->Settings.Humans, State->Settings.Toys), 758,
	     405, .95f, Ink);
	Text(FString::Printf(TEXT("%d / %d人 準備OK"), Ready, State->Settings.Humans + State->Settings.Toys), 758, 430,
	     1.25f, Accent);
	Text(TEXT("どちらであそぶ？"), 28, 514, 1.f, TBPaper::Cream);
	Button(TEXT("Child"), TEXT("にんげん"), 28, 542, 190, true, Local && Local->Preference == ETBTeam::Human);
	Button(TEXT("Toy"), TEXT("おもちゃ"), 230, 542, 190, true, Local && Local->Preference == ETBTeam::Toy);
	Button(TEXT("Either"), TEXT("どちらでも"), 432, 542, 180, true, Local && Local->Preference == ETBTeam::None);
	const FString ReadyKey = TB::KeyLabel(PC, TB::Keys::LobbyReady);
	const FString StartKey = TB::KeyLabel(PC, TB::Keys::LobbyStart);
	Button(TEXT("Ready"),
	       Local && Local->bReady ? FString::Printf(TEXT("準備をやめる [%s]"), *ReadyKey) : FString::Printf(TEXT("準備OK [%s]"), *ReadyKey),
	       632, 542, 240, Local != nullptr, Local && Local->bReady);
	Button(TEXT("Start"), Host ? FString::Printf(TEXT("あそびはじめる [%s]"), *StartKey) : FString(TEXT("ホストを待っています")),
	       890, 542, 282, CanStart, CanStart);
	if (!EveryoneReady || !EnoughPlayers)
	{
		Text(!EnoughPlayers
		         ? TEXT("両陣営をそろえてください。NPCとも遊べます。")
		         : TEXT("全員の準備ができたら、はじめよう。"),
		     48, 456, .85f, Muted);
	}
	Button(TEXT("OpenOptions"), TEXT("オプション"), 700, 600, 200);
	Button(TEXT("Leave"), Host ? TEXT("部屋を閉じてタイトルへ") : TEXT("部屋を出てタイトルへ"), 922, 600, 250);
	DrawNotice(592, 650, 120, true);
	Text(TEXT("おもちゃ：電池を集めて仲間を救助 ／ にんげん：おもちゃを見つけて箱へ"), 28, 722, .9f, TBPaper::Cream);
}

void ATBHUD::DrawResults(const ATBGameState& MatchState)
{
	const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	const float X = Canvas->ClipX * .5f - 300 * S;
	const bool ShowError = !MatchState.bTravelPending && !MatchState.LobbyReturnError.IsEmpty();
	PaperPanel(FLinearColor::White, X, 24 * S, 600 * S, (ShowError ? 150 : 100) * S);
	const auto* ResultPlayer = GetOwningPlayerController()->GetPlayerState<ATBPlayerState>();
	const FString Title = MatchState.Phase == ETBPhase::Aborted ? TEXT("あそびはおしまい") :
	    MatchState.bVehicleEscapeResult && ResultPlayer && ResultPlayer->Team == ETBTeam::Toy ?
	        (MatchState.EscapedInVehicle(ResultPlayer) ? TEXT("脱出成功！") : TEXT("脱出できなかった")) :
	    MatchState.Winner == ETBWinner::Toys ? TEXT("おもちゃの勝ち！") : TEXT("にんげんの勝ち！");
	DrawText(Title, TBPaper::Accent, X + 24 * S, 36 * S, PaperFont, 1.8f * S);
	const int32 Seconds = FMath::CeilToInt(FMath::Max(0., MatchState.EndTime - MatchState.GetServerWorldTimeSeconds()));
	if (ShowError)
	{
		DrawText(MatchState.LobbyReturnError, TBPaper::Ink, X + 24 * S, 85 * S, PaperFont, S);
		if (MatchState.IsLobbyReturnStopped() && GetOwningPlayerController()->HasAuthority())
		{
			PaperPanel(TBPaper::Cream, X + 24 * S, 117 * S, 280 * S, 44 * S);
			DrawText(TEXT("ロビー復帰を再試行"), TBPaper::Ink, X + 38 * S, 125 * S, PaperFont, S);
			AddHitBox(FVector2D(X + 24 * S, 117 * S), FVector2D(280 * S, 44 * S), TEXT("RetryLobbyReturn"), true);
		}
		else
		{
			const FString Status = MatchState.IsLobbyReturnStopped() ? TEXT("ホストの再試行を待っています") :
			    FString::Printf(TEXT("あと%d秒で再試行します"), Seconds);
			DrawText(Status, TBPaper::Ink, X + 24 * S, 125 * S, PaperFont, S);
		}
		return;
	}
	const FString Status = MatchState.bTravelPending ? TEXT("ロビーに移動しています") :
	    FString::Printf(TEXT("あと%d秒でみんなでロビーへ"), Seconds);
	DrawText(Status, TBPaper::Ink, X + 24 * S, 85 * S, PaperFont, S);
}

void ATBHUD::DrawMatch(const ATBGameState& MatchState, ATBController& Controller)
{
	const auto* State = &MatchState;
	auto* PC = &Controller;
	const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const FLinearColor Ink = TBPaper::Ink, Accent = TBPaper::Accent, Panel = FLinearColor::White;
	const auto* Player = Controller.GetPlayerState<ATBPlayerState>();
	if (MatchState.bEscapePhase && !Controller.ShouldHidePreparationView())
		DrawText(Player && Player->Team == ETBTeam::Human ? TEXT("電池が集まった！脱出を阻止しよう！") :
		    TEXT("電池が集まった！車で出口へ脱出しよう！"), TBPaper::Accent, 300 * S, 110 * S, PaperFont, 1.2f * S);
	if (Controller.ShouldHidePreparationView())
	{
		if (!Controller.IsShowingEntranceApproach()) DrawRect(FLinearColor::Black, 0, 0, W, H);
		const FString Wait = Player && Player->Team == ETBTeam::Human && MatchState.HidePreparation.bValid && MatchState.HidePreparation.bHiding
		    ? FString::Printf(TEXT("にんげん：おもちゃが隠れるまで待とう　あと%d秒"), FMath::CeilToInt(MatchState.HideRemaining()))
		    : TEXT("あそぶ準備をしています…");
		if (Controller.IsShowingEntranceApproach())
		{
			PaperPanel(Panel, W * .5f - 300 * S, H - 130 * S, 600 * S, 75 * S);
			DrawText(FString::Printf(TEXT("にんげん：片付けへ　あと%d秒"), FMath::CeilToInt(MatchState.HideRemaining())),
			         Ink, W * .5f - 265 * S, H - 100 * S, PaperFont, 1.2f * S);
		}
		else DrawText(Wait, TBPaper::Cream, W * .5f - 220 * S, H * .5f, PaperFont, 1.3f * S);
		return;
	}
	DrawGazeEffects(Controller);
	if (MatchState.IsPreparing())
		DrawText(FString::Printf(TEXT("隠れよう！ あと%d秒"), FMath::CeilToInt(MatchState.HideRemaining())),
		         TBPaper::Accent, W * .5f - 145 * S, 115 * S, PaperFont, 1.3f * S);
	const bool Child = Player && Player->Team == ETBTeam::Human;
	if (const auto* Mom = ATBMomArrivalZone::FindActive(GetWorld()); Mom && !MatchState.IsPreparing())
	{
		const auto& Event = Mom->State;
		const FLinearColor Warning(1.f,.03f,.025f,1.f);
		PaperPanel(Panel,W*.5f-290*S,112*S,580*S,(Child ? 82 : 48)*S);
		DrawText(FString::Printf(TEXT("ママが来るまで%d秒！！"),FMath::CeilToInt(Event.Remaining)),
		         Warning,W*.5f-265*S,122*S,PaperFont,1.5f*S);
		if (Child)
		{
			DrawText(TEXT("指定位置に向かいママを説得しよう！"),Warning,W*.5f-265*S,157*S,PaperFont,1.1f*S);
			if (Event.bOccupied)
			{
				// 箱にしまう際と同じパネル・ゲージ寸法を使う。
				PaperPanel(Panel,W*.5f-235*S,H*.64f,470*S,64*S);
				DrawText(FString::Printf(TEXT("説得中… %.1f / %.1f秒"),Event.Held,Event.RequiredHold),
				         Ink,W*.5f-220*S,H*.64f+9*S,PaperFont,S);
				DrawRect(FLinearColor(.1f,.14f,.16f,1),W*.5f-220*S,H*.64f+37*S,440*S,7*S);
				DrawRect(Accent,W*.5f-220*S,H*.64f+37*S,440*S*FMath::Clamp(Event.Held/FMath::Max(.1f,Event.RequiredHold),0.f,1.f),7*S);
				DrawText(TEXT("到着カウント停止中 ／ 全員が範囲を出ると説得をやり直します"),
				         Ink,W*.5f-220*S,H*.64f+49*S,PaperFont,.7f*S);
			}
		}
	}
	if (Player && Player->Team == ETBTeam::Toy && Player->ToyState == ETBToyState::Boxed)
	{
		if (const auto* Course = ATBBoxEscapeCourse::Find(GetWorld(), MatchState.Box))
		{
			DrawText(TEXT("箱の高いところへ登って脱出しよう"),
			         Accent, 24 * S, H * .65f, PaperFont, 1.1f * S);
		}
	}
	PaperPanel(Panel, 24 * S, 24 * S, 310 * S, 77 * S);
	DrawText(Child ? TEXT("にんげん") : TEXT("おもちゃ"), Accent, 40 * S, 35 * S, PaperFont, 1.4f * S);
	const TCHAR* Objective = Child ? TEXT("おもちゃを見つけて、箱へ。") : TEXT("電池を集めて、仲間を救助。");
	if (MatchState.IsPreparing() && Player && Player->Team == ETBTeam::Toy)
		Objective = TEXT("物かげに隠れよう。電池は開始後！");
	FTBEmoteSample Emote;
	if (const auto* EmoteCharacter = Controller.GetPawn<ATBCharacter>(); EmoteCharacter && EmoteCharacter->GetEmoteSample(Emote))
		Objective = Emote.Kind == ETBEmoteKind::Wave ? TEXT("手ふり中。移動するとやめます。") :
		    Emote.Kind == ETBEmoteKind::Cheer ? TEXT("よろこび中。移動するとやめます。") :
		    Emote.Kind == ETBEmoteKind::HairDisappear ? TEXT("髪かくし中。移動するとやめます。") :
		    Emote.Kind == ETBEmoteKind::Surprise ? TEXT("びっくり中。移動するとやめます。") : TEXT("ダンス中。移動するとやめます。");
	if (Player && Player->Team == ETBTeam::Toy)
	{
		switch (Player->ToyState)
		{
			case ETBToyState::Grabbed:
				Objective = TEXT("捕まった！ 持ち上げられています。");
				break;
			case ETBToyState::Carried:
				Objective = TEXT("運ばれています。今は動けません。");
				break;
			case ETBToyState::Storing:
				Objective = TEXT("箱にしまわれています…");
				break;
			case ETBToyState::Boxed:
				Objective = ATBBoxEscapeCourse::Find(GetWorld(), MatchState.Box)
				    ? TEXT("上へ登って脱出。仲間の救助も！") : TEXT("箱のなか。仲間の救助を待とう。");
				break;
			case ETBToyState::Free:
				break;
		}
	}
	float ObjectiveWidth = 0, ObjectiveHeight = 0;
	GetTextSize(Objective, ObjectiveWidth, ObjectiveHeight, PaperFont, 1.f);
	DrawText(Objective, Ink, 40 * S, 67 * S, PaperFont,
	         FMath::Min(.95f, 278.f / FMath::Max(ObjectiveWidth, 1.f)) * S);
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(MatchState.Remaining()));
	PaperPanel(Panel, W * .5f - 64 * S, 24 * S, 128 * S, 56 * S);
	DrawText(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60), Seconds < 60 ? Accent : Ink,
	         W * .5f - 42 * S, 36 * S, PaperFont, 2.f * S);
	PaperPanel(Panel, W - 286 * S, 24 * S, 262 * S, 77 * S);
	DrawText(FString::Printf(TEXT("電池  %d / %d"), MatchState.Collected, MatchState.Settings.RequiredItems), Ink,
	         W - 270 * S, 36 * S, PaperFont, 1.35f * S);
	DrawText(FString::Printf(TEXT("箱のなか  %d / %d"), MatchState.BoxedCount, MatchState.Settings.Toys + MatchState.Settings.NPCToys), Accent,
	         W - 270 * S, 68 * S, PaperFont, .95f * S);
	PaperPanel(Panel, 24 * S, H - 62 * S, (Child ? 900 : 700) * S, 38 * S);
	DrawText(TB::MatchHintText(PC, Child), Ink, 40 * S, H - 51 * S, PaperFont, .95f * S);
	if (Player && Player->bFrozen)
	{
		// 赤で怖がらせず、紙の札で知らせる。止まった間の色はセピアの画面が伝える。
		PaperPanel(TBPaper::Cream, W * .5f - 165 * S, H * .7f, 330 * S, 44 * S);
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
		const float Duration = FMath::Max(TB::Rules().Box.StoreSeconds, .01f);
		const float Elapsed =
		    Storing
		        ? FMath::Clamp(float(MatchState.GetServerWorldTimeSeconds() - Character->StoreStarted), 0.f, Duration)
		        : 0.f;
		const FString InteractKey = TB::KeyLabel(PC, TB::Keys::HumanInteract);
		const FString Prompt = Storing
		                           ? FString::Printf(TEXT("%s 長押し  しまっています… %.1f / %.0f秒"), *InteractKey, Elapsed, Duration)
		                           : (NearBox ? FString::Printf(TEXT("%sを%.0f秒長押しして箱にしまう"), *InteractKey, Duration)
		                                      : TEXT("おもちゃを運搬中。箱の正面へ。"));
		PaperPanel(Panel, W * .5f - 235 * S, H * .76f, 470 * S, 64 * S);
		DrawText(Prompt, Ink, W * .5f - 220 * S, H * .76f + 9 * S, PaperFont, 1.f * S);
		DrawRect(FLinearColor(.1f, .14f, .16f, 1), W * .5f - 220 * S, H * .76f + 37 * S, 440 * S, 7 * S);
		DrawRect(Accent, W * .5f - 220 * S, H * .76f + 37 * S, 440 * S * Elapsed / Duration, 7 * S);
		DrawText(FString::Printf(TEXT("%sを離すと中止 ／ %sでおろす"), *InteractKey, *TB::KeyLabel(PC, TB::Keys::HumanDrop)), Ink, W * .5f - 220 * S, H * .76f + 49 * S, PaperFont, .75f * S);
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
