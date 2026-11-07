// Copyright (c) 2026 Mamodou. All Rights Reserved.

#include "UI/TBGameViewportClient.h"
#include "Core/TBController.h"
#include "Engine/World.h"
#include "EngineGlobals.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SGameLayerManager.h"
#include "Widgets/SViewport.h"

namespace
{
	class FTBUIShortcut : public IInputProcessor
	{
	public:
		explicit FTBUIShortcut(UTBGameViewportClient* InViewport) : Viewport(InViewport) {}
		virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			auto* Client = Viewport.Get();
			const auto Widget = Client ? Client->GetGameViewportWidget() : nullptr;
			if (Event.GetKey() != EKeys::F9 || !Widget || (!Widget->HasKeyboardFocus() && !Widget->HasFocusedDescendants())) return false;
			if (!Event.IsRepeat()) Client->ToggleUI();
			return true;
		}
	private:
		TWeakObjectPtr<UTBGameViewportClient> Viewport;
	};
}

void UTBGameViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
	if (FSlateApplication::IsInitialized())
	{
		UIShortcut = MakeShared<FTBUIShortcut>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(UIShortcut, 0);
	}
}

void UTBGameViewportClient::DetachViewportClient()
{
	ReleaseShortcut();
	Super::DetachViewportClient();
}

void UTBGameViewportClient::BeginDestroy()
{
	ReleaseShortcut();
	Super::BeginDestroy();
}

void UTBGameViewportClient::ReleaseShortcut()
{
	if (UIShortcut && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(UIShortcut);
	UIShortcut.Reset();
}

bool UTBGameViewportClient::IsUIHidden(const UWorld* World)
{
	const auto* Client = World ? Cast<UTBGameViewportClient>(World->GetGameViewport()) : nullptr;
	return Client && Client->IsUIHidden();
}

void UTBGameViewportClient::ToggleUI()
{
	bUIHidden = !bUIHidden;
	if (bUIHidden && GetWorld())
	{
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			if (auto* PC = Cast<ATBController>(It->Get()); PC && PC->IsLocalController()) PC->CloseChat();
	}
	if (const auto Layers = GetGameLayerManager())
	{
		// Hidden/Collapsedはタイトル映像を進めるWidgetのTickも止めるため使わない。
		Layers->AsWidget()->SetRenderOpacity(bUIHidden ? 0.f : 1.f);
		Layers->AsWidget()->SetVisibility(bUIHidden ? EVisibility::HitTestInvisible : EVisibility::SelfHitTestInvisible);
	}
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

void UTBGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
	// ゲームスレッドの画面メッセージ。描画スレッドのVRAM警告はDefaultEngine.iniで別途抑える。
	TGuardValue<bool> HideScreenMessages(GAreScreenMessagesEnabled, false);
	Super::Draw(InViewport, SceneCanvas);
}

EMouseCursor::Type UTBGameViewportClient::GetCursor(FViewport* InViewport, int32 X, int32 Y)
{
	return bUIHidden ? EMouseCursor::None : Super::GetCursor(InViewport, X, Y);
}
