// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TBDataSettings.generated.h"

class UTBCharacterData;
class UTBConfigData;
class UTBPresentationData;
class UTBRuleData;

/** どのマスターデータを使うかの参照だけを持つ。値は各DataAssetに置く */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Toon Story Data"))
class TOONSTORY_API UTBDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}
	/** 未設定なら C++ の初期値を返す */
	static const UTBRuleData& GetRuleData();
	static const UTBCharacterData& GetCharacterData();
	static const UTBPresentationData& GetPresentationData();
	static const UTBConfigData& GetConfigData();
	/** パッケージ版に含めるため、アセットは DirectoriesToAlwaysCook のフォルダに置く */
	UPROPERTY(Config, EditAnywhere, Category = "マスターデータ", meta = (DisplayName = "ルール"))
	TSoftObjectPtr<UTBRuleData> RuleData;
	UPROPERTY(Config, EditAnywhere, Category = "マスターデータ", meta = (DisplayName = "キャラ"))
	TSoftObjectPtr<UTBCharacterData> CharacterData;
	UPROPERTY(Config, EditAnywhere, Category = "マスターデータ", meta = (DisplayName = "演出"))
	TSoftObjectPtr<UTBPresentationData> PresentationData;
	/** プレイヤーが変えられる設定の、初期値と範囲 */
	UPROPERTY(Config, EditAnywhere, Category = "マスターデータ", meta = (DisplayName = "コンフィグ"))
	TSoftObjectPtr<UTBConfigData> ConfigData;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	// 読み込んだアセットを持っておき、GC で消えて読み直しになるのを防ぐ。
	UPROPERTY(Transient)
	TObjectPtr<UTBRuleData> LoadedRuleData;
	UPROPERTY(Transient)
	TObjectPtr<UTBCharacterData> LoadedCharacterData;
	UPROPERTY(Transient)
	TObjectPtr<UTBPresentationData> LoadedPresentationData;
	UPROPERTY(Transient)
	TObjectPtr<UTBConfigData> LoadedConfigData;
	bool bTriedRuleData = false;
	bool bTriedCharacterData = false;
	bool bTriedPresentationData = false;
	bool bTriedConfigData = false;
};
