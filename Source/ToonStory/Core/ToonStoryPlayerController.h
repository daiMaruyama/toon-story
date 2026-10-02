// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ToonStoryPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/** 三人称操作の入力とタッチUIを管理する。 */
UCLASS(abstract)
class AToonStoryPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:


	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;


	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;


	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;


	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** モバイル以外でもタッチUIを表示する。 */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;


	virtual void BeginPlay() override;


	virtual void SetupInputComponent() override;


	bool ShouldUseTouchControls() const;

};
