// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "TBHUD.h"
#include "TBPaperStyle.h"
#include "Audio/TBAudioSubsystem.h"
#include "Character/TBCharacter.h"
#include "Core/TBController.h"
#include "Core/TBPlayerState.h"
#include "GamePlay/TBRuleMath.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"

namespace
{
	// 紙の影の色。赤や黒で怖がらせず、茶色い紙をかぶせたように見せる。
	const FLinearColor PaperShadow = FLinearColor::FromSRGBColor(FColor(92, 64, 38));
	constexpr double DokiSeconds = .8;
}

// 見られ方はおもちゃ本人のPCにしか届かないので、相手の画面には何も出ない。
void ATBHUD::DrawGazeEffects(ATBController& Controller)
{
	const float W = Canvas->ClipX, H = Canvas->ClipY, S = FMath::Min(W / 1280.f, H / 800.f);
	const float Short = FMath::Min(W, H);
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	const double Now = GetWorld()->GetTimeSeconds();
	const auto* Pawn = Controller.GetPawn<ATBCharacter>();
	const auto* Player = Controller.GetPlayerState<ATBPlayerState>();
	using TBRuleMath::EGazeState;
	EGazeState State = EGazeState::None;
	if (Pawn && Player && Player->Team == ETBTeam::Toy)
	{
		State = Pawn->IsGazeFrozen() ? EGazeState::Frozen
		        : Pawn->IsGazed()    ? EGazeState::Gazed
		        : Pawn->IsSeen()     ? EGazeState::Seen
		                             : EGazeState::None;
	}
	// おもちゃ: 見られている間は端にうっすら、凝視されたら一気に影が寄る。
	const bool Gazed = State == EGazeState::Gazed;
	GazeShadow = TBRuleMath::AdvanceShade(GazeShadow, TBRuleMath::GazeShadowTarget(State), DeltaSeconds,
	                                      Gazed ? .12f : .5f, .4f);
	if (GazeShadow > 0.f)
	{
		DrawEdgeShade(PaperShadow, Short * .3f * GazeShadow, .8f * GazeShadow);
	}
	if (Gazed && !bWasGazed)
	{
		DokiStarted = Now;
		if (auto* Audio = GetGameInstance()->GetSubsystem<UTBAudioSubsystem>())
		{
			Audio->PlaySoundEvent(ETBSoundEvent::GazeAlert);
		}
	}
	bWasGazed = Gazed;
	// 「ドキッ」は飛び出して少し震え、薄れて消える。
	if (DokiStarted >= 0 && Now - DokiStarted < DokiSeconds)
	{
		const float T = float(Now - DokiStarted);
		const float Scale = 3.2f * S * (1.f + .4f * FMath::Max(0.f, 1.f - T / .12f));
		const float Shake = FMath::Sin(T * 70.f) * 5.f * S * FMath::Max(0.f, 1.f - T / .4f);
		const float Fade = FMath::Clamp(float(DokiSeconds - T) / .3f, 0.f, 1.f);
		float TextW = 0, TextH = 0;
		GetTextSize(TEXT("ドキッ"), TextW, TextH, PaperFont, Scale);
		// 暗い部屋でも読めるよう、紙の札に書く。
		const float PadX = 22.f * S, PadY = 10.f * S;
		const float X = W * .5f - TextW * .5f + Shake, Y = H * .26f;
		FLinearColor Paper = TBPaper::Cream, Ink = TBPaper::Ink;
		Paper.A = Ink.A = Fade;
		PaperPanel(Paper, X - PadX, Y - PadY, TextW + PadX * 2, TextH * .8f + PadY * 2);
		DrawText(TEXT("ドキッ"), Ink, X, Y, PaperFont, Scale);
	}
	// 人間: 凝視中は端を暗くして見える範囲を絞り、中心へ向かう集中線を薄く出す。
	if (Pawn && Pawn->IsHuman())
	{
		const float Zoom = FMath::SmoothStep(0.f, 1.f, Pawn->GetGazeZoom());
		if (Zoom > 0.f)
		{
			DrawEdgeShade(FLinearColor(.02f, .02f, .03f), Short * .32f * Zoom, .6f * Zoom);
			// 線は少しずつ入れ替えて、止まった絵に見せない。
			FRandomStream Lines(int32(Now / .07));
			// 端だけに短く出し、中心の見たいものには重ねない。
			const float HalfDiagonal = FMath::Sqrt(W * W + H * H) * .5f;
			FLinearColor Color = TBPaper::Cream;
			Color.A = .16f * Zoom;
			constexpr int32 Count = 28;
			for (int32 I = 0; I < Count; ++I)
			{
				const float Angle = (I + Lines.FRandRange(-.35f, .35f)) * UE_TWO_PI / Count;
				const float Inner = FMath::Lerp(HalfDiagonal, HalfDiagonal * Lines.FRandRange(.62f, .8f), Zoom);
				const float CX = FMath::Cos(Angle), SY = FMath::Sin(Angle);
				DrawLine(W * .5f + CX * HalfDiagonal, H * .5f + SY * HalfDiagonal, W * .5f + CX * Inner,
				         H * .5f + SY * Inner, Color, Lines.FRandRange(.8f, 1.6f) * S);
			}
		}
	}
}

// 画面の四辺から内側へ、外ほど濃い帯を重ねて柔らかい影にする。
void ATBHUD::DrawEdgeShade(FLinearColor Color, float Thickness, float Alpha)
{
	constexpr int32 Bands = 14;
	const float W = Canvas->ClipX, H = Canvas->ClipY, Band = Thickness / Bands;
	for (int32 I = 0; I < Bands; ++I)
	{
		const float Inset = Band * I;
		Color.A = Alpha * FMath::Square(1.f - float(I) / Bands);
		DrawRect(Color, 0, Inset, W, Band);
		DrawRect(Color, 0, H - Inset - Band, W, Band);
		DrawRect(Color, Inset, 0, Band, H);
		DrawRect(Color, W - Inset - Band, 0, Band, H);
	}
}
