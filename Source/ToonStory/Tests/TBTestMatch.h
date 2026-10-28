// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Core/TBGameState.h"

namespace TBTest
{
	// 準備の済んだ試合中にして、抜けるときに元へ戻す。
	// Phaseだけ入れると、準備の情報が無いので準備中扱いになり、キャラが動けない。
	// 戻さないと、ロビーなのに準備の期限が有効という状態が、後のテストへ流れる。
	// 待機コマンドの途中で使うときは、TOptionalでコマンドのメンバーに持たせ、毎フレームHoldする。
	class FPlayingScope
	{
	public:
		// InEndTimeを渡すと、EndTimeもその値にして、抜けるときに戻す。
		explicit FPlayingScope(ATBGameState& InState, TOptional<double> InEndTime = {})
		    : State(&InState), Phase(InState.Phase), Preparation(InState.HidePreparation), EndTime(InState.EndTime)
		{
			Hold(InEndTime);
		}
		FPlayingScope(const FPlayingScope&) = delete;
		FPlayingScope& operator=(const FPlayingScope&) = delete;
		~FPlayingScope()
		{
			if (ATBGameState* Live = State.Get())
			{
				Live->Phase = Phase;
				Live->HidePreparation = Preparation;
				if (bRestoreEndTime) Live->EndTime = EndTime;
			}
		}
		// ゲーム側が試合を終わらせるテストで、毎フレーム試合中へ戻す。元の値は控えたまま。
		void Hold(TOptional<double> InEndTime = {})
		{
			ATBGameState* Live = State.Get();
			if (!Live) return;
			Live->Phase = ETBPhase::Playing;
			Live->HidePreparation.bValid = true;
			Live->HidePreparation.bHiding = false;
			if (InEndTime.IsSet())
			{
				Live->EndTime = InEndTime.GetValue();
				bRestoreEndTime = true;
			}
		}

	private:
		TWeakObjectPtr<ATBGameState> State;
		ETBPhase Phase;
		FTBHidePreparation Preparation;
		double EndTime;
		bool bRestoreEndTime = false;
	};

	// 戻さない版。ゲーム側が最後にロビーへ戻すテストだけで使う（戻すと、ゲームが戻した状態を壊す）。
	inline void BeginPlaying(ATBGameState& State)
	{
		State.Phase = ETBPhase::Playing;
		State.HidePreparation.bValid = true;
		State.HidePreparation.bHiding = false;
	}
}
