#include "../Source/ToonStory/TBRuleMath.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main()
{
	using namespace TBRuleMath;
	auto Near = [](float A, float B) { return std::fabs(A - B) < 0.0001f; };
	assert(FrozenVerticalSpeed(400) == 0);     // rising stops immediately
	assert(FrozenVerticalSpeed(-400) == -400); // existing fall is not slowed
	assert(FrozenVerticalSpeed(0) == 0);
	float P = AdvanceContact(0, 1, 3, true, false, true);
	assert(Near(P, 1.f / 3));
	P = AdvanceContact(P, 20, 3, true, true, true); // freeze for 20 s grants no progress
	assert(Near(P, 1.f / 3));
	P = AdvanceContact(P, 1, 3, true, false, true);
	assert(Near(P, 2.f / 3));
	assert(AdvanceContact(P, 0, 3, false, true, true) == 0);  // leaving while frozen resets
	assert(AdvanceContact(P, 1, 3, true, false, false) == 0); // carried/boxed reset
	P = AdvanceContact(P, 1, 3, true, false, true);
	assert(Near(P, 1));
	assert(AdvanceContact(.9f, 1, 3, true, false, true) == 1); // cap once
	assert(Near(RescueRate(1, 10), .1f));
	assert(Near(RescueRate(2, 10), .15f));
	assert(Near(RescueRate(3, 10), .2f));
	assert(RescueRate(0, 10) == 0);
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
