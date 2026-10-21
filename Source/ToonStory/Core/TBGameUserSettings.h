// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Data/TBConfigData.h"
#include "TBGameUserSettings.generated.h"

class UGameInstance;
class UTBAudioSubsystem;

/** プレイヤーが選んだ設定を、PC・ユーザーごとに保存する。範囲と初期値は UTBConfigData から読む。
    ini を手で書き換えられても、ここの Get と Set を通った時点で範囲内になる。
    DefaultEngine.ini の GameUserSettingsClassName で登録してあり、エンジンはこのクラスを使う */
UCLASS()
class TOONSTORY_API UTBGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
public:
	// エンジンが使っている設定。
	static UTBGameUserSettings* Get();
	virtual void SetToDefaults() override;

	float GetVolume(ETBVolume Kind) const;
	void SetVolume(ETBVolume Kind, float Value);
	// データの刻みで上げ下げする（Steps が正なら上げる）。
	void StepVolume(ETBVolume Kind, int32 Steps);
	// 音量だけを「まだ選んでいない」（データの初期値）に戻す。
	void ResetVolumes();
	// 今の音量で鳴らす。
	void ApplyVolumes(UTBAudioSubsystem& Audio) const;
	// 今の音量で鳴らし、保存する。設定画面で変えたときに呼ぶ。
	void CommitVolumes(const UGameInstance* GameInstance);

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
};
