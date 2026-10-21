// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBGameUserSettings.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "TBOptionRow.h"
#include "TBOptionsMenu.h"
#include "TBOptionsPages.h"

// 足音は効果音に含める（足音だけを上げると索敵が有利になるため）。
void TBOptionsPages::BuildAudio(UTBOptionsMenu& Menu, UTBGameUserSettings& Settings)
{
	auto* S = &Settings;
	auto* M = &Menu;
	const TCHAR* VolumeNames[] = {TEXT("全体"), TEXT("BGM"), TEXT("効果音"), TEXT("ボイスチャット")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(VolumeNames); ++Index)
	{
		const auto Kind = ETBVolume(Index);
		auto* Row = Menu.AddRow(TBOptionsPage::Audio, VolumeNames[Index], true);
		Row->Step = [M, S, Kind](int32 Steps)
		{
			S->StepVolume(Kind, Steps);
			S->CommitVolumes(M->GetOwningPlayer() ? M->GetOwningPlayer()->GetGameInstance() : nullptr);
		};
		Row->Value = [S, Kind] { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S->GetVolume(Kind) * 100)); };
		Row->Fill = [S, Kind] { return S->GetVolume(Kind); };
		Row->CanStep = [S, Kind](int32 Steps)
		{
			const auto& Range = TB::ConfigData().Audio.Get(Kind);
			const float Volume = S->GetVolume(Kind);
			return Steps < 0 ? Volume > Range.Clamp(0.f) + KINDA_SMALL_NUMBER : Volume < Range.Clamp(1.f) - KINDA_SMALL_NUMBER;
		};
	}
}
