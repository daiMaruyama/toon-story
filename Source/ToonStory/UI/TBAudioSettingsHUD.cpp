// Copyright (c) 2026 Mamodou. All Rights Reserved.
#include "TBHUD.h"
#include "TBPaperStyle.h"
#include "Audio/TBAudioSubsystem.h"
#include "Core/TBGameUserSettings.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

namespace
{
	struct FVolumeRow
	{
		ETBVolume Kind;
		const TCHAR* Label;
	};
	const FVolumeRow VolumeRows[] = {{ETBVolume::Master, TEXT("全体")},
	                                 {ETBVolume::Music, TEXT("BGM")},
	                                 {ETBVolume::Effects, TEXT("効果音")},
	                                 {ETBVolume::Voice, TEXT("ボイスチャット")}};

	FName VolumeButton(ETBVolume Kind, int32 Steps)
	{
		return FName(*FString::Printf(TEXT("Volume%s_%d"), Steps > 0 ? TEXT("Up") : TEXT("Down"), int32(Kind)));
	}
}

// ゲーム中のメニューとロビーの設定から開く。タイトルの「おとの設定」と同じ項目・同じ操作にする。
bool ATBHUD::HandleAudioClick(FName Name)
{
	auto* Settings = UTBGameUserSettings::Get();
	auto* PC = GetOwningPlayerController();
	if (!Settings || !PC || (Name != TEXT("OpenAudio") && !bAudioOpen))
	{
		return false;
	}
	if (Name == TEXT("OpenAudio") || Name == TEXT("CloseAudio"))
	{
		bAudioOpen = Name == TEXT("OpenAudio");
	}
	else if (Name == TEXT("ResetAudio"))
	{
		Settings->ResetVolumes();
		Settings->CommitVolumes(PC->GetGameInstance());
	}
	for (const auto& Row : VolumeRows)
	{
		for (const int32 Steps : {-1, 1})
		{
			if (Name == VolumeButton(Row.Kind, Steps))
			{
				Settings->StepVolume(Row.Kind, Steps);
				Settings->CommitVolumes(PC->GetGameInstance());
			}
		}
	}
	// 変えたあとの音量で鳴らして、聞こえ方を確かめられるようにする。
	if (auto* Audio = PC->GetGameInstance()->GetSubsystem<UTBAudioSubsystem>())
	{
		Audio->PlaySoundEvent(Name == TEXT("CloseAudio") ? ETBSoundEvent::UIBack : ETBSoundEvent::UIClick);
	}
	return true;
}

void ATBHUD::DrawAudioSettings()
{
	const auto* Settings = UTBGameUserSettings::Get();
	if (!Settings)
	{
		bAudioOpen = false;
		return;
	}
	const float S = FMath::Min(Canvas->ClipX / 1280.f, Canvas->ClipY / 800.f);
	const float X = (Canvas->ClipX - 660 * S) * .5f, Y = (Canvas->ClipY - 470 * S) * .5f;
	DrawRect(FLinearColor(0, 0, 0, .6f), 0, 0, Canvas->ClipX, Canvas->ClipY);
	PaperPanel(TBPaper::Cream, X, Y, 660 * S, 470 * S);
	auto Text = [&](const FString& Value, float PX, float PY, float Scale = 1.f, FLinearColor Color = TBPaper::Ink)
	{ DrawText(Value, Color, X + PX * S, Y + PY * S, PaperFont, Scale * S); };
	auto Button = [&](FName Id, const FString& Label, float PX, float PY, float W, bool Enabled = true)
	{
		PaperPanel(Enabled ? FLinearColor::White : FLinearColor(.78f, .78f, .75f), X + PX * S, Y + PY * S, W * S, 42 * S);
		Text(Label, PX + 12, PY + 7, .9f, Enabled ? TBPaper::Ink : TBPaper::Muted);
		if (Enabled)
		{
			AddHitBox(FVector2D(X + PX * S, Y + PY * S), FVector2D(W * S, 42 * S), Id, true);
		}
	};
	Text(TEXT("おとの設定"), 30, 24, 1.5f, TBPaper::Accent);
	Text(TEXT("変えた音量はすぐに反映され、次からもこの音量になります。"), 30, 72, .8f, TBPaper::Muted);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(VolumeRows); ++Index)
	{
		const auto& Row = VolumeRows[Index];
		const auto& Range = TB::ConfigData().Audio.Get(Row.Kind);
		const float Volume = Settings->GetVolume(Row.Kind);
		const float PY = 120 + Index * 62;
		Text(Row.Label, 30, PY + 8);
		Button(VolumeButton(Row.Kind, -1), TEXT("-"), 210, PY, 56, Volume > Range.Clamp(0.f) + KINDA_SMALL_NUMBER);
		DrawRect(FLinearColor(.78f, .78f, .75f), X + 284 * S, Y + (PY + 13) * S, 220 * S, 16 * S);
		DrawRect(TBPaper::Accent, X + 284 * S, Y + (PY + 13) * S, 220 * S * Volume, 16 * S);
		Text(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Volume * 100)), 516, PY + 8, .9f);
		Button(VolumeButton(Row.Kind, 1), TEXT("+"), 578, PY, 56, Volume < Range.Clamp(1.f) - KINDA_SMALL_NUMBER);
	}
	Button(TEXT("ResetAudio"), TEXT("初期設定に戻す"), 30, 390, 290);
	Button(TEXT("CloseAudio"), TEXT("もどる"), 340, 390, 290);
}
