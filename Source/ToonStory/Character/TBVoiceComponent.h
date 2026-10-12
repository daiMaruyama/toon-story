#pragma once
#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "TBVoiceComponent.generated.h"

class USoundAttenuation;

/** 持ち主のプレイヤーの声を、キャラの位置から距離減衰つきで鳴らす。聞く側の各PCで動く。 */
UCLASS()
class TOONSTORY_API UTBVoiceComponent : public UVOIPTalker
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// この距離で無音になる。
	UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0"))
	float HearingDistance = 1500.f;
	// おもちゃの声は、人間にはかなり近くでしか聞こえない。
	UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0"))
	float ToyToHumanHearingDistance = 400.f;

private:
	FTimerHandle UpdateTimer;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;
	void UpdateVoice();
};
