// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "TBTitleMenu.generated.h"

class UTBSession;
class UButton;
class UTextBlock;

/** Temporary UMG menu; observes the existing session subsystem. */
UCLASS()
class TOONSTORY_API UTBTitleMenu : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
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
