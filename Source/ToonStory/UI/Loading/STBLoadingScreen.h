// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SLeafWidget.h"

class UFont;
class UTexture2D;

/**
 * ロード画面の絵。暗い子ども部屋で、おもちゃが左の積み木の陰から顔を出し、右の箱の陰へ小走りで隠れる。
 * ロード中はゲームの処理が止まるので、UObject の更新に頼らず、時間だけから毎回描く（ロード用のスレッドからも描かれる）。
 */
class STBLoadingScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(STBLoadingScreen) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	// 下に出す状態の文字。ロード中は描画が別スレッドなので、ロードを始める前に決めておく。
	void SetStatus(const FText& Text) { Status = Text; }
	// 出しはじめ。黒から明けて出てくる。
	void Restart() { ShownAt = FPlatformTime::Seconds(), Exit = 0.f; }
	// ロード中に MoviePlayer が描いていた絵から、時間・おもちゃ・文字を引き継ぐ。同じ部品を2か所に付けないため、別に作って続ける。
	void ContinueFrom(const STBLoadingScreen& Other)
	{
		Status = Other.Status, ShownAt = Other.ShownAt, Started = Other.Started, Exit = 0.f;
		Loop = Other.Loop, Toy = Other.Toy;
	}
	// 消え具合。0で出ている、0.5で真っ黒、1で消えきる（下のゲーム画面が明けて見える）。
	void SetExit(float Value) { Exit = FMath::Clamp(Value, 0.f, 1.f); }
	float GetExit() const { return Exit; }
	// おもちゃが箱の陰に隠れきって、次が顔を出す前の間か。ここで消せば、走る途中で切れない。
	bool IsToyHidden() const;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	                      FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
	                      bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1280, 720); }

private:
	TArray<TStrongObjectPtr<UTexture2D>> Textures;
	TArray<FSlateBrush> Brushes;
	TStrongObjectPtr<UFont> Font;
	FText Status;
	float Exit = 0.f;
	double ShownAt = 0;
	double Started = 0;
	// 1周ごとに替えるおもちゃ。描画のたびに時間から求め直す。
	mutable int32 Loop = -1;
	mutable int32 Toy = 0;
	mutable FRandomStream Random;
};
