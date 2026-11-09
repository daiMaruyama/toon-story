// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "STBLoadingScreen.h"
#include "TBLoadingScreenSettings.h"
#include "UI/TBPaperStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void STBLoadingScreen::Construct(const FArguments&)
{
	const auto* Settings = GetDefault<UTBLoadingScreenSettings>();
	// ロード中に消されないよう、絵とフォントはここで読み込んで持っておく。
	for (const auto& Path : Settings->Toys)
	{
		if (UTexture2D* Texture = Path.LoadSynchronous())
		{
			Textures.Emplace(Texture);
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight());
			Brushes.Add(Brush);
		}
	}
	Font.Reset(TBPaper::Font(GetTransientPackage()));
	Random.Initialize(int32(FPlatformTime::Cycles()));
	// 始まった瞬間から、おもちゃがもう顔を出している。途中で切れても不自然にならないように。
	Started = FPlatformTime::Seconds() - Settings->PeekSeconds * .4;
	Restart();
	SetVisibility(EVisibility::HitTestInvisible);
}

bool STBLoadingScreen::IsToyHidden() const
{
	const auto* Settings = GetDefault<UTBLoadingScreenSettings>();
	const double Period = Settings->PeekSeconds + Settings->RunSeconds + Settings->HiddenSeconds;
	const double T = FMath::Fmod(FPlatformTime::Seconds() - Started, Period);
	// 隠れたあとの間が0なら、隠れきる瞬間がないので待たない。
	return Brushes.IsEmpty() || Settings->HiddenSeconds <= 0.f || T >= Settings->PeekSeconds + Settings->RunSeconds;
}

