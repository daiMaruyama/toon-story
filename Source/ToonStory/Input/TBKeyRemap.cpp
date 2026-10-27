// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Input/TBKeyRemap.h"
#include "Algo/AllOf.h"
#include "Input/TBInputSettings.h"
#include "Input/TBKeyLabels.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
	// IMCの並び。Commonだけは全部の行とかぶりを見る。
	constexpr int32 CommonScope = 0;

	// 行 → 入っているIMCの番号（Common, Human, Toy, Lobby）。
	// Keys::AllのどれかがどのIMCにも無い、または別のIMCにも入っていればfalse。同じIMC内のキーボードとパッドの重複は正常。
	bool BuildScopes(TMap<FName, int32>& Scopes)
	{
		const auto* Settings = GetDefault<UTBInputSettings>();
		const TSoftObjectPtr<UInputMappingContext>* Contexts[] = {&Settings->CommonContext, &Settings->HumanContext,
		                                                          &Settings->ToyContext, &Settings->LobbyContext};
		for (int32 Scope = 0; Scope < UE_ARRAY_COUNT(Contexts); ++Scope)
		{
			const UInputMappingContext* Context = Contexts[Scope]->IsNull() ? nullptr : Contexts[Scope]->LoadSynchronous();
			if (!Context)
			{
				continue;
			}
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				const UPlayerMappableKeySettings* Key = Mapping.GetPlayerMappableKeySettings();
				if (!Key)
				{
					continue;
				}
				const int32* Existing = Scopes.Find(Key->Name);
				if (Existing && *Existing != Scope)
				{
					UE_LOG(LogTemp, Warning, TEXT("[ToonStoryInput] Mappable name %s is in more than one IMC."), *Key->Name.ToString());
					return false;
				}
				Scopes.Add(Key->Name, Scope);
			}
		}
		for (const FName& Name : TB::Keys::All())
		{
			if (!Scopes.Contains(Name))
			{
				UE_LOG(LogTemp, Warning, TEXT("[ToonStoryInput] Mappable name %s is not in any IMC."), *Name.ToString());
				return false;
			}
		}
		return true;
	}

	bool Overlaps(int32 A, int32 B)
	{
		return A == CommonScope || B == CommonScope || A == B;
	}

	// 13行の今のキーボード側のキー。
	bool BuildTable(const APlayerController* PC, TMap<FName, FKey>& Table)
	{
		for (const FName& Name : TB::Keys::All())
		{
			TB::FKeyboardKey Row;
			if (!TB::FindKeyboardKey(PC, Name, Row))
			{
				UE_LOG(LogTemp, Warning, TEXT("[ToonStoryInput] Row %s has no keyboard slot."), *Name.ToString());
				return false;
			}
			Table.Add(Name, Row.Key);
		}
		return true;
	}

	// 表の中で、Rowと同じキーを使っていて、範囲が重なる別の行。外した行（Invalid）はかぶらない。
	TArray<FName> ConflictsFor(const TMap<FName, FKey>& Table, const TMap<FName, int32>& Scopes, FName Row)
	{
		TArray<FName> Result;
		const FKey& Key = Table.FindChecked(Row);
		if (!Key.IsValid())
		{
			return Result;
		}
		for (const FName& Other : TB::Keys::All())
		{
			if (Other != Row && Table.FindChecked(Other) == Key && Overlaps(Scopes.FindChecked(Row), Scopes.FindChecked(Other)))
			{
				Result.Add(Other);
			}
		}
		return Result;
	}

	UEnhancedInputUserSettings* GetUserSettings(const APlayerController* PC)
	{
		const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
		const auto* Input = LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
		return Input ? Input->GetUserSettings() : nullptr;
	}

	// OnSettingsChangedはすぐ送られ、入力の割り当ての作り直しは保留されて次の処理で行われる。
	bool MapKey(UEnhancedInputUserSettings* User, FName Name, const TB::FKeyboardKey& Row, const FKey& Key)
	{
		const auto* Profile = User->GetActiveKeyProfile();
		if (!Profile)
		{
			return false;
		}
		FMapPlayerKeyArgs Args;
		Args.MappingName = Name;
		Args.Slot = static_cast<EPlayerMappableKeySlot>(Row.Slot);
		Args.HardwareDeviceId = Row.DeviceId;
		Args.NewKey = Key;
		Args.ProfileIdString = Profile->GetProfileIdString();
		FGameplayTagContainer Failure;
		User->MapPlayerKey(Args, Failure);
		return Failure.IsEmpty();
	}

	TB::FRemapResult Make(TB::ERemapResult Result, TArray<FName> Conflicting = {}, FName SwappedWith = NAME_None)
	{
		TB::FRemapResult Out;
		Out.Result = Result;
		Out.Conflicting = MoveTemp(Conflicting);
		Out.SwappedWith = SwappedWith;
		return Out;
	}
}

bool TB::IsUsableKey(const FKey& Key)
{
	if (!Key.IsValid() || !Key.IsBindableToActions() || Key == EKeys::Escape || Key.IsGamepadKey() || Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D() ||
	    Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown || Key == EKeys::LeftCommand || Key == EKeys::RightCommand)
	{
		return false;
	}
	return !GetDefault<UInputSettings>()->ConsoleKeys.Contains(Key);
}

