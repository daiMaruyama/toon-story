// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/KismetMathLibrary.h" // EEasingFunc用
#include "TBCharacterData.generated.h"

USTRUCT(BlueprintType)
struct FTBHumanMovement
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人間",
	          meta = (DisplayName = "歩く速さ", Units = "cm/s", ClampMin = "0"))
	float WalkSpeed = 270.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人間",
	          meta = (DisplayName = "人形を担いでいるときの速さ", Units = "cm/s", ClampMin = "0"))
	float CarrySpeed = 180.f;
	/** 歩く速さに対する割合 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人間",
	          meta = (DisplayName = "しゃがんでいるときの速さ", ClampMin = "0", ClampMax = "1"))
	float CrouchSpeedRatio = .48f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人間",
	          meta = (DisplayName = "ジャンプの強さ", Units = "cm/s", ClampMin = "0"))
	float JumpVelocity = 420.f;
	/** 歩いたまま乗り越えられる段差の高さ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人間",
	          meta = (DisplayName = "乗り越えられる段差", Units = "cm", ClampMin = "0"))
	float MaxStepHeight = 45.f;
};

USTRUCT(BlueprintType)
struct FTBToyMovement
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人形",
	          meta = (DisplayName = "歩く速さ", Units = "cm/s", ClampMin = "0"))
	float WalkSpeed = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人形",
	          meta = (DisplayName = "走る速さ", Units = "cm/s", ClampMin = "0"))
	float SprintSpeed = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人形",
	          meta = (DisplayName = "ジャンプの強さ", Units = "cm/s", ClampMin = "0"))
	float JumpVelocity = 400.f;
	/** 歩いたまま乗り越えられる段差の高さ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人形",
	          meta = (DisplayName = "乗り越えられる段差", Units = "cm", ClampMin = "0"))
	float MaxStepHeight = 8.f;
};

USTRUCT(BlueprintType)
struct FTBViewFeel
{
	GENERATED_BODY()
	/** 視点の向きに体が追いつく速さ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "共通",
	          meta = (DisplayName = "振り向く速さ", Units = "deg/s", ClampMin = "0"))
	float TurnRate = 540.f;
	/** 段差を上り下りしたとき、目線の高さがこの速さで追いつく。大きいほど速い */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "共通",
	          meta = (DisplayName = "段差で目線が追いつく速さ", ClampMin = "1"))
	float StepSmoothSpeed = 12.f;
	/** 段差で目線を遅らせる最大の高さ。これを超える分は遅らせずに追従する */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "共通",
	          meta = (DisplayName = "段差で目線を遅らせる最大", Units = "cm", ClampMin = "0"))
	float StepSmoothMax = 60.f;
};

USTRUCT(BlueprintType)
struct FTBCarryCameraFeel
{
	GENERATED_BODY()
	/** 担がれている間の見下ろし角。左右には回り込めるが、上下は固定 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "担がれカメラ",
	          meta = (DisplayName = "見下ろす角度", Units = "Degrees", ClampMin = "-89", ClampMax = "0"))
	float ViewPitch = -40.f;
	/** 担ぐ人間からカメラまでの距離 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "担がれカメラ",
	          meta = (DisplayName = "カメラの距離", Units = "cm", ClampMin = "0"))
	float ArmLength = 450.f;
	/** 担ぐ人間の足元からの、注視点の高さ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "担がれカメラ",
	          meta = (DisplayName = "注視点の高さ", Units = "cm"))
	float LookHeight = 80.f;
	/** 一人称から担がれカメラへつなぐ時間 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "担がれカメラ",
	          meta = (DisplayName = "担がれたときの切り替え時間", Units = "s", ClampMin = "0"))
	float BlendTime = .45f;
	/** 下ろされるときは担ぐ人の頭を横切るので短くする */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "担がれカメラ",
	          meta = (DisplayName = "下ろされたときの切り替え時間", Units = "s", ClampMin = "0"))
	float ReleaseBlendTime = .12f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "担がれカメラ", meta = (DisplayName = "切り替えの動き方"))
	TEnumAsByte<EEasingFunc::Type> BlendEase = EEasingFunc::EaseInOut;
	/** 動き方の効きの強さ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "担がれカメラ",
	          meta = (DisplayName = "切り替えの効きの強さ", ClampMin = "1"))
	float BlendExp = 2.f;
};

/** 人間・人形の動きの手触り。人間と人形は同じキャラのクラスで、試合ごとに役が切り替わる */
UCLASS(BlueprintType)
class TOONSTORY_API UTBCharacterData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人間", meta = (ShowOnlyInnerProperties))
	FTBHumanMovement Human;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人形", meta = (ShowOnlyInnerProperties))
	FTBToyMovement Toy;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "共通", meta = (ShowOnlyInnerProperties))
	FTBViewFeel View;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "担がれカメラ", meta = (ShowOnlyInnerProperties))
	FTBCarryCameraFeel CarryCamera;
};

namespace TB
{
	// UTBDataSettings で選んだキャラのデータ。未設定なら C++ の初期値。
	TOONSTORY_API const UTBCharacterData& CharacterData();
}
