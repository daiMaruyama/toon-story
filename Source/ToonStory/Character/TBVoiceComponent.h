#pragma once
#include "CoreMinimal.h"
#include "Net/VoiceConfig.h"
#include "TBVoiceComponent.generated.h"

class USoundAttenuation;
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
	// この距離で無音になる。
	UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0"))
	float HearingDistance = 1500.f;
	// おもちゃの声は、人間にはかなり近くでしか聞こえない。
	UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0"))
	float ToyToHumanHearingDistance = 400.f;
	// 箱の中と外の間の声は、箱の近くでしか聞こえない。
	UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0"))
	float BoxedHearingDistance = 300.f;
	// 箱越しの声のこもり具合。低いほどこもって聞こえる。
	UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0"))
	float BoxedMuffleFrequency = 800.f;

private:
	FTimerHandle UpdateTimer;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;
	TWeakObjectPtr<ATBBox> Box;
	// 箱の中の聞き手に向けて、外の声を収納部屋へ写す位置。
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Mirror;
	void UpdateVoice();
	void UpdateMirror();
	ATBBox* FindBox();
};
