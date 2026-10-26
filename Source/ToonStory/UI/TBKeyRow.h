// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBKeyRow.generated.h"

class UButton;
class UFont;
class UTextBlock;
class UTexture2D;

/** オプション画面のキー設定の1行。表示名と、今のキー名を出し、「変える」でPressedを呼ぶ。 */
UCLASS()
class TOONSTORY_API UTBKeyRow : public UUserWidget
{
	GENERATED_BODY()
public:
	void Build(const FString& Label, FName InMappingName, UFont* Font, UTexture2D* Paper);
	// 割り当ては変わるので、毎回キー名を引き直す。
	void Refresh();
	FName GetMappingName() const
	{
		return MappingName;
	}
	FString GetKeyText() const;
	// 取り消し・確定のあと、押した行の「変える」にフォーカスを戻す。
	void FocusButton();
	// 「変える」を押したとき。
	TFunction<void()> Pressed;
	// 「戻す」を押したとき。
	TFunction<void()> Reset;

private:
	UFUNCTION()
	void Clicked();
	UFUNCTION()
	void ResetClicked();
	FName MappingName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ChangeButton;
};
