#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBBoxTV.generated.h"

class ATBBox;
class ATBCharacter;
class ATBPlayerState;
class ATBTVCamera;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/** 0〜2は固定カメラ、3〜9は箱にいないプレイヤー（人間→おもちゃ、各PlayerId順。最大8人−捕まった1人＝7人）。映すものが無い番号は砂嵐。 */
USTRUCT()
struct FTBTVChannel
{
	GENERATED_BODY()
	UPROPERTY()
	int32 Number = 0;
	UPROPERTY()
	TObjectPtr<ATBPlayerState> Target;
	UPROPERTY()
	uint32 Revision = 0;
};

/** 箱内テレビ。サーバーはチャンネル、視聴者のPCは撮影とノイズ表示を担当する。 */
UCLASS(Blueprintable)
class TOONSTORY_API ATBBoxTV : public AActor
{
	GENERATED_BODY()
public:
	ATBBoxTV();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Replicated, Category = "Television")
	TObjectPtr<ATBBox> SourceBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Television")
	TObjectPtr<UStaticMeshComponent> Frame;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Television")
	TObjectPtr<UStaticMeshComponent> Screen;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Television")
	TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(EditDefaultsOnly, Category = "Television")
	TObjectPtr<UMaterialInterface> ScreenMaterial;
	UPROPERTY(EditDefaultsOnly, Category = "Television", meta = (ClampMin = "0"))
	int32 ScreenMaterialIndex = 0;
	UPROPERTY(EditDefaultsOnly, Category = "Television")
	FIntPoint CaptureResolution = FIntPoint(512, 288);
	UPROPERTY(EditDefaultsOnly, Category = "Television", meta = (ClampMin = "1", ClampMax = "30"))
	float CaptureRate = 15.f;
	UPROPERTY(EditDefaultsOnly, Category = "Television", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float NoiseSeconds = .4f;

	static constexpr int32 CameraChannels = 3;
	static constexpr int32 MaxChannel = 9;

	// サーバーだけが変更する。操作できるのは収納済みのおもちゃだけ。
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Television")
	bool SelectChannel(ATBCharacter* Viewer, int32 Number);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Television")
	bool StepChannel(ATBCharacter* Viewer, int32 Direction);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Television")
	bool TogglePower(ATBCharacter* Viewer);
	const FTBTVChannel& GetChannel() const
	{
		return Channel;
	}
	bool IsPowerOn() const
	{
		return bPowerOn;
	}
	bool IsCapturing() const
	{
		return bViewing;
	}

private:
	UPROPERTY(ReplicatedUsing = OnRep_Channel)
	FTBTVChannel Channel;
	UPROPERTY(Replicated)
	bool bPowerOn = true;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenInstance;
	FTimerHandle CaptureTimer;
	bool bViewing = false;
	double NoiseUntil = 0;

	UFUNCTION()
	void OnRep_Channel();
	bool CanOperate(const ATBCharacter* Viewer) const;
	TArray<ATBPlayerState*> GetPlayerChannels() const;
	ATBTVCamera* FindCamera(int32 Number) const;
	bool HasPicture(int32 Number, const TArray<ATBPlayerState*>& Players) const;
	void SetChannel(int32 Number);
	bool HasLocalViewer() const;
	bool IsAvailableTarget(const ATBPlayerState* Player) const;
	void SetViewing(bool bEnable);
	void CaptureFrame();
	bool SetCaptureView();
};
