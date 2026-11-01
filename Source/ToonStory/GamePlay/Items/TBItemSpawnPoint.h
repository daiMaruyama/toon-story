#pragma once
#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "TBItemSpawnPoint.generated.h"

class ATBRoomLighting;

/** エディタで置く出現候補。ゲーム中は表示せず、抽選はサーバーだけで行う。 */
UCLASS()
class TOONSTORY_API ATBItemSpawnPoint : public ATargetPoint
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="この候補を使用"))
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="配置種別"))
	FName PlacementType;
	/** 照明スイッチが選ばれた場合の操作対象。未指定の地点にはスイッチを出さない。 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="ランダム配置", meta=(DisplayName="スイッチが操作する部屋"))
	TObjectPtr<ATBRoomLighting> Room;
};
