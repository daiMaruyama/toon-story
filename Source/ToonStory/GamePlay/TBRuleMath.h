// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include <cmath>
// UEに依存しないルール計算。ゲーム本体と単体テストで同じ式を使う。
namespace TBRuleMath
{
	// UE distances are centimeters. At exactly N meters the toy is protected.
	inline bool DarknessProtects(bool LightsOn, double DistanceSquaredCm, float DistanceMeters)
	{
		const double DistanceCm = (DistanceMeters > 0.f ? DistanceMeters : 0.f) * 100.0;
		return !LightsOn && DistanceSquaredCm >= DistanceCm * DistanceCm;
	}

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

	// 基準の画面比 BaseAspect で横の視野角が FieldOfView のとき、縦の視野をそのままに画面比 ViewAspect の画面で
	// 映る横の視野角（度）。
	inline float FieldOfViewAtAspect(float FieldOfView, float BaseAspect, float ViewAspect)
	{
		constexpr float DegToRad = 3.14159265f / 180.f;
		const float Base = BaseAspect > 0.f ? BaseAspect : 1.f;
		const float View = ViewAspect > 0.f ? ViewAspect : Base;
		return 2.f * std::atan(std::tan(FieldOfView * .5f * DegToRad) / Base * View) / DegToRad;
	}

	// 視界判定に使う片側の角度（度）。上下左右とも、画面の端から余白の分だけ内側にする。
	// カメラは縦の視野を基準の画面比のときにそろえ、狭い画面は横だけ狭く映す（エンジンの初期設定の Hor+）。
	// 基準より横長の画面は黒帯で基準の画面比に止める。判定もこれに合わせ、縦は一定、横は画面比 ViewAspect が
	// 基準より狭ければ映っている範囲まで狭め、基準より広くはしない。画面比はクライアントが送る値なので、広げる方向に
	// 使うと嘘で得をできてしまう。ViewAspect が 0 なら基準の画面比で見ているとみなす。
	inline FVisionAngles JudgeHalfAngles(float FieldOfView, int AspectWidth, int AspectHeight, float EdgeMargin,
	                                     float ViewAspect = 0.f)
	{
		constexpr float DegToRad = 3.14159265f / 180.f;
		const float Base = AspectRatio(AspectWidth, AspectHeight);
		const float View = ViewAspect > 0.f && ViewAspect < Base ? ViewAspect : Base;
		const float HalfH = FieldOfViewAtAspect(FieldOfView, Base, View) * .5f;
		const float HalfV = std::atan(std::tan(FieldOfView * .5f * DegToRad) / Base) / DegToRad;
		const float Margin = EdgeMargin > 0.f ? EdgeMargin : 0.f;
		return {HalfH > Margin ? HalfH - Margin : 0.f, HalfV > Margin ? HalfV - Margin : 0.f};
	}

	// 寄り具合Alpha（0〜1）を、Onなら1へ、そうでなければ0へ、Seconds秒で一定の速さで動かす。0秒なら即座に切り替える。
	inline float AdvanceZoom(float Alpha, bool On, float DeltaSeconds, float Seconds)
	{
		const float Target = On ? 1.f : 0.f;
		if (Seconds <= 0.f)
		{
			return Target;
		}
		const float Step = (DeltaSeconds > 0.f ? DeltaSeconds : 0.f) / Seconds;
		const float Next = On ? Alpha + Step : Alpha - Step;
		return Next > 1.f ? 1.f : (Next < 0.f ? 0.f : Next);
	}

	// 凝視の範囲。視線の奥行きDepthと、横・縦のずれから、中心からHalfAngle度の円の内側かを調べる。
	inline bool InGazeCone(float Depth, float Side, float Up, float HalfAngle)
	{
		constexpr float DegToRad = 3.14159265f / 180.f;
		if (Depth <= 0.f || HalfAngle <= 0.f)
		{
			return false;
		}
		const float Tan = std::tan((HalfAngle < 89.f ? HalfAngle : 89.f) * DegToRad);
		return Side * Side + Up * Up <= Depth * Depth * Tan * Tan;
	}

	// おもちゃが見られている度合い。Seenは視界に入っているだけ、Gazedは凝視されて止まる前。
	enum class EGazeState : unsigned char
	{
		None,
		Seen,
		Gazed,
		Frozen
	};

	struct FGazeStep
	{
		EGazeState State = EGazeState::None;
		// 凝視され続けている時間。Gazedの間だけ数える。
		float GazedSeconds = 0.f;
	};

	// 1回分の評価で状態を進める。Seenは誰かの視界に入っている、Gazedは誰かに凝視されている。
	// 凝視がFreezeDelaySeconds続くと止まる。途中で外れたら止めずに戻す。
	// 止まったあとは、ReleaseWhenGazeEndsなら凝視が外れたら、そうでなければ視界から外れたら解く。
	inline FGazeStep AdvanceGaze(FGazeStep Current, bool Seen, bool Gazed, float DeltaSeconds,
	                             float FreezeDelaySeconds, bool ReleaseWhenGazeEnds)
	{
		const bool Visible = Seen || Gazed;
		if (Current.State == EGazeState::Frozen && (ReleaseWhenGazeEnds ? Gazed : Visible))
		{
			return {EGazeState::Frozen, 0.f};
		}
		if (Gazed && Current.State != EGazeState::Frozen)
		{
			const float Elapsed = Current.GazedSeconds + (DeltaSeconds > 0.f ? DeltaSeconds : 0.f);
			return Elapsed >= FreezeDelaySeconds ? FGazeStep{EGazeState::Frozen, 0.f}
			                                     : FGazeStep{EGazeState::Gazed, Elapsed};
		}
		return {Visible ? EGazeState::Seen : EGazeState::None, 0.f};
	}

	// 演出の濃さ（0〜1）をTargetへ一定の速さで近づける。濃くするときはRiseSeconds、薄くするときはFallSeconds秒で全幅を動く。
	inline float AdvanceShade(float Shade, float Target, float DeltaSeconds, float RiseSeconds, float FallSeconds)
	{
		const float Seconds = Target > Shade ? RiseSeconds : FallSeconds;
		if (Seconds <= 0.f)
		{
			return Target;
		}
		const float Step = (DeltaSeconds > 0.f ? DeltaSeconds : 0.f) / Seconds;
		return Target > Shade ? (Shade + Step < Target ? Shade + Step : Target)
		                      : (Shade - Step > Target ? Shade - Step : Target);
	}

	// おもちゃの画面の端に寄せる紙の影の濃さ。視界に入っているだけなら薄く、凝視されたら寄りきる。
	inline float GazeShadowTarget(EGazeState State)
	{
		return State == EGazeState::None ? 0.f : State == EGazeState::Seen ? .5f : 1.f;
	}
}
