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
	// この距離までは最大音量。
	UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0"))
	float InnerRadius = 200.f;
	// InnerRadiusの外側で、この距離をかけて無音になる。
	UPROPERTY(EditDefaultsOnly, Category = "Voice", meta = (ClampMin = "0.0"))
	float FalloffDistance = 1500.f;

private:
	FTimerHandle RegisterTimer;
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;
	void TryRegister();
};
