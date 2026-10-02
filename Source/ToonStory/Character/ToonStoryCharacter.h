// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ToonStoryCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/** 旋回カメラ付きの三人称キャラクター。 */
UCLASS(abstract)
class AToonStoryCharacter : public ACharacter
{
	GENERATED_BODY()


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

protected:


	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;


	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;


	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;


	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:


	AToonStoryCharacter();

protected:


	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:


	void Move(const FInputActionValue& Value);


	void Look(const FInputActionValue& Value);

public:

	/** 操作入力・UI共通の移動処理。 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** 操作入力・UI共通の視点処理。 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** 操作入力・UI共通のジャンプ処理。 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** 操作入力・UI共通のジャンプ処理。 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:


	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }


	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

