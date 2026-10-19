// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "Data/TBDataSettings.h"
#include "Data/TBRuleData.h"

const UTBRuleData& UTBDataSettings::GetRuleData()
{
	auto* Settings = GetMutableDefault<UTBDataSettings>();
	if (!Settings->LoadedRuleData)
	{
		Settings->LoadedRuleData = Settings->RuleData.LoadSynchronous();
	}
	return Settings->LoadedRuleData ? *Settings->LoadedRuleData : *GetDefault<UTBRuleData>();
}

#if WITH_EDITOR
void UTBDataSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	LoadedRuleData = nullptr; // 参照先を差し替えたら読み直す
}
#endif