TB::FRemapResult TB::CheckRemap(const APlayerController* PC, FName MappingName, const FKey& NewKey)
{
	if (!IsUsableKey(NewKey))
	{
		return Make(ERemapResult::Unusable);
	}
	TMap<FName, int32> Scopes;
	TMap<FName, FKey> Table;
	if (!BuildScopes(Scopes) || !BuildTable(PC, Table) || !Table.Contains(MappingName))
	{
		return Make(ERemapResult::Error);
	}
	Table[MappingName] = NewKey;
	TArray<FName> Conflicts = ConflictsFor(Table, Scopes, MappingName);
	return Make(Conflicts.IsEmpty() ? ERemapResult::OK : ERemapResult::Conflict, MoveTemp(Conflicts));
}

TB::FRemapResult TB::Remap(const APlayerController* PC, FName MappingName, const FKey& NewKey)
{
	FRemapResult Check = CheckRemap(PC, MappingName, NewKey);
	if (Check.Result != ERemapResult::OK)
	{
		return Check;
	}
	auto* User = GetUserSettings(PC);
	FKeyboardKey Row;
	if (!User || !FindKeyboardKey(PC, MappingName, Row) || !MapKey(User, MappingName, Row, NewKey))
	{
		return Make(ERemapResult::Error);
	}
	return Check;
}

TB::FRemapResult TB::SwapRemap(const APlayerController* PC, FName MappingName, const FKey& NewKey)
{
	if (!IsUsableKey(NewKey))
	{
		return Make(ERemapResult::Unusable);
	}
	TMap<FName, int32> Scopes;
	TMap<FName, FKey> Table;
	auto* User = GetUserSettings(PC);
	if (!User || !BuildScopes(Scopes) || !BuildTable(PC, Table) || !Table.Contains(MappingName))
	{
		return Make(ERemapResult::Error);
	}
	const FKey OldKey = Table.FindChecked(MappingName);
	Table[MappingName] = NewKey;
	const TArray<FName> Conflicts = ConflictsFor(Table, Scopes, MappingName);
	if (Conflicts.IsEmpty())
	{
		return Make(ERemapResult::NoPartner);
	}
	if (Conflicts.Num() > 1)
	{
		return Make(ERemapResult::Conflict, Conflicts);
	}
	// 入れ替えたあとの表を先に作り、かぶりが残るなら断る。
	const FName Partner = Conflicts[0];
	Table[Partner] = OldKey;
	TArray<FName> Remaining = ConflictsFor(Table, Scopes, MappingName);
	for (const FName& Name : ConflictsFor(Table, Scopes, Partner))
	{
		Remaining.AddUnique(Name);
	}
	if (!Remaining.IsEmpty())
	{
		return Make(ERemapResult::Conflict, MoveTemp(Remaining));
	}
	FKeyboardKey Row, PartnerRow;
	if (!FindKeyboardKey(PC, MappingName, Row) || !FindKeyboardKey(PC, Partner, PartnerRow) ||
	    !MapKey(User, Partner, PartnerRow, OldKey) || !MapKey(User, MappingName, Row, NewKey))
	{
		return Make(ERemapResult::Error);
	}
	return Make(ERemapResult::OK, {}, Partner);
}

TB::FRemapResult TB::ResetToDefault(const APlayerController* PC, FName MappingName)
{
	TMap<FName, int32> Scopes;
	TMap<FName, FKey> Table;
	auto* User = GetUserSettings(PC);
	FKeyboardKey Row;
	if (!User || !BuildScopes(Scopes) || !BuildTable(PC, Table) || !Table.Contains(MappingName) ||
	    !FindKeyboardKey(PC, MappingName, Row))
	{
		return Make(ERemapResult::Error);
	}
	Table[MappingName] = Row.DefaultKey;
	TArray<FName> Conflicts = ConflictsFor(Table, Scopes, MappingName);
	if (!Conflicts.IsEmpty())
	{
		return Make(ERemapResult::Conflict, MoveTemp(Conflicts));
	}
	return Make(MapKey(User, MappingName, Row, Row.DefaultKey) ? ERemapResult::OK : ERemapResult::Error);
}

TB::FRemapResult TB::ResetAll(const APlayerController* PC)
{
	auto* User = GetUserSettings(PC);
	if (!User)
	{
		return Make(ERemapResult::Error);
	}
	// 先に全部の枠が引けることを確かめてから書き換える。
	TArray<TPair<FName, FKeyboardKey>> Rows;
	for (const FName& Name : Keys::All())
	{
		FKeyboardKey Row;
		if (!FindKeyboardKey(PC, Name, Row))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ToonStoryInput] Row %s has no keyboard slot."), *Name.ToString());
			return Make(ERemapResult::Error);
		}
		Rows.Emplace(Name, Row);
	}
	bool bOK = true;
	for (const TPair<FName, FKeyboardKey>& Row : Rows)
	{
		bOK &= MapKey(User, Row.Key, Row.Value, Row.Value.DefaultKey);
	}
	return Make(bOK ? ERemapResult::OK : ERemapResult::Error);
}
