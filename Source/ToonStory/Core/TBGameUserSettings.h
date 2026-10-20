// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "TBGameUserSettings.generated.h"

/** プレイヤーが選んだ設定を、PC・ユーザーごとに保存する。範囲と初期値は UTBConfigData から読む。
    ini を手で書き換えられても、ここの Get と Set を通った時点で範囲内になる。
    DefaultEngine.ini の GameUserSettingsClassName にはまだ登録しない（音量を実際の音につなぐときに登録する） */
UCLASS()
class TOONSTORY_API UTBGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
public:
	virtual void SetToDefaults() override;

	float GetMasterVolume() const;
	float GetMusicVolume() const;
	float GetEffectsVolume() const;
	float GetVoiceVolume() const;
	void SetMasterVolume(float Value);
	void SetMusicVolume(float Value);
	void SetEffectsVolume(float Value);
	void SetVoiceVolume(float Value);

private:
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
