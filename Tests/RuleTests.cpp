#include "../Source/ToonStory/GamePlay/TBRuleMath.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main()
{
	using namespace TBRuleMath;
	auto Near = [](float A, float B) { return std::fabs(A - B) < 0.0001f; };
	assert(FrozenVerticalSpeed(400) == 0);     // 上昇は即停止
	assert(FrozenVerticalSpeed(-400) == -400); // 落下速度は維持
	assert(FrozenVerticalSpeed(0) == 0);
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
	std::cout << "PASS: freeze, contact, rescue, timeout priority and boxing victory\n";
}
