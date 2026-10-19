// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "Data/TBTypes.h"
#include "TBVoiceComponent.generated.h"

class USoundAttenuation;
class USoundEffectSourcePresetChain;
class UTBPitchShiftPreset;
class UAudioComponent;
class APlayerController;
class ATBBox;

/** 持ち主のプレイヤーの声を、キャラの位置から距離減衰つきで鳴らす。聞く側の各PCで動く。 */
UCLASS()
class TOONSTORY_API UTBVoiceComponent : public UVOIPTalker
{
	GENERATED_BODY()
public:
	UTBVoiceComponent(const FObjectInitializer& Init);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnTalkingBegin(UAudioComponent* AudioComponent) override;
	virtual void OnTalkingEnd() override;
	// 聞こえる距離・こもり・声の高さはマスターデータ（TB::Rules().Voice）にある。

private:
	FTimerHandle UpdateTimer;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;
	UPROPERTY(Transient)
	TObjectPtr<UTBPitchShiftPreset> Pitch;
	UPROPERTY(Transient)
	TObjectPtr<USoundEffectSourcePresetChain> PitchChain;
	float AppliedSemitones = 0.f;
	float AppliedDelayLength = 0.f;
	// 話している最中の声。エンジンは話し始めにしか設定を読まないので、変化はこれに直接当てる。
	TWeakObjectPtr<UAudioComponent> TalkingAudio;
	// 耳をキャラへ移した相手。終わるときに戻す。
	TWeakObjectPtr<APlayerController> EarsController;
	TWeakObjectPtr<ATBBox> Box;
	// 箱の中の聞き手に向けて、外の声を収納部屋へ写す位置。
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Mirror;
	void UpdateVoice();
	void UpdateMirror();
	void ApplyToTalkingAudio();
	ATBBox* FindBox();
	float DistanceFor(ETBTeam SpeakerTeam, ETBTeam ListenerTeam) const;
	float PitchFor(ETBTeam SpeakerTeam, ETBTeam ListenerTeam) const;
};
