// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GenericPlatform/GenericWindow.h"
#include "TBOptionsMenu.generated.h"

class APlayerController;
class UButton;
class UFont;
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

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
	void Build(UTBGameUserSettings& Settings);
	UTextBlock* MakeLabel(const FString& Text, int32 FontSize, FLinearColor Color);
	void ShowPage(int32 Index);
	void RefreshRows();
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
	TObjectPtr<UWidget> ConfirmOverlay;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> KeepButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;
	UPROPERTY(Transient)
	TObjectPtr<UFont> PaperFont;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PaperTexture;
};
