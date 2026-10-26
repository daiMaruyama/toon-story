// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Core/TBKeyLabels.h"
#include "Core/TBInputSettings.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
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
	if (Key == EKeys::LeftShift || Key == EKeys::RightShift)
	{
		return TEXT("Shift");
	}
	if (Key == EKeys::LeftControl || Key == EKeys::RightControl)
	{
		return TEXT("Ctrl");
	}
	return Key.GetDisplayName(false).ToString();
}

// 行の割り当てのうち、有効でパッドでないキーでスロットが一番小さいものを選ぶ。
// FindCurrentMappingForSlotとFindMappingsInRowは、行を作る副作用や警告があるので使わない。
FString TB::KeyLabel(const APlayerController* PC, FName MappingName)
{
	const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	const auto* Input = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	const auto* User = Input ? Input->GetUserSettings() : nullptr;
	const auto* Profile = User ? User->GetActiveKeyProfile() : nullptr;
	const FKeyMappingRow* Row = Profile ? Profile->GetPlayerMappingRows().Find(MappingName) : nullptr;
	if (Row)
	{
		const FPlayerKeyMapping* Best = nullptr;
		bool bHasKeyboardSlot = false;
		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			if (Mapping.GetCurrentKey().IsGamepadKey())
			{
				continue;
			}
			bHasKeyboardSlot = true;
			if (Mapping.GetCurrentKey().IsValid() && (!Best || Mapping.GetSlot() < Best->GetSlot()))
			{
				Best = &Mapping;
			}
		}
		if (Best)
		{
			return FormatKey(Best->GetCurrentKey());
		}
		if (bHasKeyboardSlot)
		{
			return FormatKey(EKeys::Invalid);
		}
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
