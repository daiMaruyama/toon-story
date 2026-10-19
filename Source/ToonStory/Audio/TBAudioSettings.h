#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Audio/TBAudioTypes.h"
#include "TBAudioSettings.generated.h"

class USoundBase;
class USoundAttenuation;
class USoundConcurrency;

/** Asset references are optional. Put registered assets under /Game/Audio for packaged builds. */
USTRUCT(BlueprintType)
struct TOONSTORY_API FTBAudioClip
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> Sound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0", ClampMax = "4"))
	float Volume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.25", ClampMax = "4"))
	float Pitch = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	bool bSpatial = true;
	// Opt in for toy movement/actions, including future events. UI/team notifications stay unmasked.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	bool bToyActionSound = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundAttenuation> Attenuation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundConcurrency> Concurrency;
	// Used when no attenuation asset is assigned (Unreal units).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "1"))
	float MaxDistance = 1800.f;
	// Per-event safety limit on this machine, also applies when no concurrency asset is assigned.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxInstances = 8;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Toon Story Audio"))
class TOONSTORY_API UTBAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UTBAudioSettings();
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	TMap<ETBMusicState, FTBAudioClip> Music;
	UPROPERTY(Config, EditAnywhere, Category = "Sound Effects")
	TMap<ETBSoundEvent, FTBAudioClip> Sounds;
	UPROPERTY(Config, EditAnywhere, Category = "Music", meta = (ClampMin = "0", ClampMax = "5"))
	float MusicFadeSeconds = .5f;
	// One human footstep asset is shared; only Toy listeners receive these multipliers.
	UPROPERTY(Config, EditAnywhere, Category = "Human Footsteps", meta = (ClampMin = "1", ClampMax = "4"))
	float HumanFootstepToyVolumeMultiplier = 1.75f;
	UPROPERTY(Config, EditAnywhere, Category = "Human Footsteps", meta = (ClampMin = "0.25", ClampMax = "1"))
	float HumanFootstepToyPitchMultiplier = .75f;
};
