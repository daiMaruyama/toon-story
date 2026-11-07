// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "TBLoadingScreenSubsystem.generated.h"

class STBLoadingScreen;
class SWidget;
class UGameViewportClient;
class UNetDriver;
class UTexture2D;

/**
 * ロード画面を出し入れする。ほかの処理は変えず、読むだけでつなぐ。
 * - マップを丸ごと読み直す移動（タイトル ↔ ロビー）は、読み込みの開始・終了の通知で MoviePlayer に出させる。
 * - 裏で切り替える移動（ロビー ↔ 試合）は、コントローラーが画面を黒くしている間、その上にかぶせる。
 * どちらも、自分のキャラが出てくるまで出しておき、短く薄れて消える。
 */
UCLASS()
class TOONSTORY_API UTBLoadingScreenSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTBLoadingScreenSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }

private:
	void PreLoadMap(const FString& MapName);
	void PostLoadMap(UWorld* World);
	void Failed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void TravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void StopMovie();
	void ShowOverlay();
	void RemoveOverlay();
	// 裏で切り替える移動の間、かぶせておくか。
	bool ShouldCoverSeamless() const;

	// MoviePlayer に渡す絵と、ゲーム画面にかぶせる絵。同じ部品を2か所に付けないよう分ける。
	TSharedPtr<STBLoadingScreen> MovieScreen;
	// おもちゃの絵。ロードの直前に読むと出だしの1フレームが仮の絵（灰色の市松）になるので、起動時から持っておく。
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> ToyTextures;
	TSharedPtr<STBLoadingScreen> Screen;
	TSharedPtr<SWidget> Overlay;
	TWeakObjectPtr<UGameViewportClient> OverlayViewport;
	FDelegateHandle PreLoadHandle, PostLoadHandle, NetworkFailureHandle, TravelFailureHandle;
	// 一度でもマップを読み終えたか。起動直後の最初の読み込みには出さない。
	bool bHadWorld = false;
	// マップを読み直していて、読み終わったあと自分のキャラを待っているか。
	bool bHardLoad = false;
	double LoadedAt = 0;
	// ロビーと試合の切り替わりを見るための、前のフレームの状態。ワールドが替わったら数え直す。
	TWeakObjectPtr<UWorld> PhaseWorld;
	uint8 LastPhase = 0;
	double SwitchUntil = 0;
	bool bReturning = false;
};
