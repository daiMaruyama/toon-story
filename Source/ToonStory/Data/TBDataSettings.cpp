// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Data/TBDataSettings.h"
#include "Data/TBCharacterData.h"
#include "Data/TBConfigData.h"
#include "Data/TBPresentationData.h"
#include "Data/TBRuleData.h"

namespace
{
	// 選ばれたアセットの読み込みは一度だけ試す。見つからなくても毎フレーム読み直さず、C++ の初期値を使い続ける。
	template <typename T>
	const T& LoadOrDefault(const TSoftObjectPtr<T>& Selected, TStrongObjectPtr<T>& Loaded, bool& bTried)
	{
		if (!bTried)
		{
			bTried = true;
			Loaded.Reset(Selected.LoadSynchronous());
			if (!Loaded && !Selected.IsNull())
			{
				UE_LOG(LogTemp, Warning, TEXT("[ToonStoryData] %s not found; using the C++ defaults."),
				       *Selected.ToString());
			}
		}
		return Loaded ? *Loaded.Get() : *GetDefault<T>();
	}
}

const UTBRuleData& UTBDataSettings::GetRuleData()
{
	auto* Settings = GetMutableDefault<UTBDataSettings>();
	return LoadOrDefault(Settings->RuleData, Settings->LoadedRuleData, Settings->bTriedRuleData);
}

const UTBCharacterData& UTBDataSettings::GetCharacterData()
{
	auto* Settings = GetMutableDefault<UTBDataSettings>();
	return LoadOrDefault(Settings->CharacterData, Settings->LoadedCharacterData, Settings->bTriedCharacterData);
}

const UTBPresentationData& UTBDataSettings::GetPresentationData()
{
	auto* Settings = GetMutableDefault<UTBDataSettings>();
	return LoadOrDefault(Settings->PresentationData, Settings->LoadedPresentationData,
	                     Settings->bTriedPresentationData);
}

const UTBConfigData& UTBDataSettings::GetConfigData()
{
	auto* Settings = GetMutableDefault<UTBDataSettings>();
	return LoadOrDefault(Settings->ConfigData, Settings->LoadedConfigData, Settings->bTriedConfigData);
}

const UTBRuleData& TB::Rules()
{
	return UTBDataSettings::GetRuleData();
}

const UTBCharacterData& TB::CharacterData()
{
	return UTBDataSettings::GetCharacterData();
}

const UTBPresentationData& TB::PresentationData()
{
	return UTBDataSettings::GetPresentationData();
}

const UTBConfigData& TB::ConfigData()
{
	return UTBDataSettings::GetConfigData();
}

#if WITH_EDITOR
void UTBDataSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	// 参照先を差し替えたら読み直す
	LoadedRuleData.Reset();
	LoadedCharacterData.Reset();
	LoadedPresentationData.Reset();
	LoadedConfigData.Reset();
	bTriedRuleData = bTriedCharacterData = bTriedPresentationData = bTriedConfigData = false;
}
#endif
