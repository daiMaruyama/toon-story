// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/Attenuation.h"
#include "Engine/DataAsset.h"
#include "TBRuleData.generated.h"

USTRUCT(BlueprintType)
struct FTBVisionRules
{
	GENERATED_BODY()
	/** 横の視野角。カメラと視界判定の両方が使う */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "視野角", Units = "Degrees", ClampMin = "60", ClampMax = "120"))
	float FieldOfView = 90.f;
	/** 判定の縦の広さはこの比率から決まる。実際の画面の比率が違っても、判定はこの比率で行う */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "画面比率（幅）", ClampMin = "1", ClampMax = "32"))
	int32 AspectWidth = 16;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "画面比率（高さ）", ClampMin = "1", ClampMax = "32"))
	int32 AspectHeight = 9;
	/** 判定を画面の上下左右の端より、それぞれ内側にする角度。端にわずかに映った人形は止めない */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "判定の余白（片側）", Units = "Degrees", ClampMin = "0", ClampMax = "15"))
	float EdgeMargin = 5.f;
	/** 人形の頭と足の判定点を、体の中心から体の高さの半分のこの割合だけ離す。頭・中心・足のどれかが見えたら止める */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "頭と足の判定点", ClampMin = "0", ClampMax = "1"))
	float BodySampleRatio = .85f;
	/** 消灯中、この距離以上の人間から見られても凍結しない。0なら距離による制限なし */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界",
	          meta = (DisplayName = "暗い部屋で視線を防ぐ距離", Units = "m", ClampMin = "0"))
	float DarkProtectionDistanceMeters = 3.f;
};

USTRUCT(BlueprintType)
struct FTBAimRules
{
	GENERATED_BODY()
	/** Eキーの対象にできる、狙った方向からの角度。アイテムと人形で共通 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "狙う・つかむ",
	          meta = (DisplayName = "狙える角度（片側）", Units = "Degrees", ClampMin = "1", ClampMax = "90"))
	float AimAngle = 45.f;
	/** 人間が人形・デコイをつかめる距離 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "狙う・つかむ",
	          meta = (DisplayName = "つかめる距離", Units = "cm", ClampMin = "0"))
	float CaptureDistance = 180.f;
	/** つかんでから担ぎ上げ終わるまでの時間。この間はしまえない */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "狙う・つかむ",
	          meta = (DisplayName = "担ぎ上げるまでの時間", Units = "s", ClampMin = "0"))
	float GrabSeconds = .25f;
	/** 担いだ人形を下ろすとき、前に置く距離 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "狙う・つかむ",
	          meta = (DisplayName = "下ろす距離", Units = "cm", ClampMin = "0"))
	float DropDistance = 100.f;
};

USTRUCT(BlueprintType)
struct FTBBoxRules
{
	GENERATED_BODY()
	/** 1人で救助したときにかかる秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱",
	          meta = (DisplayName = "救助の秒数（1人のとき）", Units = "s", ClampMin = "0.1"))
	float RescueSeconds = 5.f;
	/** 2人目からの救助者1人につき、1人のときの救助の速さに足す割合 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱", meta = (DisplayName = "救助者ボーナス", ClampMin = "0"))
	float AdditionalRescuerBonus = .5f;
	/** 救助できる、箱の操作位置からの距離 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱",
	          meta = (DisplayName = "救助できる距離", Units = "cm", ClampMin = "0"))
	float RescueDistance = 180.f;
	/** 人形をしまうのに、Eを押し続ける秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱",
	          meta = (DisplayName = "しまう秒数", Units = "s", ClampMin = "0"))
	float StoreSeconds = 3.f;
	/** 人形をしまえる、箱の手前の範囲。X は手前への奥行き、Y は左右の片側、Z は高さ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱", meta = (DisplayName = "しまえる範囲"))
	FVector StoreReach = FVector(180.f, 140.f, 220.f);
	/** しまった人形を落下エリアに落とすとき、しまった人数ごとに左右へずらす幅 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱",
	          meta = (DisplayName = "落とす間隔", Units = "cm", ClampMin = "0"))
	float DropSpacing = 30.f;
	/** 救助で箱の外に出すとき、1列目の真ん中に置く位置（箱から見た位置。X は手前がマイナス） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱", meta = (DisplayName = "出す位置"))
	FVector ReleaseOrigin = FVector(-65.f, 0.f, 60.f);
	/** 救助で出す人どうしの間隔。1列3人で、X は列どうし（手前へ）、Y は左右 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱", meta = (DisplayName = "出す間隔"))
	FVector2D ReleaseSpacing = FVector2D(35.f, 50.f);
};

USTRUCT(BlueprintType)
struct FTBPickupRules
{
	GENERATED_BODY()
	/** 人形が電池に触れ続けて回収するまでの秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "電池",
	          meta = (DisplayName = "回収秒数", Units = "s", ClampMin = "0.1"))
	float CollectSeconds = 3.f;
};

USTRUCT(BlueprintType)
struct FTBMatchRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "試合",
	          meta = (DisplayName = "試合時間の初期値", Units = "s", ClampMin = "10"))
	float DefaultDuration = 600.f;
	/** ロビーで選べる試合時間の範囲と刻み */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "試合",
	          meta = (DisplayName = "試合時間の最短", Units = "s", ClampMin = "10"))
	float MinDuration = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "試合",
	          meta = (DisplayName = "試合時間の最長", Units = "s", ClampMin = "10"))
	float MaxDuration = 3600.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "試合",
	          meta = (DisplayName = "試合時間の刻み", Units = "s", ClampMin = "1"))
	float DurationStep = 60.f;
	/** ステージの電池の数より多ければ、ステージ側の数に合わせる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "試合",
	          meta = (DisplayName = "必要な電池数の初期値", ClampMin = "1"))
	int32 DefaultRequiredItems = 5;
};

USTRUCT(BlueprintType)
struct FTBVoiceRules
{
	GENERATED_BODY()
	/** オフならロビーでは全員の声が距離に関係なく聞こえる。オンならロビーも人間→人間の距離で近接にする */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声", meta = (DisplayName = "ロビーも近接ボイス"))
	bool bProximityInLobby = false;
	/** 聞こえる距離。「話す人→聞く人」ごとに、この距離で無音になる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声",
	          meta = (DisplayName = "人間→人間", Units = "cm", ClampMin = "0"))
	float HumanToHumanDistance = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声",
	          meta = (DisplayName = "人間→人形", Units = "cm", ClampMin = "0"))
	float HumanToToyDistance = 1500.f;
	/** 人形の声は、人間にはかなり近くでしか聞こえない */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声",
	          meta = (DisplayName = "人形→人間", Units = "cm", ClampMin = "0"))
	float ToyToHumanDistance = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声",
	          meta = (DisplayName = "人形→人形", Units = "cm", ClampMin = "0"))
	float ToyToToyDistance = 1500.f;
	/** 箱の中と外の間（両方向） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声",
	          meta = (DisplayName = "箱の中と外", Units = "cm", ClampMin = "0"))
	float BoxedDistance = 300.f;
	/** 距離に対する音量の落ち方。Custom のときは下のカーブ（横軸0〜1が距離の割合、縦軸が音量）を使う */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声", meta = (DisplayName = "音量の落ち方"))
	EAttenuationDistanceModel FalloffModel = EAttenuationDistanceModel::Linear;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声", meta = (DisplayName = "音量の落ち方（カーブ）"))
	FRuntimeFloatCurve FalloffCurve;
	/** 箱越しの声のこもり具合。低いほどこもって聞こえる */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声",
	          meta = (DisplayName = "箱越しのこもり", Units = "Hz", ClampMin = "0"))
	float BoxedMuffleFrequency = 800.f;
	/** 半音単位。人形から聞く人間の声は低く、人間から聞く人形の声は高くして、大きさの差を表す */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声",
	          meta = (DisplayName = "人形に聞こえる人間の声の高さ", ClampMin = "-12", ClampMax = "12"))
	float HumanPitchForToys = -4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声",
	          meta = (DisplayName = "人間に聞こえる人形の声の高さ", ClampMin = "-12", ClampMax = "12"))
	float ToyPitchForHumans = 4.f;
	/** 声の揺れが気になるときに調整する */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声",
	          meta = (DisplayName = "声の高さを変える処理の遅延", Units = "ms", ClampMin = "10", ClampMax = "100"))
	float PitchDelayLength = 30.f;
};

