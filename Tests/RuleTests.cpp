#include "../Source/ToonStory/GamePlay/TBRuleMath.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main()
{
	using namespace TBRuleMath;
	assert(!DarknessProtects(true, 1000000, 3));
	assert(!DarknessProtects(false, 299.0 * 299.0, 3));
	assert(DarknessProtects(false, 300.0 * 300.0, 3));
	assert(DarknessProtects(false, 301.0 * 301.0, 3));
	assert(DarknessProtects(false, 0, 0));
	auto Near = [](float A, float B) { return std::fabs(A - B) < 0.0001f; };
	assert(FrozenVerticalSpeed(400) == 0);     // 上昇は即停止
	assert(FrozenVerticalSpeed(-400) == -400); // 落下速度は維持
	assert(FrozenVerticalSpeed(0) == 0);
	float Step = SmoothStepOffset(0, 40, 0, 12, 60); // 40cmの段差を上った瞬間は、目線を元の高さに残す
	assert(Near(Step, -40));
	Step = SmoothStepOffset(Step, 0, .05f, 12, 60); // 0.05秒で exp(-0.6) 倍まで縮む
	assert(Near(Step, -40 * std::exp(-.6f)));
	assert(std::fabs(SmoothStepOffset(Step, 0, 1, 12, 60)) < .01f); // 時間がたてば追いつく
	// フレームの刻み方によらず同じ速さで追いつく（0.05秒×2回 ＝ 0.1秒×1回）。
	assert(Near(SmoothStepOffset(SmoothStepOffset(-40, 0, .05f, 12, 60), 0, .05f, 12, 60),
	            SmoothStepOffset(-40, 0, .1f, 12, 60)));
	assert(SmoothStepOffset(-40, 0, .2f, 12, 60) < -3); // カクついて間が空いても、一気には戻らない
	assert(Near(SmoothStepOffset(0, 40, .1f, 12, 60), -40)); // フレームが長くても、跳ねたフレームでは目線を動かさない
	// 追いつく途中で次の段に乗ったら、縮めた残りに今回の分を足す。
	assert(Near(SmoothStepOffset(-20, 10, .05f, 12, 60), -20 * std::exp(-.6f) - 10));
	assert(Near(SmoothStepOffset(0, -30, 0, 12, 60), 30));  // 下りも同じ
	assert(Near(SmoothStepOffset(0, 200, 0, 12, 60), -60)); // 大きな変化でも遅れは上限で止める
	assert(Near(SmoothStepOffset(-50, 20, 0, 12, 60), -60)); // たまった分と合わせて超えても上限で止める
	assert(SmoothStepOffset(-50, 20, 0, 12, -1) == 0);       // 上限が負なら遅らせない
	float P = AdvanceContact(0, 1, 3, true, false, true);
	assert(Near(P, 1.f / 3));
	P = AdvanceContact(P, 20, 3, true, true, true); // 凍結中は進捗を保持
	assert(Near(P, 1.f / 3));
	P = AdvanceContact(P, 1, 3, true, false, true);
	assert(Near(P, 2.f / 3));
	assert(AdvanceContact(P, 0, 3, false, true, true) == 0);  // 凍結中でも接触が切れたらリセット
	assert(AdvanceContact(P, 1, 3, true, false, false) == 0); // 運搬・収納中はリセット
	P = AdvanceContact(P, 1, 3, true, false, true);
	assert(Near(P, 1));
	assert(AdvanceContact(.9f, 1, 3, true, false, true) == 1);
	assert(Near(RescueRate(1, 10, .5f), .1f));
	assert(Near(RescueRate(2, 10, .5f), .15f));
	assert(Near(RescueRate(3, 10, .5f), .2f));
	assert(RescueRate(0, 10, .5f) == 0);
	assert(Near(RescueRate(3, 10, 0.f), .1f));
	assert(Near(RescueRate(3, 10, 1.f), .3f));
	assert(Near(RescueRate(3, 10, -.5f), .1f));
	assert(!HumansWin(0, 0));
	assert(!HumansWin(2, 1));
	assert(HumansWin(2, 2));
	assert(EvaluateWin(0, 0, 5, 2, 0) == EWinReason::TimeExpired);
	assert(EvaluateWin(-1, 5, 5, 2, 2) == EWinReason::TimeExpired);
	assert(EvaluateWin(1, 5, 5, 2, 2) == EWinReason::ItemsCollected);
	assert(EvaluateWin(1, 0, 5, 2, 2) == EWinReason::AllToysBoxed);
	assert(EvaluateWin(1, 0, 5, 2, 1) == EWinReason::None);
	std::cout << "PASS: freeze, step camera, contact, rescue, timeout priority and boxing victory\n";
}
