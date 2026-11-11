// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TBLoadingScreenSettings.generated.h"

class UTexture2D;

/** ロード画面。暗い子ども部屋で、おもちゃが積み木の陰から箱の陰へこっそり走る。プロジェクト設定の「ロード画面」で変える */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ロード画面"))
class TOONSTORY_API UTBLoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UTBLoadingScreenSettings();

	UPROPERTY(Config, EditAnywhere, Category = "全体", meta = (DisplayName = "ロード画面を出す"))
	bool bEnabled = true;
	/** 背景が透明で、右を向いた絵。1周ごとにランダムに替える */
	UPROPERTY(Config, EditAnywhere, Category = "おもちゃ", meta = (DisplayName = "おもちゃの絵"))
	TArray<TSoftObjectPtr<UTexture2D>> Toys;
	/** 画面の高さに対する、おもちゃの絵（正方形）の高さ */
	UPROPERTY(Config, EditAnywhere, Category = "おもちゃ",
	          meta = (DisplayName = "おもちゃの大きさ", ClampMin = "0.05", ClampMax = "0.6"))
	float ToyHeight = .32f;
	/** おもちゃの絵にかける色。暗い部屋になじませる。白なら絵のまま */
	UPROPERTY(Config, EditAnywhere, Category = "おもちゃ", meta = (DisplayName = "おもちゃの色味"))
	FLinearColor ToyTint = FLinearColor(.9f, .88f, .86f);

	UPROPERTY(Config, EditAnywhere, Category = "動き",
	          meta = (DisplayName = "顔を出して様子を見る時間", Units = "s", ClampMin = "0.2"))
	float PeekSeconds = 1.6f;
	UPROPERTY(Config, EditAnywhere, Category = "動き",
	          meta = (DisplayName = "箱まで走る時間", Units = "s", ClampMin = "0.2"))
	float RunSeconds = 2.2f;
	UPROPERTY(Config, EditAnywhere, Category = "動き", meta = (DisplayName = "走る間に跳ねる回数", ClampMin = "1"))
	int32 Hops = 7;
	/** 箱に隠れてから、次のおもちゃが顔を出すまで */
	UPROPERTY(Config, EditAnywhere, Category = "動き",
	          meta = (DisplayName = "隠れたあとの間", Units = "s", ClampMin = "0"))
	float HiddenSeconds = .7f;

	/** ロビーと試合の切り替えは読み込みがほとんどないので、フェードでつなぐためにこの時間だけ見せる */
	UPROPERTY(Config, EditAnywhere, Category = "切り替え",
	          meta = (DisplayName = "試合の切り替えで見せる時間", Units = "s", ClampMin = "0"))
	float MatchSwitchSeconds = 1.5f;
	/** 出しはじめに、黒から明けるまで */
	UPROPERTY(Config, EditAnywhere, Category = "切り替え",
	          meta = (DisplayName = "出るときの暗転", Units = "s", ClampMin = "0"))
	float FadeInSeconds = .2f;
	/** ロードが終わってから、いったん真っ黒に落とし、ゲーム画面が明けて見えきるまで */
	UPROPERTY(Config, EditAnywhere, Category = "切り替え",
	          meta = (DisplayName = "消えるときの暗転", Units = "s", ClampMin = "0"))
	float FadeOutSeconds = .45f;
	/** ロード後、自分のキャラが出てくるのを待つ上限。キャラのいない画面（タイトルなど）はこの時間で消える */
	UPROPERTY(Config, EditAnywhere, Category = "切り替え",
	          meta = (DisplayName = "キャラを待つ上限", Units = "s", ClampMin = "0"))
	float PawnWaitSeconds = 1.f;
	/** みんなの見た目が決まり、走っているおもちゃが箱に隠れきるまでは出しておく。その待ちの上限 */
	UPROPERTY(Config, EditAnywhere, Category = "切り替え",
	          meta = (DisplayName = "見た目とおもちゃを待つ上限", Units = "s", ClampMin = "0"))
	float SettleWaitSeconds = 5.f;

	UPROPERTY(Config, EditAnywhere, Category = "色", meta = (DisplayName = "壁"))
	FLinearColor WallColor = FLinearColor::FromSRGBColor(FColor(38, 34, 48));
	UPROPERTY(Config, EditAnywhere, Category = "色", meta = (DisplayName = "床"))
	FLinearColor FloorColor = FLinearColor::FromSRGBColor(FColor(56, 42, 38));
	UPROPERTY(Config, EditAnywhere, Category = "色", meta = (DisplayName = "積み木"))
	TArray<FLinearColor> BlockColors = {FLinearColor::FromSRGBColor(FColor(120, 52, 46)),
	                                    FLinearColor::FromSRGBColor(FColor(52, 78, 112)),
	                                    FLinearColor::FromSRGBColor(FColor(150, 118, 48))};
	UPROPERTY(Config, EditAnywhere, Category = "色", meta = (DisplayName = "箱"))
	FLinearColor BoxColor = FLinearColor::FromSRGBColor(FColor(112, 82, 54));
};
