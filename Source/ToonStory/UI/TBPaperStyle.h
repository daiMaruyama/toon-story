#pragma once
#include "CoreMinimal.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace TBPaper
{
	inline const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(59, 59, 54));
	inline const FLinearColor Accent = FLinearColor::FromSRGBColor(FColor(190, 83, 61));
	inline const FLinearColor Cream = FLinearColor::FromSRGBColor(FColor(246, 237, 207));
	inline const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor(111, 111, 101));
	inline FString Status(const FString& Value)
	{
		if (Value == TEXT("Create a room or find friends to begin."))
		{
			return TEXT("部屋をつくるか、友だちの部屋をさがそう。");
		}
		if (Value == TEXT("Creating room..."))
		{
			return TEXT("部屋をつくっています…");
		}
		if (Value == TEXT("Searching rooms..."))
		{
			return TEXT("みんなの部屋をさがしています…");
		}
		if (Value == TEXT("Joining room..."))
		{
			return TEXT("部屋に入っています…");
		}
		if (Value == TEXT("Connecting to host..."))
		{
			return TEXT("ホストにつないでいます…");
		}
		if (Value == TEXT("Leaving room..."))
		{
			return TEXT("部屋から退出しています…");
		}
		if (Value == TEXT("Room creation failed."))
		{
			return TEXT("部屋をつくれませんでした。もう一度試してください。");
		}
		if (Value == TEXT("Search failed."))
		{
			return TEXT("部屋をさがせませんでした。もう一度試してください。");
		}
		if (Value == TEXT("Offline. Last match ended; TBHost / TBFind for a new lobby."))
		{
			return TEXT("おかえりなさい。次はどの部屋であそぶ？");
		}
		return Value;
	}

	inline UFont* Font(UObject* Outer, bool Title = false)
	{
		auto* Result = NewObject<UFont>(Outer);
		Result->FontCacheType = EFontCacheType::Runtime;
		Result->LegacyFontSize = 14;
		Result->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Emplace(
		    TEXT("Regular"),
		    FPaths::ProjectContentDir() / TEXT("UI/PaperClub/Fonts") / (Title ? TEXT("mochiy.ttf") : TEXT("zen.ttf")),
		    EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		return Result;
	}
	inline FSlateBrush Brush(UTexture2D* Texture, FLinearColor Tint = FLinearColor::White)
	{
		FSlateBrush Result;
		Result.SetResourceObject(Texture);
		Result.DrawAs = ESlateBrushDrawType::Box;
		Result.Margin = FMargin(.25f);
		Result.ImageSize = FVector2D(128);
		Result.TintColor = Tint;
		return Result;
	}
	// 紙のボタン。Secondary は少し暗い紙。
	inline FButtonStyle ButtonStyle(UTexture2D* Texture, bool Secondary = false)
	{
		FButtonStyle Style;
		Style.SetNormal(Brush(Texture, Secondary ? FLinearColor(.78f, .78f, .75f) : FLinearColor::White));
		Style.SetHovered(Brush(Texture, Cream));
		Style.SetPressed(Brush(Texture, FLinearColor(.9f, .66f, .47f)));
		Style.SetDisabled(Brush(Texture, FLinearColor(.46f, .46f, .43f)));
		return Style;
	}
} // namespace TBPaper
