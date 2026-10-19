// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include <cmath>
// UEに依存しないルール計算。ゲーム本体と単体テストで同じ式を使う。
namespace TBRuleMath
{
	// 上昇は止めるが、すでに落下している速度は残す。
	inline float FrozenVerticalSpeed(float Speed)
	{
		return Speed > 0.f ? 0.f : Speed;
	}

	// 段差で跳ねた分だけ目線を元の高さに残し、0 へ向けて追いつかせる。遅れは上限で止め、超えた分はそのまま追従する。
	// それまでの遅れを先に縮めてから今回の分を足すので、跳ねたフレームでは目線が動かない。
	// 指数で減らすので、フレームレートが落ちたり一瞬止まったりしても、同じ速さで追いつく。
	inline float SmoothStepOffset(float Offset, float StepZ, float DeltaSeconds, float Speed, float MaxOffset)
	{
		const float Rate = Speed > 0.f ? Speed : 0.f;
		const float Raw = Offset * std::exp(-Rate * (DeltaSeconds > 0.f ? DeltaSeconds : 0.f)) - StepZ;
		const float Limit = MaxOffset > 0.f ? MaxOffset : 0.f;
		return Raw > Limit ? Limit : (Raw < -Limit ? -Limit : Raw);
	}

	// 進捗は0～1。接触解除・資格喪失でリセットし、凍結中は保持する。
	inline float AdvanceContact(float Progress, float DeltaSeconds, float Seconds, bool Touching, bool Frozen,
	                            bool Eligible)
	{
		if (!Touching || !Eligible)
		{
			return 0.f;
		}
		if (Frozen || DeltaSeconds <= 0.f)
		{
			return Progress;
		}
		const float Next = Progress + DeltaSeconds / (Seconds > .1f ? Seconds : .1f);
		return Next > 1.f ? 1.f : Next;
	}

	// 追加の救助者1人につき、指定割合を基準速度へ加える。
	inline float RescueRate(int N, float Seconds, float AdditionalBonus)
	{
		return N > 0
		           ? (1.f + (AdditionalBonus > 0.f ? AdditionalBonus : 0.f) * (N - 1)) / (Seconds > .1f ? Seconds : .1f)
		           : 0.f;
	}

	// 扉の状態はなく、全員を収納すると人間の勝利。
	inline bool HumansWin(int Toys, int Boxed)
	{
		return Toys > 0 && Toys == Boxed;
	}

	enum class EWinReason
	{
		None,
		TimeExpired,
		ItemsCollected,
		AllToysBoxed
	};

	// 同時成立時は時間切れを優先。時間切れと全員収納は人間、収集達成はおもちゃ勝利。
	inline EWinReason EvaluateWin(float Remaining, int Collected, int Required, int Toys, int Boxed)
	{
		if (Remaining <= 0.f)
		{
			return EWinReason::TimeExpired;
		}
		if (Collected >= Required)
		{
			return EWinReason::ItemsCollected;
		}
		return HumansWin(Toys, Boxed) ? EWinReason::AllToysBoxed : EWinReason::None;
	}

	struct FVisionAngles
	{
		float Horizontal;
		float Vertical;
	};

	// 幅÷高さ。0 以下は 1 として扱う。
	inline float AspectRatio(int Width, int Height)
	{
		return float(Width > 0 ? Width : 1) / float(Height > 0 ? Height : 1);
	}

	// 視界判定に使う片側の角度（度）。画面の上下左右の端から、余白の分だけ内側にする。
	inline FVisionAngles JudgeHalfAngles(float FieldOfView, int AspectWidth, int AspectHeight, float EdgeMargin)
	{
		constexpr float DegToRad = 3.14159265f / 180.f;
		const float HalfH = FieldOfView * .5f;
		const float Aspect = AspectRatio(AspectWidth, AspectHeight);
		const float HalfV = std::atan(std::tan(HalfH * DegToRad) / Aspect) / DegToRad;
		const float Margin = EdgeMargin > 0.f ? EdgeMargin : 0.f;
		return {HalfH > Margin ? HalfH - Margin : 0.f, HalfV > Margin ? HalfV - Margin : 0.f};
	}
}