USTRUCT(BlueprintType)
struct FTBToyRules
{
	GENERATED_BODY()
	/** 下の一覧にない見た目の、選ばれやすさ */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人形の見た目",
	          meta = (DisplayName = "ふつうの重み", ClampMin = "0", ClampMax = "1000"))
	int32 DefaultToyWeight = 4;
	/** 見た目ごとの選ばれやすさ。重み 1 なら、ふつう（4）の 1/4 の確率で出る */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人形の見た目", meta = (DisplayName = "見た目ごとの重み"))
	TMap<TSoftClassPtr<AActor>, int32> ToyWeights = {
	    {TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Characters/Toys/BP_SpaceRanger_Realistic.BP_SpaceRanger_Realistic_C"))),
	     1}};
};

class UTBRandomEvent;

USTRUCT(BlueprintType)
struct FTBEventRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "イベント", meta = (DisplayName = "イベントを有効化"))
	bool bEnabled = true;
	/** 全イベント共通の毎秒抽選。0.01 = 1%。候補数で発生率は増えない。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "イベント", meta = (DisplayName = "毎秒の発生確率", ClampMin = "0", ClampMax = "1"))
	float ChancePerSecond = .01f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "イベント", meta = (DisplayName = "発生後の共通クールダウン", Units = "s", ClampMin = "0"))
	float CooldownSeconds = 30.f;
	/** 実行可能な候補から等確率で1つ選ぶ。重複登録は無視。試合開始時に読み込む。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "イベント", meta = (DisplayName = "イベント候補"))
	TArray<TSubclassOf<UTBRandomEvent>> EventClasses;
};

/** 作る側が決めるルールの値。試合中は読むだけ */
UCLASS(BlueprintType)
class TOONSTORY_API UTBRuleData : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "イベント", meta = (ShowOnlyInnerProperties))
	FTBEventRules Events;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "視界", meta = (ShowOnlyInnerProperties))
	FTBVisionRules Vision;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "狙う・つかむ", meta = (ShowOnlyInnerProperties))
	FTBAimRules Aim;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "箱", meta = (ShowOnlyInnerProperties))
	FTBBoxRules Box;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "電池", meta = (ShowOnlyInnerProperties))
	FTBPickupRules Pickup;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "試合", meta = (ShowOnlyInnerProperties))
	FTBMatchRules Match;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "声", meta = (ShowOnlyInnerProperties))
	FTBVoiceRules Voice;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "人形の見た目", meta = (ShowOnlyInnerProperties))
	FTBToyRules Toys;
};

namespace TB
{
	// UTBDataSettings で選んだルール。未設定なら C++ の初期値。
	TOONSTORY_API const UTBRuleData& Rules();
}
