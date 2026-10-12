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
	// オフならロビーでは全員の声が距離に関係なく聞こえる。オンならロビーも人間→人間の距離で近接にする。
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Distance")
	bool bProximityInLobby = false;
	// 聞こえる距離。「話す人→聞く人」ごとに、この距離で無音になる。
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Distance", meta = (ClampMin = "0.0"))
	float HumanToHumanDistance = 1500.f;
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Distance", meta = (ClampMin = "0.0"))
	float HumanToToyDistance = 1500.f;
	// おもちゃの声は、人間にはかなり近くでしか聞こえない。
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Distance", meta = (ClampMin = "0.0"))
	float ToyToHumanDistance = 400.f;
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Distance", meta = (ClampMin = "0.0"))
	float ToyToToyDistance = 1500.f;
	// 箱の中と外の間（両方向）。
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Distance", meta = (ClampMin = "0.0"))
	float BoxedDistance = 300.f;
	// 距離に対する音量の落ち方。Customのときは下のカーブ（横軸0〜1が距離の割合、縦軸が音量）を使う。
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Falloff")
	EAttenuationDistanceModel FalloffModel = EAttenuationDistanceModel::Linear;
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Falloff")
	FRuntimeFloatCurve FalloffCurve;
	// 箱越しの声のこもり具合。低いほどこもって聞こえる。
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Box", meta = (ClampMin = "0.0"))
	float BoxedMuffleFrequency = 800.f;
	// 半音単位。人形から聞く人間の声は低く、人間から聞く人形の声は高くして、大きさの差を表す。
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Pitch", meta = (ClampMin = "-12.0", ClampMax = "12.0"))
	float HumanPitchForToys = -4.f;
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Pitch", meta = (ClampMin = "-12.0", ClampMax = "12.0"))
	float ToyPitchForHumans = 4.f;
	// 声の揺れが気になるときに調整する(ms)。
	UPROPERTY(EditDefaultsOnly, Category = "Voice|Pitch", meta = (ClampMin = "10.0", ClampMax = "100.0"))
	float PitchDelayLength = 30.f;

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
