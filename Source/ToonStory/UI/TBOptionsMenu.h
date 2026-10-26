// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GenericPlatform/GenericWindow.h"
#include "InputCoreTypes.h"
#include "TBOptionsMenu.generated.h"

namespace TB
{
	struct FKeyPrompt;
}

class APlayerController;
class UButton;
class UFont;
class UScrollBox;
class UTBGameUserSettings;
class UTBKeyRow;
class UTBOptionRow;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

/** オプション画面（音・画面・操作）。タイトル・ロビー・試合中のメニューから、同じ画面を開く。
    値の範囲と初期値は UTBConfigData、プレイヤーが選んだ値は UTBGameUserSettings にある */
UCLASS()
class TOONSTORY_API UTBOptionsMenu : public UUserWidget
{
	GENERATED_BODY()
public:
	// 画面の手前に開く。閉じたら OnClosed を呼ぶ（開いた側がフォーカスを戻す）。
	static UTBOptionsMenu* Open(APlayerController* Player, TFunction<void()> OnClosed = nullptr);
	// 閉じるボタンと Esc。
	UFUNCTION()
	void Close();
	// 試合が終わったときなど、音を鳴らさずに閉じる。
	void Dismiss();

	// ページを作る側（TBOptionsPages）が使う。PageIndex は TBOptionsPage の値。
	UTBOptionRow* AddRow(int32 PageIndex, const FString& Label, bool bBar);
	void AddNote(int32 PageIndex, const FString& Text);
	// キー設定の見出しと行。行は割り当てを毎回引き直す。
	void AddHeading(int32 PageIndex, const FString& Text);
	UTBKeyRow* AddKeyRow(int32 PageIndex, const FString& Label, FName MappingName);
	const TArray<TObjectPtr<UTBKeyRow>>& GetKeyRows() const
	{
		return KeyRows;
	}
	// 解像度と画面モードは、変えたあと確認がなければ元に戻す。
	void ChangeVideoMode(TFunctionRef<void(UTBGameUserSettings&)> Change);
	// タブの切り替え。行は引き直される。
	void ShowPage(int32 Index);

	// キーの付け替え。「変える」で待ちに入り、次のキー（マウスのボタンも）を受け取る。テストからも使う。
	void BeginKeyCapture(FName MappingName);
	void ReceiveCapturedKey(const FKey& Key);
	void ConfirmKeySwap();
	// 行を既定のキーに戻す。かぶるときは入れ替えを聞く（確認の板はキー待ちと同じ）。
	void BeginKeyReset(FName MappingName);
	// キー割り当ての保存。nullptr 以外ならそれを呼ぶ（テストが本人の保存ファイルを書き換えないための差し替え）。
	static TFunction<void()> KeySaveOverride;
	bool IsCapturing() const
	{
		return KeyCapture != EKeyCapture::None;
	}
	bool IsAwaitingSwap() const
	{
		return KeyCapture == EKeyCapture::AskSwap;
	}
	FString GetKeyPromptText() const;
	FString GetKeyErrorText() const;
	// テストからキーイベントを流す。実際のキー入力と同じ経路を通る。
	FReply SendKeyDownForTest(const FKeyEvent& KeyEvent)
	{
		return NativeOnPreviewKeyDown(FGeometry(), KeyEvent);
	}
	FReply SendKeyUpForTest(const FKeyEvent& KeyEvent)
	{
		return NativeOnKeyUp(FGeometry(), KeyEvent);
	}
	int32 GetPage() const
	{
		return Page;
	}
	// 待ちや確認の間、後ろのページ・タブ・リセット・閉じるは押せない。
	bool IsBackgroundEnabled() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
	enum class EKeyCapture : uint8
	{
		None,
		Waiting,  // 次のキーを待っている
		AskSwap,  // かぶった相手と入れ替えるかを聞いている
		Notice,   // 事情をお知らせしている
	};
	void Build(UTBGameUserSettings& Settings);
	UTextBlock* MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color);
	void RefreshRows();
	void ApplyKeyPrompt(const TB::FKeyPrompt& Prompt, const FKey& Key);
	void CancelKeyCapture(bool bBack = true);
	// お知らせを閉じて、キーを待つ状態に戻る。
	void ReturnToWaiting();
	void SaveKeys() const;
	void SetBackgroundEnabled(bool bEnabled);
	void SetKeyOverlay(EKeyCapture State, const FString& Text, const FString& Error = FString());
	UFUNCTION()
	void SwapKey();
	UFUNCTION()
	void CancelKey();
	void PlayClick(bool bBack = false) const;
	bool IsConfirming() const;
	// 変える前の解像度と画面モードに戻す。
	void RestoreVideoMode(UTBGameUserSettings& Settings) const;
	UFUNCTION()
	void ShowAudio();
	UFUNCTION()
	void ShowDisplay();
	UFUNCTION()
	void ShowControls();
	UFUNCTION()
	void ResetPage();
	UFUNCTION()
	void KeepVideoMode();
	UFUNCTION()
	void RevertVideoMode();
	TFunction<void()> Closed;
	int32 Page = 0;
	double ConfirmUntil = 0;
	EKeyCapture KeyCapture = EKeyCapture::None;
	FName CaptureRow;
	FKey PendingKey;
	// 状態を変えたキー。そのキーを押したままのリピートと、離したときのKeyUpは、ここで止める。
	FKey HeldTriggerKey;
	// 「戻す」の確認中。お知らせを閉じたら待ちには戻らず、そのまま終える。
	bool bResetting = false;
	// ウィンドウの大きさが変わると、エンジンはそれを「確認済み」にしてしまう。戻す先は自分で覚えておく。
	FIntPoint PreviousResolution = FIntPoint::ZeroValue;
	EWindowMode::Type PreviousWindowMode = EWindowMode::Windowed;
	// 行を入れる箱。操作ページはキーの行が多いので、箱をスクロールで包む（PageRootsが表に出す側）。
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVerticalBox>> Pages;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> PageRoots;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTBKeyRow>> KeyRows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> Tabs;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTBOptionRow>> Rows;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ControlsScroll;
	// キーの付け替えの板。待ち、入れ替えの確認、お知らせで文とボタンが変わる。
	UPROPERTY(Transient)
	TObjectPtr<UWidget> KeyOverlay;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyPromptText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyErrorText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> KeySwapButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> KeyCancelButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> KeyCloseButton;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> ConfirmOverlay;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> KeepButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ResetButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;
	UPROPERTY(Transient)
	TObjectPtr<UFont> PaperFont;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PaperTexture;
};
