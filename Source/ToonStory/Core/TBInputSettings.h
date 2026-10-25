// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TBInputSettings.generated.h"

class UInputAction;
class UInputMappingContext;

/** 入力の参照だけを持つ。IMC は空なら何も付けない */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Toon Story Input"))
class TOONSTORY_API UTBInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}
	/** パッケージ版に含めるため、アセットは DirectoriesToAlwaysCook のフォルダに置く */
	// マウス感度 0.07 は IMC_Common の Scale にだけ置く。ini の AxisConfig は 1 にしてある
	// （エンジンが AxisConfig を Modifier として自動で足すため、残すと二重に掛かる）。
	// スティックの Dead Zone 0.25 は、パッドには AxisConfig が掛からないので IMC 側に置いている。
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "共通"))
	TSoftObjectPtr<UInputMappingContext> CommonContext;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "人間"))
	TSoftObjectPtr<UInputMappingContext> HumanContext;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "人形"))
	TSoftObjectPtr<UInputMappingContext> ToyContext;
	/** キャラが結ぶ Input Action。IMC のマッピングと同じアセットを指す */
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "視点"))
	TSoftObjectPtr<UInputAction> LookAction;
};
