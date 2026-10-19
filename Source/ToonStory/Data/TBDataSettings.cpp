// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Data/TBDataSettings.h"
#include "Data/TBCharacterData.h"
#include "Data/TBRuleData.h"

namespace
{
	// 選ばれたアセットを一度だけ読み込む。未設定なら C++ の初期値を使う。
	template <typename T>
	const T& LoadOrDefault(const TSoftObjectPtr<T>& Selected, TObjectPtr<T>& Loaded)
	{
		if (!Loaded)
		{
			Loaded = Selected.LoadSynchronous();
		}
		return Loaded ? *Loaded : *GetDefault<T>();
	}
}

const UTBRuleData& UTBDataSettings::GetRuleData()
{
	auto* Settings = GetMutableDefault<UTBDataSettings>();
	return LoadOrDefault(Settings->RuleData, Settings->LoadedRuleData);
}

const UTBCharacterData& UTBDataSettings::GetCharacterData()
{
	auto* Settings = GetMutableDefault<UTBDataSettings>();
	return LoadOrDefault(Settings->CharacterData, Settings->LoadedCharacterData);
}

const UTBRuleData& TB::Rules()
{
	return UTBDataSettings::GetRuleData();
}

const UTBCharacterData& TB::CharacterData()
{
	return UTBDataSettings::GetCharacterData();
}

#if WITH_EDITOR
void UTBDataSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	// 参照先を差し替えたら読み直す
	LoadedRuleData = nullptr;
	LoadedCharacterData = nullptr;
}
#endif
