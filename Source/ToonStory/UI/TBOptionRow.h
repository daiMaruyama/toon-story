// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBOptionRow.generated.h"

class UButton;
class UFont;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/** オプション画面の1行。「<」「>」で値を1つずつ動かす。値の読み書きは、行を作る側が関数で渡す。 */
UCLASS()
class TOONSTORY_API UTBOptionRow : public UUserWidget
{
	GENERATED_BODY()
public:
	// bBar なら、値の割合をバーでも表す。
	void Build(const FString& Label, UFont* Font, UTexture2D* Paper, bool bBar);
	void Refresh();
	TFunction<void(int32 Steps)> Step;
	TFunction<FString()> Value;
	// 0〜1。バーがあるときだけ使う。
	TFunction<float()> Fill;
	// その向きにまだ動かせるか。端まで来たら、その向きのボタンを押せなくする。
	TFunction<bool(int32 Steps)> CanStep;
	// 値を変えたあと。ほかの行の表示も変わることがある。
	TFunction<void()> Changed;

private:
	UFUNCTION()
	void Down();
	UFUNCTION()
	void Up();
	void Move(int32 Steps);
	UPROPERTY(Transient)
	TObjectPtr<UButton> DownButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> UpButton;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueText;
};
