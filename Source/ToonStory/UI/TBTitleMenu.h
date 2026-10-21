// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "TBTitleMenu.generated.h"

class UTBSession;
class UButton;
class UProgressBar;
class UTextBlock;
enum class ETBVolume : uint8;

/** Temporary UMG menu; observes the existing session subsystem. */
UCLASS()
class TOONSTORY_API UTBTitleMenu : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
	void BuildPaperLayout();
	UPROPERTY(Transient) TObjectPtr<UTBSession> Session;
	UPROPERTY(Transient) TObjectPtr<UButton> HostButton;
	UPROPERTY(Transient) TObjectPtr<UButton> FindButton;
	UPROPERTY(Transient) TObjectPtr<UButton> JoinButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CleanupButton;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> RoomList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UFUNCTION() void Refresh();
	UFUNCTION() void Hover();
	UFUNCTION() void Host();
	UFUNCTION() void Find();
	UFUNCTION() void Join();
	UFUNCTION() void Cleanup();
	UFUNCTION()
	void Quit();
	UFUNCTION()
	void Help();
	UFUNCTION() void CloseHelp();
	UPROPERTY(Transient) TObjectPtr<UWidget> TitleContent;
	UPROPERTY(Transient) TObjectPtr<UWidget> HelpOverlay;
	UPROPERTY(Transient) TObjectPtr<UButton> HelpButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseHelpButton;
	// 音量の設定。ゲーム中のメニュー（ATBHUD）と同じ項目・同じ操作にする。並びは ETBVolume の順。
	UFUNCTION() void OpenAudio();
	UFUNCTION() void CloseAudio();
	UFUNCTION() void ResetAudio();
	UFUNCTION() void MasterDown();
	UFUNCTION() void MasterUp();
	UFUNCTION() void MusicDown();
	UFUNCTION() void MusicUp();
	UFUNCTION() void EffectsDown();
	UFUNCTION() void EffectsUp();
	UFUNCTION() void VoiceDown();
	UFUNCTION() void VoiceUp();
	void StepVolume(ETBVolume Kind, int32 Steps);
	void RefreshAudio();
	UPROPERTY(Transient) TObjectPtr<UWidget> AudioOverlay;
	UPROPERTY(Transient) TObjectPtr<UButton> AudioButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseAudioButton;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> VolumeBars;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> VolumeTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> VolumeDownButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> VolumeUpButtons;
	UFUNCTION()
	class UWidget* RoomLabel(FString Item);
	UPROPERTY(Transient)
	TObjectPtr<class UFont> PaperFont;
	UPROPERTY(Transient)
	TObjectPtr<class UFont> LogoFont;
	UPROPERTY(Transient)
	TObjectPtr<class UTexture2D> PaperTexture;
	UPROPERTY(Transient)
	TObjectPtr<class UTexture2D> BackgroundTexture;
	UFUNCTION() void SelectionChanged(FString Item, ESelectInfo::Type SelectionType);
};
