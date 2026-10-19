#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Audio/TBAudioSettings.h"
#include "TBAudioSubsystem.generated.h"

class UAudioComponent;
class USceneComponent;

/** Local audio only. GameInstance owns preferences; playback belongs to the current world. */
UCLASS()
class TOONSTORY_API UTBAudioSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bInitialized && !IsTemplate(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTBAudioSubsystem, STATGROUP_Tickables); }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	UFUNCTION(BlueprintCallable, Category = "Toon Story|Audio")
	void PlaySoundEvent(ETBSoundEvent Event, FVector Location = FVector::ZeroVector, USceneComponent* AttachTo = nullptr);
	UFUNCTION(BlueprintCallable, Category = "Toon Story|Audio")
	UAudioComponent* StartLoop(ETBSoundEvent Event, USceneComponent* AttachTo);
	UFUNCTION(BlueprintCallable, Category = "Toon Story|Audio")
	void StopLoop(UAudioComponent* Audio);
	UFUNCTION(BlueprintCallable, Category = "Toon Story|Audio")
	void ReloadAudioSettings();
	// Survives map cleanup; used for room closure notices after returning to Title.
	void QueueTravelSound(ETBSoundEvent Event);
	uint32 GetSettingsRevision() const { return SettingsRevision; }
	void TrackVoice(UAudioComponent* Audio);
	void UntrackVoice(UAudioComponent* Audio);
	// Local listener modifiers, keyed by source so stopping one device leaves the others active.
	void SetToyActionMask(AActor* Source, float Multiplier);
	void RemoveToyActionMask(AActor* Source);
	float GetToyActionMask() const;
	UFUNCTION(BlueprintPure, Category = "Toon Story|Audio")
	ETBMusicState GetMusicState() const { return MusicState; }
	UFUNCTION(BlueprintPure, Category = "Toon Story|Audio")
	float GetCategoryVolume(ETBAudioCategory Category) const;
	UFUNCTION(BlueprintCallable, Category = "Toon Story|Audio")
	void SetMasterVolume(float Value);
	UFUNCTION(BlueprintCallable, Category = "Toon Story|Audio")
	void SetMusicVolume(float Value);
	UFUNCTION(BlueprintCallable, Category = "Toon Story|Audio")
	void SetEffectsVolume(float Value);
	UFUNCTION(BlueprintCallable, Category = "Toon Story|Audio")
	void SetVoiceVolume(float Value);
	UFUNCTION(BlueprintPure, Category = "Toon Story|Audio")
	float GetMasterVolume() const { return MasterVolume; }
	UFUNCTION(BlueprintPure, Category = "Toon Story|Audio")
	float GetMusicVolume() const { return MusicVolume; }
	UFUNCTION(BlueprintPure, Category = "Toon Story|Audio")
	float GetEffectsVolume() const { return EffectsVolume; }
	UFUNCTION(BlueprintPure, Category = "Toon Story|Audio")
	float GetVoiceVolume() const { return VoiceVolume; }

private:
	struct FPlayingSound
	{
		TWeakObjectPtr<UAudioComponent> Component;
		ETBAudioCategory Category = ETBAudioCategory::Effects;
		ETBSoundEvent Event = ETBSoundEvent::UIClick;
		float BaseVolume = 1.f;
		bool bLoop = false;
		bool bToyActionSound = false;
	};
	TArray<FPlayingSound> PlayingSounds;
	TMap<TWeakObjectPtr<AActor>, float> ToyActionMasks;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> OwnedComponents;
	UPROPERTY(Transient)
	TMap<ETBMusicState, FTBAudioClip> MusicClips;
	UPROPERTY(Transient)
	TMap<ETBSoundEvent, FTBAudioClip> SoundClips;
	// Keep loaded soft references alive; load once per registration, never on every footstep.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> LoadedAssets;
	TWeakObjectPtr<UWorld> PlaybackWorld;
	TWeakObjectPtr<UAudioComponent> MusicComponent;
	FDelegateHandle CleanupHandle;
	ETBMusicState MusicState = ETBMusicState::None;
	float MasterVolume = 1.f, MusicVolume = 1.f, EffectsVolume = 1.f, VoiceVolume = 1.f;
	float MusicFadeSeconds = .5f;
	float HumanFootstepToyVolumeMultiplier = 1.75f;
	float HumanFootstepToyPitchMultiplier = .75f;
	uint32 SettingsRevision = 0;
	bool bInitialized = false, bSawPlaying = false, bResultCuePlayed = false;
	TOptional<ETBSoundEvent> TravelSound;
	TWeakObjectPtr<UWorld> TravelFromWorld;
	int32 LastLobbyCount = -1, LastRemainingSecond = -1;
	double LastErrorTime = -1000;
	void ChangeMusic(ETBMusicState Next);
	void StopOwnedAudio();
	void ReleaseVoiceTracking();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void ApplyVolumes();
	void EnsurePlaybackWorld(UWorld* World);
	UAudioComponent* CreateAudio(const FTBAudioClip& Clip, ETBAudioCategory Category,
	                            ETBSoundEvent Event, FVector Location, USceneComponent* AttachTo, bool bLoop);
};
