// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBOptionRow.generated.h"

class UButton;
class UFont;
class USlider;
class UTextBlock;
class UTexture2D;

/** オプション画面の1行。「<」「>」で値を1つずつ動かし、bBar の行はスライダーでも動かせる。
    値の読み書きは、行を作る側が関数で渡す。 */
UCLASS()
class TOONSTORY_API UTBOptionRow : public UUserWidget
{
	GENERATED_BODY()
public:
	void Build(const FString& Label, UFont* Font, UTexture2D* Paper, bool bBar);
	void Refresh();
	TFunction<void(int32 Steps)> Step;
	TFunction<FString()> Value;
	// 0〜1。スライダーがあるときだけ使う。
	TFunction<float()> Fill;
	// その向きにまだ動かせるか。端まで来たら、その向きのボタンを押せなくする。
	TFunction<bool(int32 Steps)> CanStep;
	// スライダーを動かしている途中（0〜1）。すぐ反映してよいものだけ反映し、保存はしない。
	TFunction<void(float Fraction)> Slide;
	// スライダーから手を離したとき。ここで保存する。
	TFunction<void()> Released;
	// スライダーの目盛りの幅（0〜1 の割合）。0 なら目盛りなし。
	float SliderStep = 0.f;
	// 値を変えたあと。ほかの行の表示も変わることがある。
	TFunction<void()> Changed;

private:
	UFUNCTION()
	void Down();
	UFUNCTION()
	void Up();
	UFUNCTION()
	void SliderMoved(float Fraction);
	UFUNCTION()
	void SliderGrabbed();
	UFUNCTION()
	void SliderReleased();
	void Move(int32 Steps);
	void RefreshText();
	bool bDragging = false;
	bool bRefreshing = false;
	UPROPERTY(Transient)
	TObjectPtr<UButton> DownButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> UpButton;
	UPROPERTY(Transient)
	TObjectPtr<USlider> Slider;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueText;
};
