// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Data/TBConfigData.h"
#include "TBGameUserSettings.generated.h"

class UGameInstance;
class UTBAudioSubsystem;

/** プレイヤーが選んだ設定を、PC・ユーザーごとに保存する。範囲と初期値は UTBConfigData から読む。
    ini を手で書き換えられても、ここの Get と Set、ApplyNonResolutionSettings を通った時点で範囲内になる。
    DefaultEngine.ini の GameUserSettingsClassName で登録してあり、エンジンはこのクラスを使う */
UCLASS()
class TOONSTORY_API UTBGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
public:
	// エンジンが使っている設定。
	static UTBGameUserSettings* Get();
	virtual void SetToDefaults() override;
	// 画質の下限・描画解像度の上限・明るさ・モーションブラーも、ここで効かせる。起動時にもエンジンが呼ぶ。
	virtual void ApplyNonResolutionSettings() override;

	// 音
	float GetVolume(ETBVolume Kind) const;
	void SetVolume(ETBVolume Kind, float Value);
	// データの刻みで上げ下げする（Steps が正なら上げる）。
	void StepVolume(ETBVolume Kind, int32 Steps);
	// 音量だけを「まだ選んでいない」（データの初期値）に戻す。
	void ResetVolumes();
	// 今の音量で鳴らす。
	void ApplyVolumes(UTBAudioSubsystem& Audio) const;
	// 今の音量で鳴らすが、保存はしない。スライダーを動かしている途中に呼ぶ。
	void PreviewVolumes(const UGameInstance* GameInstance) const;
	// 今の音量で鳴らし、保存する。設定画面で変えたときに呼ぶ。
	void CommitVolumes(const UGameInstance* GameInstance);

	// 画面。解像度・画面モード・VSync・fps の上限はエンジンの設定をそのまま使う。
	// 0 低 / 1 中 / 2 高 / 3 最高。選んでおらず、エンジンの画質が段階どうしで揃っていなければ -1。
	int32 GetQualityPreset() const;
	void SetQualityPreset(int32 Level);
	float GetResolutionScalePercent() const;
	void StepResolutionScale(int32 Steps);
	// データの刻みにそろえ、範囲に収めて入れる（スライダー用）。
	void SetResolutionScalePercent(float Percent);
	float GetBrightness() const;
	void StepBrightness(int32 Steps);
	void SetBrightness(float Gamma);
	// 明るさだけをすぐ画面に出す。保存も、ほかの画面の設定の反映もしない。
	void PreviewBrightness() const;
	bool IsMotionBlurEnabled() const;
	void SetMotionBlurEnabled(bool bEnabled);
	// 光の計算（Lumen）にレイトレーシングを使うか。この PC の GPU が対応していなければ選べない。
	static bool IsRayTracingSupported();
	bool IsRayTracingOn() const;
	void SetRayTracingOn(bool bOn);
	// データの選択肢の中で、fps の上限を前後に動かす。
	int32 GetFrameRateLimitIndex() const;
	void StepFrameRateLimit(int32 Steps);
	// 解像度と画面モード以外の画面の設定を、データの初期値に戻す。
	void ResetDisplay();
	// 画質の下限と描画解像度の上限を、今の値に効かせる。
	void EnforceLimits();
	// 画面の設定を効かせて、保存する。
	void CommitDisplay();

	// 操作
	float GetMouseSensitivity() const;
	void StepMouseSensitivity(int32 Steps);
	void SetMouseSensitivity(float Value);
	bool IsMouseYInverted() const;
	void SetMouseYInverted(bool bInverted);
	void ResetControls();
	// 視点の入力に掛ける倍率。X は左右、Y は上下（上下反転なら負）。
	FVector2D GetLookScale() const;

private:
	float& Saved(ETBVolume Kind);
	float Saved(ETBVolume Kind) const;
	// 負の値は「まだ選んでいない」。そのあいだはデータの初期値を使うので、あとから初期値を変えても、
	// 選んでいない人にはそれが効く。
	UPROPERTY(Config)
	float MasterVolume = -1.f;
	UPROPERTY(Config)
	float MusicVolume = -1.f;
	UPROPERTY(Config)
	float EffectsVolume = -1.f;
	UPROPERTY(Config)
	float VoiceVolume = -1.f;
	UPROPERTY(Config)
	int32 QualityPreset = -1;
	UPROPERTY(Config)
	float Brightness = -1.f;
	UPROPERTY(Config)
	int32 MotionBlur = -1;
	UPROPERTY(Config)
	int32 RayTracing = -1;
	UPROPERTY(Config)
	float MouseSensitivity = -1.f;
	UPROPERTY(Config)
	int32 InvertMouseY = -1;
};
