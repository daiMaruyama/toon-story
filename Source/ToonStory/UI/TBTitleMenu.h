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
	// 寸劇を飛ばした直後に挟む黒の濃さ。
	float GetBlackoutOpacity() const
	{
		return BlackoutFade;
	}
	// 寸劇の間、クリックを受け止める板が出ているか。
	bool IsCatchingClicks() const
	{
		return Blackout && Blackout->GetVisibility() == ESlateVisibility::Visible;
	}
	// 放置と、止めたループの再開を、時間を進めて確かめる。
	void AdvanceIdleForTest(float Seconds)
	{
		UpdateIdle(Seconds);
	}

protected:
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& InputEvent) override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

	// 以下は WBP_TitleMenu に置けば、置いたものを使う。名前を合わせる。無ければ C++ が従来どおり作る。
	// ScaleBox（TitleScale）の中の 1280×800 の Canvas（TitleCanvas）に、ボタンなどを置く。
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<class UScaleBox> TitleScale;
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<class UCanvasPanel> TitleCanvas;
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> HostButton;
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> FindButton;
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> HelpButton;
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> OptionsButton;
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> QuitButton;
	// ロゴの画像。ブラシに画像を設定すると、仮の TOON / STORY の文字は出さない。画像が無ければ隠す。
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<class UImage> LogoImage;
	// カーソルを乗せたボタンの左に出す集中線。普段は隠れている。
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<class UImage> HostBurst;
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<class UImage> FindBurst;
	UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<class UImage> HelpBurst;

private:
	void BuildPaperLayout();
	// 乗せているボタンの集中線だけを出す。
	void UpdateBursts();
	void EnterMenu();
	// 寸劇を流さずメニューへ入るとき、寸劇を最後のコマの状態にする。
	void SettleStory();
	void StartLoop();
	// 触ったら、放置の数え直し。
	void NoteInput();
	void UpdateIdle(float DeltaTime);
	bool IsBusyInMenu() const;
	void SetRoomCardsShown(bool bShown);
	UFUNCTION()
	void StoryFinished();
	EState State = EState::Menu;
	float MenuFade = 1.f;
	float BlackoutFade = 0.f;
	// 最後の入力からの秒数。
	float IdleSeconds = 0.f;
	FVector2D LastCursor = FVector2D::ZeroVector;
	bool bRoomCardsShown = false;
	TWeakObjectPtr<class ALevelSequenceActor> StoryActor;
	UPROPERTY(Transient) TObjectPtr<UWidget> Blackout;
	UPROPERTY(Transient) TObjectPtr<UWidget> MenuLayer;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> RoomCardWidgets;
	UPROPERTY(Transient) TObjectPtr<UTBSession> Session;
	UPROPERTY(Transient) TObjectPtr<UButton> JoinButton;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> RoomList;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UFUNCTION() void Refresh();
	UFUNCTION() void Hover();
	UFUNCTION() void Host();
	UFUNCTION() void Find();
	UFUNCTION() void Join();
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
	UPROPERTY(Transient) TObjectPtr<UButton> CloseHelpButton;
	// オプション画面（UTBOptionsMenu）を開く。ロビー・試合中のメニューと同じ画面。
	UFUNCTION() void OpenOptions();
	TWeakObjectPtr<class UTBOptionsMenu> Options;
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