int32 STBLoadingScreen::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                                FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	const auto* Settings = GetDefault<UTBLoadingScreenSettings>();
	const FVector2f Size = FVector2f(Geometry.GetLocalSize());
	const float W = Size.X, H = Size.Y;
	if (W <= 0 || H <= 0 || Exit >= 1.f) return Layer;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	// 入るときは黒から明け、出るときはいったん真っ黒に落としてから、黒だけを薄くしてゲーム画面を見せる。
	// ロード画面とゲーム画面が半透明で重ならないようにする。
	const float In = Settings->FadeInSeconds > 0.f
	                     ? FMath::Clamp(float(FPlatformTime::Seconds() - ShownAt) / Settings->FadeInSeconds, 0.f, 1.f)
	                     : 1.f;
	const float Black = FMath::Max(1.f - In, Exit < .5f ? Exit * 2.f : (1.f - Exit) * 2.f);
	auto Curtain = [&](int32 At)
	{
		if (Black > 0.f)
			FSlateDrawElement::MakeBox(Out, At, Geometry.ToPaintGeometry(), White, ESlateDrawEffect::None,
			                           FLinearColor(0, 0, 0, Black));
	};
	if (Exit >= .5f)
	{
		Curtain(Layer);
		return Layer;
	}
	static const FSlateRoundedBoxBrush Rounded(FLinearColor::White, 6.f);
	auto Box = [&](const FSlateBrush* Brush, float X, float Y, float BW, float BH, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2f(BW, BH), FSlateLayoutTransform(FVector2f(X, Y))),
		                           Brush, ESlateDrawEffect::None, Color);
	};
	auto Shade = [](FLinearColor Color, float Scale)
	{
		return FLinearColor(Color.R * Scale, Color.G * Scale, Color.B * Scale, Color.A);
	};

	// 部屋。上ほど暗い壁と、床。
	const float FloorY = H * .74f;
	FSlateDrawElement::MakeGradient(
	    Out, Layer, Geometry.ToPaintGeometry(FVector2f(W, FloorY), FSlateLayoutTransform()),
	    {FSlateGradientStop(FVector2D(0, 0), Shade(Settings->WallColor, .55f)),
	     FSlateGradientStop(FVector2D(0, FloorY), Settings->WallColor)},
	    Orient_Horizontal);
	Box(White, 0, FloorY, W, H - FloorY, Settings->FloorColor);
	Box(White, 0, FloorY, W, FMath::Max(2.f, H * .006f), Shade(Settings->FloorColor, 1.5f));
	++Layer;

	// おもちゃ。積み木の陰から顔を出し、様子を見てから、箱の陰へ跳ねながら走る。
	const float BlockRight = W * .22f, BoxLeft = W * .70f;
	const float Peek = Settings->PeekSeconds, Run = Settings->RunSeconds;
	const double Period = Peek + Run + Settings->HiddenSeconds;
	const double Elapsed = FPlatformTime::Seconds() - Started;
	const int32 Current = int32(Elapsed / Period);
	const float T = float(Elapsed - Current * Period);
	if (Current != Loop && Brushes.Num() > 0)
	{
		// 続けて同じおもちゃが出ないように選ぶ。
		const int32 Next = Brushes.Num() > 1 ? (Toy + 1 + Random.RandRange(0, Brushes.Num() - 2)) % Brushes.Num() : 0;
		Toy = Loop < 0 ? Random.RandRange(0, Brushes.Num() - 1) : Next;
		Loop = Current;
	}
	if (Brushes.IsValidIndex(Toy) && T < Peek + Run)
	{
		const FSlateBrush& Brush = Brushes[Toy];
		const float ToyH = H * Settings->ToyHeight;
		const float ToyW = ToyH * float(Brush.ImageSize.X / FMath::Max(Brush.ImageSize.Y, 1.0));
		const float PeekX = BlockRight - ToyW * .45f;
		float X, Lift = 0, Angle = 0;
		bool bLookBack = false;
		if (T < Peek)
		{
			// そっと出てきて、きょろきょろと後ろを振り返る。
			const float Out01 = FMath::InterpEaseOut(0.f, 1.f, FMath::Min(T / .35f, 1.f), 2.f);
			X = FMath::Lerp(BlockRight - ToyW, PeekX, Out01);
			Lift = FMath::Abs(FMath::Sin(T * 7.f)) * H * .006f;
			bLookBack = T > Peek * .45f && T < Peek * .75f;
		}
		else
		{
			const float U = (T - Peek) / Run;
			X = FMath::Lerp(PeekX, BoxLeft + ToyW * .15f, FMath::InterpEaseInOut(0.f, 1.f, U, 1.6f));
			const float Hop = FMath::Sin(U * Settings->Hops * PI);
			Lift = FMath::Abs(Hop) * H * .045f;
			Angle = FMath::DegreesToRadians(-5.f + 6.f * FMath::Sin(U * Settings->Hops * 2.f * PI));
		}
		// 影は床に残し、体だけ跳ねる。
		Box(&Rounded, X + ToyW * .25f, FloorY - H * .008f, ToyW * .5f, H * .016f, FLinearColor(0, 0, 0, .35f));
		FSlateBrush Drawn = Brush;
		Drawn.Mirroring = bLookBack ? ESlateBrushMirrorType::Horizontal : ESlateBrushMirrorType::NoMirror;
		FSlateDrawElement::MakeBox(
		    Out, Layer,
		    Geometry.ToPaintGeometry(FVector2f(ToyW, ToyH), FSlateLayoutTransform(FVector2f(X, FloorY - ToyH - Lift)),
		                             FSlateRenderTransform(FQuat2f(Angle)), FVector2f(.5f, 1.f)),
		    &Drawn, ESlateDrawEffect::None, Settings->ToyTint);
	}
	++Layer;

	// 手前の積み木と箱。おもちゃはこの陰に隠れる。
	const float Cube = H * .14f;
	const TArray<FLinearColor>& Colors = Settings->BlockColors;
	auto Block = [&](int32 Index, float X, float Y)
	{
		const FLinearColor Color = Colors.Num() ? Colors[Index % Colors.Num()] : FLinearColor::Gray;
		Box(&Rounded, X, Y, Cube, Cube, Color);
		Box(&Rounded, X + Cube * .18f, Y + Cube * .18f, Cube * .64f, Cube * .64f, Shade(Color, 1.25f));
	};
	Block(0, BlockRight - Cube * 2.f, FloorY - Cube);
	Block(1, BlockRight - Cube, FloorY - Cube);
	Block(2, BlockRight - Cube * 1.55f, FloorY - Cube * 2.f);
	const float BoxW = W * .25f, BoxH = H * .34f;
	Box(&Rounded, BoxLeft, FloorY - BoxH, BoxW, BoxH, Settings->BoxColor);
	Box(White, BoxLeft, FloorY - BoxH, BoxW, BoxH * .12f, Shade(Settings->BoxColor, .7f));
	Box(White, BoxLeft + BoxW * .5f - 1.f, FloorY - BoxH * .88f, 2.f, BoxH * .88f, Shade(Settings->BoxColor, .75f));
	++Layer;

	if (!Status.IsEmpty() && Font.IsValid())
	{
		const FSlateFontInfo Info(Font.Get(), FMath::Max(12.f, H * .03f));
		FSlateDrawElement::MakeText(Out, Layer,
		                            Geometry.ToPaintGeometry(FVector2f(W * .8f, H * .08f), FSlateLayoutTransform(FVector2f(W * .06f, H * .85f))),
		                            Status, Info, ESlateDrawEffect::None, TBPaper::Cream);
	}
	Curtain(++Layer);
	return Layer;
}
