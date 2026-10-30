// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBChatBox.generated.h"

class UBorder;
class UCanvasPanelSlot;
class UEditableText;
class UFont;
class UScrollBox;
class UVerticalBox;

/** 左隅のチャット。閉じている間は新しい発言だけを出して薄れさせ、開くと履歴と入力欄を出す。 */
UCLASS()
class TOONSTORY_API UTBChatBox : public UUserWidget
{
	GENERATED_BODY()
public:
	bool IsOpen() const
	{
		return bOpen;
	}
	// 開くと入力欄にフォーカスを移す。閉じても書きかけの文は残す。
	void SetOpen(bool bInOpen);
	// 結果画面では真ん中の板を避けて下へ寄せる。
	void SetResultsLayout(bool bInResults);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	UFUNCTION()
	void TextChanged(const FText& Text);
	UFUNCTION()
	void TextCommitted(const FText& Text, ETextCommit::Type Method);
	void Rebuild();
	bool bOpen = false;
	bool bResults = false;
	uint32 BuiltRevision = MAX_uint32;
	UPROPERTY(Transient)
	TObjectPtr<UFont> PaperFont;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Panel;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> PanelSlot;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> LogBack;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> Scroll;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LineBox;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> InputBack;
	UPROPERTY(Transient)
	TObjectPtr<UEditableText> Input;
	// 履歴と同じ並び。
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> LineWidgets;
};
