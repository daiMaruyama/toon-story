#pragma once
// UEに依存しないルール計算。ゲーム本体と単体テストで同じ式を使う。
namespace TBRuleMath
{
	// 上昇は止めるが、すでに落下している速度は残す。
	inline float FrozenVerticalSpeed(float Speed)
	{
		return Speed > 0.f ? 0.f : Speed;
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

	// 追加の救助者1人につき、基準速度の50%を加える。
	inline float RescueRate(int N, float Seconds)
	{
		return N > 0 ? (1.f + .5f * (N - 1)) / (Seconds > .1f ? Seconds : .1f) : 0.f;
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
} // namespace TBRuleMath
