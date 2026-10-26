// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBKeyRow.generated.h"

class UButton;
class UFont;
class UTextBlock;
class UTexture2D;

/** オプション画面のキー設定の1行。表示名と、今のキー名を出す。押す操作は次の回で足す。 */
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
	// 「変える」を押したとき。今は何も設定しない。
	TFunction<void()> Pressed;

private:
	UFUNCTION()
	void Clicked();
	FName MappingName;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ChangeButton;
};
