// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Input/TBKeyLabels.h"
#include "Input/TBInputSettings.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
	// 印の中身がこれより長ければ、印とみなさない。
	constexpr int32 MaxTokenLength = 64;

	// UserSettingsがまだ引けないとき（接続直後など）のために、IMCに書いた既定のキー（パッドでないもの）を返す。
	FKey DefaultKeyboardKey(FName MappingName)
	{
		for (const UInputMappingContext* Context : GetDefault<UTBInputSettings>()->GetAllContexts())
		{
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				const UPlayerMappableKeySettings* Settings = Mapping.GetPlayerMappableKeySettings();
				if (Settings && Settings->Name == MappingName && !Mapping.Key.IsGamepadKey())
				{
					return Mapping.Key;
				}
			}
		}
		return EKeys::Invalid;
	}
}

const TArray<FName>& TB::Keys::All()
{
	static const TArray<FName> Names = {MoveForward, MoveBack, MoveLeft, MoveRight, Jump, HumanCrouch, HumanInteract,
	                                    HumanDrop, ToySprint, ToyInteract, ToyTaunt, LobbyReady, LobbyStart};
	return Names;
}

const TArray<TB::FKeySection>& TB::KeySections()
{
	static const TArray<FKeySection> Sections = {
	    {TEXT("共通"),
	     {{Keys::MoveForward, TEXT("前へ")},
	      {Keys::MoveBack, TEXT("後ろへ")},
	      {Keys::MoveLeft, TEXT("左へ")},
	      {Keys::MoveRight, TEXT("右へ")},
	      {Keys::Jump, TEXT("ジャンプ")}}},
	    {TEXT("にんげん"),
	     {{Keys::HumanCrouch, TEXT("しゃがむ")},
	      {Keys::HumanInteract, TEXT("つかむ・収納")},
	      {Keys::HumanDrop, TEXT("おろす")}}},
	    {TEXT("おもちゃ"),
	     {{Keys::ToySprint, TEXT("ダッシュ")},
	      {Keys::ToyInteract, TEXT("救助")},
	      {Keys::ToyTaunt, TEXT("音を鳴らす")}}},
	    {TEXT("ロビー"), {{Keys::LobbyReady, TEXT("準備")}, {Keys::LobbyStart, TEXT("開始")}}},
	};
	return Sections;
}

FString TB::FormatKey(const FKey& Key)
{
	if (!Key.IsValid())
	{
		return TEXT("未割り当て");
	}
	if (Key == EKeys::SpaceBar)
	{
		return TEXT("Space");
	}
	if (Key == EKeys::LeftShift)
	{
		return TEXT("Shift");
	}
	if (Key == EKeys::RightShift)
	{
		return TEXT("右Shift");
	}
	if (Key == EKeys::LeftControl)
	{
		return TEXT("Ctrl");
	}
	if (Key == EKeys::RightControl)
	{
		return TEXT("右Ctrl");
	}
	if (Key == EKeys::LeftAlt)
	{
		return TEXT("Alt");
	}
	if (Key == EKeys::RightAlt)
	{
		return TEXT("右Alt");
	}
	return Key.GetDisplayName(false).ToString();
}

// FindCurrentMappingForSlotとFindMappingsInRowは、行を作る副作用や警告があるので使わない。
// キーボード側かどうかは今のキーではなく既定のキーで決める（外した枠もキーボード側のまま）。
bool TB::FindKeyboardKey(const APlayerController* PC, FName MappingName, FKeyboardKey& Out)
{
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	const auto* Input = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	const auto* User = Input ? Input->GetUserSettings() : nullptr;
	const auto* Profile = User ? User->GetActiveKeyProfile() : nullptr;
	const FKeyMappingRow* Row = Profile ? Profile->GetPlayerMappingRows().Find(MappingName) : nullptr;
	if (!Row)
	{
		return false;
	}
	const FPlayerKeyMapping* Best = nullptr;
	const FPlayerKeyMapping* Empty = nullptr;
	for (const FPlayerKeyMapping& Mapping : Row->Mappings)
	{
		if (Mapping.GetDefaultKey().IsGamepadKey())
		{
			continue;
		}
		const FPlayerKeyMapping*& Slot = Mapping.GetCurrentKey().IsValid() ? Best : Empty;
		if (!Slot || Mapping.GetSlot() < Slot->GetSlot())
		{
			Slot = &Mapping;
		}
	}
	const FPlayerKeyMapping* Found = Best ? Best : Empty;
	if (!Found)
	{
		return false;
	}
	Out.Key = Found->GetCurrentKey();
	Out.DefaultKey = Found->GetDefaultKey();
	Out.Slot = static_cast<int32>(Found->GetSlot());
	Out.DeviceId = Found->GetHardwareDeviceId().HardwareDeviceIdentifier;
	return true;
}

