// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "TBTitleMenu.generated.h"

class UTBSession;
class UButton;
class UTextBlock;
class UTexture2D;

/** Temporary UMG menu; observes the existing session subsystem. */
UCLASS()
class TOONSTORY_API UTBTitleMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	enum class EState : uint8
	{
		Story, // 寸劇を全画面で流す。メニューは隠す
		Menu,  // メニューをフェードで出す。後ろでは部屋の様子がループする
	};
	EState GetState() const
	{
		return State;
	}
	// 物語に入る。寸劇が無ければ入らない。
	void EnterStory();
	// 物語を飛ばしてメニューへ。
	void SkipStory();
	// 寸劇を流している間、どのキーでも飛ばせる。テストからもキー入力を流す。
	FReply SendKeyDownForTest(const FKeyEvent& KeyEvent)
	{
		return NativeOnPreviewKeyDown(FGeometry(), KeyEvent);
	}
	float GetMenuOpacity() const;
	bool AreRoomCardsShown() const
	{
		return bRoomCardsShown;
	}

protected:
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& InputEvent) override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
	void BuildPaperLayout();
	// 左側だけ、右へ向かって薄くなる暗幕の画像。
	static UTexture2D* MakeSideShade(UObject* Outer);
	void EnterMenu();
	void StartLoop();
	void SetRoomCardsShown(bool bShown);
	UFUNCTION()
	void StoryFinished();
	UFUNCTION()
	void WatchStory();
	EState State = EState::Menu;
	float MenuFade = 1.f;
	bool bRoomCardsShown = false;
	TWeakObjectPtr<class ALevelSequenceActor> StoryActor;
	UPROPERTY(Transient) TObjectPtr<UWidget> MenuLayer;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> RoomCardWidgets;
	UPROPERTY(Transient) TObjectPtr<UButton> StoryButton;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> ShadeTexture;
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
	// キー名は割り当てで変わるので、開くたびに作り直す。
	void RefreshHelpText();
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HelpToyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HelpHumanText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HelpControlsText;
	UPROPERTY(Transient) TObjectPtr<UWidget> TitleContent;
	UPROPERTY(Transient) TObjectPtr<UWidget> HelpOverlay;
	UPROPERTY(Transient) TObjectPtr<UButton> HelpButton;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseHelpButton;
	// オプション画面（UTBOptionsMenu）を開く。ロビー・試合中のメニューと同じ画面。
	UFUNCTION() void OpenOptions();
	UPROPERTY(Transient) TObjectPtr<UButton> OptionsButton;
	UFUNCTION()
	class UWidget* RoomLabel(FString Item);
	UPROPERTY(Transient)
	TObjectPtr<class UFont> PaperFont;
	UPROPERTY(Transient)
	TObjectPtr<class UFont> LogoFont;
	UPROPERTY(Transient)
	TObjectPtr<class UTexture2D> PaperTexture;
	UFUNCTION() void SelectionChanged(FString Item, ESelectInfo::Type SelectionType);
};
