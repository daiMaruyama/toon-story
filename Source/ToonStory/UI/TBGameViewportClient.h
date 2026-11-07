// Copyright (c) 2026 Mamodou. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "TBGameViewportClient.generated.h"

/** ゲーム画面の警告表示と、撮影用のUI切り替え。エディタの表示には触れない。 */
UCLASS()
class TOONSTORY_API UTBGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
public:
	virtual void Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	virtual void DetachViewportClient() override;
	virtual void BeginDestroy() override;
	virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;
	virtual EMouseCursor::Type GetCursor(FViewport* InViewport, int32 X, int32 Y) override;
	void ToggleUI();
	bool IsUIHidden() const { return bUIHidden; }
	static bool IsUIHidden(const UWorld* World);
private:
	void ReleaseShortcut();
	bool bUIHidden = false;
	TSharedPtr<class IInputProcessor> UIShortcut;
};
