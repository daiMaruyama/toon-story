#pragma once
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "GameplayTagContainer.h"
#include "UserSettings/EnhancedInputUserSettings.h"

// テスト用。キーボード側（またはパッド側）の割り当てを変え、保存はせず、変える前のキーを控えて破棄時に書き戻す。
class FTBKeyRemapper
{
public:
	FTBKeyRemapper(FAutomationTestBase& InTest, UEnhancedInputUserSettings* InUser) : Test(InTest), User(InUser)
	{
	}

	~FTBKeyRemapper()
	{
		for (int32 Index = Saved.Num() - 1; Index >= 0; --Index)
		{
			FGameplayTagContainer Failure;
			User->MapPlayerKey(MakeArgs(Saved[Index].Name, Saved[Index].Slot, Saved[Index].DeviceId, Saved[Index].Key), Failure);
		}
	}

	// 今のキーを控える（変えるのは別の関数）。
	bool Remember(FName Name, bool bGamepad = false)
	{
		const FPlayerKeyMapping* Current = FindMapping(Name, bGamepad);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s has a %s slot"), *Name.ToString(), bGamepad ? TEXT("gamepad") : TEXT("keyboard")), Current))
			return false;
		Saved.Add({Name, Current->GetSlot(), Current->GetHardwareDeviceId().HardwareDeviceIdentifier, Current->GetCurrentKey()});
		return true;
	}

	// 控えてから、UserSettingsを直接書き換える（かぶりの判定はしない）。
	void Remap(FName Name, const FKey& NewKey, bool bGamepad = false)
	{
		const FPlayerKeyMapping* Current = FindMapping(Name, bGamepad);
		if (!Remember(Name, bGamepad)) return;
		FGameplayTagContainer Failure;
		User->MapPlayerKey(MakeArgs(Name, Current->GetSlot(), Current->GetHardwareDeviceId().HardwareDeviceIdentifier, NewKey), Failure);
	}

private:
	struct FSaved
	{
		FName Name;
		EPlayerMappableKeySlot Slot;
		FName DeviceId;
		FKey Key;
	};

	// 既定のキーがパッドかどうかで分ける。キーボードはTB::FindKeyboardKeyと同じ選び方（有効なキーのうちスロットが一番小さいもの、無ければ外した枠）。
	const FPlayerKeyMapping* FindMapping(FName Name, bool bGamepad) const
	{
		const FKeyMappingRow* Row = User->GetActiveKeyProfile()->GetPlayerMappingRows().Find(Name);
		if (!Row) return nullptr;
		const FPlayerKeyMapping* Valid = nullptr;
		const FPlayerKeyMapping* Empty = nullptr;
		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			if (Mapping.GetDefaultKey().IsGamepadKey() != bGamepad) continue;
			const FPlayerKeyMapping*& Slot = Mapping.GetCurrentKey().IsValid() ? Valid : Empty;
			if (!Slot || Mapping.GetSlot() < Slot->GetSlot()) Slot = &Mapping;
		}
		return Valid ? Valid : Empty;
	}

	FMapPlayerKeyArgs MakeArgs(FName Name, EPlayerMappableKeySlot Slot, FName DeviceId, const FKey& Key) const
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = Name;
		Args.Slot = Slot;
		Args.HardwareDeviceId = DeviceId;
		Args.NewKey = Key;
		Args.ProfileIdString = User->GetActiveKeyProfile()->GetProfileIdString();
		return Args;
	}

	FAutomationTestBase& Test;
	UEnhancedInputUserSettings* User;
	TArray<FSaved> Saved;
};
#endif
