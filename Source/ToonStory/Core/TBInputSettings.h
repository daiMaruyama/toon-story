// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TBInputSettings.generated.h"

class UInputAction;
class UInputMappingContext;

/** 入力の参照だけを持つ。IMCは空なら何も付けない */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Toon Story Input"))
class TOONSTORY_API UTBInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}
	/** パッケージ版に含めるため、アセットはDirectoriesToAlwaysCookのフォルダに置く */
	// マウス感度0.07はIMC_CommonのScaleにだけ置く。iniのAxisConfigは1にしてある
	// （エンジンがAxisConfigをModifierとして自動で足すため、残すと二重に掛かる）。
	// スティックのDead Zone 0.25は、パッドにはAxisConfigが掛からないのでIMC側に置いている。
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "共通"))
	TSoftObjectPtr<UInputMappingContext> CommonContext;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "人間"))
	TSoftObjectPtr<UInputMappingContext> HumanContext;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "人形"))
	TSoftObjectPtr<UInputMappingContext> ToyContext;
	/** キャラが結ぶInput Action。IMCのマッピングと同じアセットを指す */
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "視点"))
	TSoftObjectPtr<UInputAction> LookAction;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "移動"))
	TSoftObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "ジャンプ"))
	TSoftObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "しゃがむ"))
	TSoftObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "ダッシュ"))
	TSoftObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "インタラクト"))
	TSoftObjectPtr<UInputAction> InteractAction;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "置く"))
	TSoftObjectPtr<UInputAction> DropAction;
	UPROPERTY(Config, EditAnywhere, Category = "入力", meta = (DisplayName = "挑発"))
	TSoftObjectPtr<UInputAction> TauntAction;
};