FString TB::KeyLabel(const APlayerController* PC, FName MappingName)
{
	FKeyboardKey Row;
	if (FindKeyboardKey(PC, MappingName, Row))
	{
		return FormatKey(Row.Key);
	}
	const FKey Default = DefaultKeyboardKey(MappingName);
	return Default.IsValid() ? FormatKey(Default) : FString(TEXT("?"));
}

FString TB::MoveKeysLabel(const APlayerController* PC)
{
	const FString Labels[] = {KeyLabel(PC, Keys::MoveForward), KeyLabel(PC, Keys::MoveLeft), KeyLabel(PC, Keys::MoveBack),
	                          KeyLabel(PC, Keys::MoveRight)};
	bool bAllSingle = true;
	bool bAllUnknown = true;
	for (const FString& Label : Labels)
	{
		bAllSingle &= Label.Len() == 1;
		bAllUnknown &= Label == TEXT("?");
	}
	// どれも引けないときは「????」ではなく「?」にする。
	return bAllUnknown ? FString(TEXT("?")) : FString::Join(Labels, bAllSingle ? TEXT("") : TEXT("/"));
}

FString TB::ExpandKeyTokens(const APlayerController* PC, const FString& Text)
{
	FString Result;
	int32 Pos = 0;
	while (Pos < Text.Len())
	{
		const int32 Close = Text.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
		if (Close == INDEX_NONE)
		{
			break;
		}
		// }の手前で一番近い{を名前の始まりにする。{{A}は{とAの印。
		int32 Open = INDEX_NONE;
		for (int32 Index = Close - 1; Index >= Pos; --Index)
		{
			if (Text[Index] == TEXT('{'))
			{
				Open = Index;
				break;
			}
		}
		if (Open == INDEX_NONE)
		{
			Result += Text.Mid(Pos, Close + 1 - Pos);
			Pos = Close + 1;
			continue;
		}
		// 名前テーブルを増やさないよう、既にある名前だけを引く。長すぎる中身は印とみなさない。
		const int32 NameLength = Close - Open - 1;
		const bool bCandidate = NameLength > 0 && NameLength <= MaxTokenLength;
		const FString NameText = bCandidate ? Text.Mid(Open + 1, NameLength) : FString();
		const FName Name = bCandidate ? FName(*NameText, FNAME_Find) : FName();
		Result += Text.Mid(Pos, Open - Pos);
		if (Keys::All().Contains(Name))
		{
			Result += KeyLabel(PC, Name);
		}
		else
		{
			// 知らない名前は{}ごと残す。スペルミスに気づけるよう、開発中はWarningを出す。
			UE_CLOG(bCandidate, LogTemp, Warning, TEXT("[ToonStoryInput] Unknown key token {%s} was left as is."), *NameText);
			Result += Text.Mid(Open, Close - Open + 1);
		}
		Pos = Close + 1;
	}
	return Result + Text.Mid(Pos);
}

FString TB::MatchHintText(const APlayerController* PC, bool bHuman)
{
	const FString Move = MoveKeysLabel(PC), Jump = KeyLabel(PC, Keys::Jump);
	if (bHuman)
	{
		return FString::Printf(TEXT("%s 移動 ／ %s しゃがむ ／ %s ジャンプ ／ %s つかむ・収納 ／ %s おろす"), *Move,
		                       *KeyLabel(PC, Keys::HumanCrouch), *Jump, *KeyLabel(PC, Keys::HumanInteract),
		                       *KeyLabel(PC, Keys::HumanDrop));
	}
	return FString::Printf(TEXT("%s 移動 ／ %s ダッシュ ／ %s ジャンプ ／ %s 救助 ／ %s 音を鳴らす"), *Move,
	                       *KeyLabel(PC, Keys::ToySprint), *Jump, *KeyLabel(PC, Keys::ToyInteract), *KeyLabel(PC, Keys::ToyTaunt));
}
