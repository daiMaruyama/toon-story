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
	constexpr float Deg = 3.14159265f / 180;
	const FVisionAngles Vision = JudgeHalfAngles(90, 16, 9, 5);
	assert(Near(Vision.Horizontal, 40)); // 移行前の GazeHalfAngle と同じ
	assert(Near(Vision.Vertical, std::atan(std::tan(45 * Deg) / (16.f / 9)) / Deg - 5)); // 移行前の Watched の式と同じ
	const FVisionAngles Wide = JudgeHalfAngles(90, 21, 9, 5);
	assert(Near(Wide.Horizontal, 40) && Wide.Vertical < Vision.Vertical); // 横長の画面では縦だけ狭くなる
	assert(JudgeHalfAngles(90, 16, 9, 60).Horizontal == 0); // 余白が大きくてもマイナスにならない
	assert(JudgeHalfAngles(90, 16, 9, 60).Vertical == 0);
	assert(std::isfinite(JudgeHalfAngles(90, 0, 0, 5).Vertical)); // 比率が 0 でも壊れない
	assert(Near(AspectRatio(16, 9), 16.f / 9)); // カメラと判定が同じ比率を使う
	assert(AspectRatio(0, -1) == 1);
	// 画面比に合わせる（Hor+）。縦は16:9のときと同じまま、横だけ変わる。
	const float Ultra = 21.f / 9;
	assert(Near(FieldOfViewAtAspect(90, 16.f / 9, 16.f / 9), 90)); // 基準と同じ画面比なら視野角はそのまま
	assert(Near(std::tan(FieldOfViewAtAspect(90, 16.f / 9, 4.f / 3) * Deg / 2), .75f)); // 4:3 では横が狭い
	// 判定の横は、基準より横長の画面（黒帯で基準の幅に止まる）や、嘘で広く送られた値でも広げない。
	const FVisionAngles OnUltra = JudgeHalfAngles(90, 16, 9, 5, Ultra);
	assert(Near(OnUltra.Horizontal, Vision.Horizontal) && Near(OnUltra.Vertical, Vision.Vertical));
	assert(Near(JudgeHalfAngles(90, 16, 9, 5, 32.f / 9).Horizontal, Vision.Horizontal));
	// 基準より狭い画面（MacBook の 16:10、4:3）では、映っている範囲まで狭める。縦は同じ。
	const FVisionAngles OnMac = JudgeHalfAngles(90, 16, 9, 5, 16.f / 10);
	assert(OnMac.Horizontal < Vision.Horizontal && Near(OnMac.Vertical, Vision.Vertical));
	const FVisionAngles OnClassic = JudgeHalfAngles(90, 16, 9, 5, 4.f / 3);
	assert(OnClassic.Horizontal < OnMac.Horizontal && Near(OnClassic.Vertical, Vision.Vertical));
	assert(Near(std::tan((OnClassic.Horizontal + 5) * Deg), .75f)); // 4:3 の画面に映る横の端
	assert(!HumansWin(0, 0));
	assert(!HumansWin(2, 1));
	assert(HumansWin(2, 2));
	assert(EvaluateWin(0, 0, 5, 2, 0) == EWinReason::TimeExpired);
	assert(EvaluateWin(-1, 5, 5, 2, 2) == EWinReason::TimeExpired);
	assert(EvaluateWin(1, 5, 5, 2, 2) == EWinReason::ItemsCollected);
	assert(EvaluateWin(1, 0, 5, 2, 2) == EWinReason::AllToysBoxed);
	assert(EvaluateWin(1, 0, 5, 2, 1) == EWinReason::None);
	// 凝視の視野の寄り具合。0.25秒で寄りきり、離すと同じ時間で戻る。行き過ぎない。
	assert(Near(AdvanceZoom(0, true, .1f, .25f), .4f));
	assert(AdvanceZoom(.9f, true, .1f, .25f) == 1);
	assert(Near(AdvanceZoom(1, false, .1f, .25f), .6f));
	assert(AdvanceZoom(.1f, false, .1f, .25f) == 0);
	assert(AdvanceZoom(0, true, .25f, .25f) == 1);   // ちょうど0.25秒で寄りきる
	assert(AdvanceZoom(.5f, true, -1, .25f) == .5f); // 時間が負なら動かさない
	assert(AdvanceZoom(.3f, true, .01f, 0) == 1);    // 0秒なら即座に切り替える
	assert(AdvanceZoom(.3f, false, .01f, 0) == 0);
	// 凝視の範囲は中心から12°の円。奥行き100で、ずれがtan12°×100≒21.3までなら内側。
	assert(InGazeCone(100, 0, 0, 12));
	assert(InGazeCone(100, 21, 0, 12));
	assert(!InGazeCone(100, 22, 0, 12));
	assert(InGazeCone(100, 0, -21, 12));                 // 縦も同じ
	assert(!InGazeCone(100, 16, 16, 12));                // 斜めは円の外（四角なら内側）
	assert(!InGazeCone(-100, 0, 0, 12));                 // 後ろは対象外
	assert(!InGazeCone(100, 0, 0, 0));                   // 角度0なら誰も凝視できない
	// 状態の進め方。初期値は止まるまで0.3秒、止まったあとは視界に入っている間止まったまま。
	using G = EGazeState;
	FGazeStep S = AdvanceGaze({}, true, false, .1f, .3f, false);
	assert(S.State == G::Seen);                           // 視界に入っただけでは止まらない
	S = AdvanceGaze(S, true, true, .1f, .3f, false);
	assert(S.State == G::Gazed && Near(S.GazedSeconds, .1f));
	S = AdvanceGaze(S, true, true, .1f, .3f, false);
	assert(S.State == G::Gazed);
	S = AdvanceGaze(S, true, true, .1f, .3f, false);
	assert(S.State == G::Frozen && S.GazedSeconds == 0); // 0.3秒で止まる
	FGazeStep Broken = AdvanceGaze({G::Gazed, .2f}, true, false, .1f, .3f, false);
	assert(Broken.State == G::Seen && Broken.GazedSeconds == 0); // 途中で外れたら止めずに戻す
	assert(AdvanceGaze(Broken, true, true, .1f, .3f, false).State == G::Gazed); // 数え直し
	assert(AdvanceGaze({G::Gazed, .2f}, false, false, .1f, .3f, false).State == G::None);
	assert(AdvanceGaze({}, false, true, .3f, .3f, false).State == G::Frozen); // 1回で0.3秒進めば止まる
	assert(AdvanceGaze({}, false, true, 0, 0, false).State == G::Frozen);     // 0秒なら即止まる
	// 解除(a): 凝視が外れても、視界に入っている間は止まったまま。視界から外れたら解く。
	const FGazeStep Frozen{G::Frozen, 0};
	assert(AdvanceGaze(Frozen, true, false, .1f, .3f, false).State == G::Frozen);
	assert(AdvanceGaze(Frozen, false, false, .1f, .3f, false).State == G::None);
	// 解除(b): 凝視が外れたら、視界に入っていても解く。
	assert(AdvanceGaze(Frozen, true, true, .1f, .3f, true).State == G::Frozen);
	assert(AdvanceGaze(Frozen, true, false, .1f, .3f, true).State == G::Seen);
	assert(AdvanceGaze(Frozen, false, false, .1f, .3f, true).State == G::None);
	// 演出の濃さ。凝視されたら速く、離れたらゆっくり。行き過ぎない。
	assert(Near(AdvanceShade(0, 1, .05f, .1f, .5f), .5f));
	assert(AdvanceShade(.9f, 1, .05f, .1f, .5f) == 1);
	assert(Near(AdvanceShade(1, 0, .05f, .1f, .5f), .9f));
	assert(AdvanceShade(.36f, .35f, .05f, .1f, .5f) == .35f);
	assert(AdvanceShade(0, 1, .01f, 0, 0) == 1); // 0秒なら即座に切り替える
	assert(AdvanceShade(.5f, 1, -1, .1f, .5f) == .5f);
	assert(GazeShadowTarget(G::None) == 0);
	assert(GazeShadowTarget(G::Seen) > 0 && GazeShadowTarget(G::Seen) < GazeShadowTarget(G::Gazed));
	assert(GazeShadowTarget(G::Frozen) == 1);
	assert(ResultsViewBefore(false, false, 9, true, true, 1)); // 箱の外の子が、箱の中の自分より先
	assert(!ResultsViewBefore(true, true, 1, false, false, 9));
	assert(ResultsViewBefore(false, true, 9, false, false, 1)); // 箱の外どうしなら自分が先
	assert(ResultsViewBefore(true, false, 1, true, false, 2)); // あとは番号の小さい順
	assert(!ResultsViewBefore(true, false, 2, true, false, 2));
	std::cout << "PASS: results view order\n";
	std::cout << "PASS: freeze, step camera, contact, rescue, timeout priority and boxing victory\n";
	std::cout << "PASS: gaze cone, gaze state and gaze shade\n";
}
