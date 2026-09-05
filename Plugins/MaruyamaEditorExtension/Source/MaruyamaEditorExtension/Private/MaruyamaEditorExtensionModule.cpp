#include "MaruyamaEditorExtensionModule.h"

#include "Framework/Notifications/NotificationManager.h"
#include "IModelContextProtocolModule.h"
#include "ISettingsModule.h"
#include "ModelContextProtocolClientConfig.h"
#include "ModelContextProtocolServer.h"
#include "ModelContextProtocolSettings.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "MaruyamaEditorExtension"

namespace MaruyamaEditorExtension
{
	/** MCPプラグインが無効時、nullptrを許容。 */
	IModelContextProtocolModule* GetMcpModule()
	{
		return IModelContextProtocolModule::Get();
	}

	bool IsMcpAvailable()
	{
		return GetMcpModule() != nullptr;
	}

	bool IsServerRunning()
	{
		IModelContextProtocolModule* Module = GetMcpModule();
		const FModelContextProtocolServer* Server = Module ? Module->GetServer() : nullptr;
		return Server != nullptr && Server->IsServerRunning();
	}

	void ShowNotification(const FText& Message, bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 5.0f;

		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	void ToggleServer()
	{
		IModelContextProtocolModule* Module = GetMcpModule();
		if (!Module)
		{
			return;
		}

		if (IsServerRunning())
		{
			Module->StopServer();
			ShowNotification(LOCTEXT("ServerStopped", "MCP サーバーを停止しました。"), true);
			return;
		}

		const uint32 Port = UE::ModelContextProtocol::GetServerPortNumber();
		Module->StartServer(Port, UE::ModelContextProtocol::GetServerUrlPath());

		// StartServer は戻り値を返さないので、実際に待ち受けに成功したかを確認する。
		// ポートが埋まっていると起動に失敗し、Output Log にだけエラーが出る。
		const bool bRunning = IsServerRunning();
		ShowNotification(
			bRunning
				? FText::Format(LOCTEXT("ServerStarted", "MCP サーバーを開始しました（ポート {0}）。"), FText::AsNumber(Port))
				: FText::Format(LOCTEXT("ServerStartFailed", "MCP サーバーを開始できませんでした。ポート {0} が使用中かもしれません。Output Log を確認してください。"), FText::AsNumber(Port)),
			bRunning);
	}

	bool IsAutoStartEnabled()
	{
		return GetDefault<UModelContextProtocolSettings>()->bAutoStartServer;
	}

	void ToggleAutoStart()
	{
		UModelContextProtocolSettings* Settings = GetMutableDefault<UModelContextProtocolSettings>();
		Settings->bAutoStartServer = !Settings->bAutoStartServer;

		// EditorPerProjectUserSettings.ini（Saved/ 配下・Git 管理外）に書かれる個人設定。
		Settings->SaveConfig();
	}

	void GenerateClientConfig(EModelContextProtocolClient Client, FText ClientLabel)
	{
		const bool bWritten = UE::ModelContextProtocol::WriteClientConfiguration(
			Client,
			UE::ModelContextProtocol::GetServerPortNumber(),
			UE::ModelContextProtocol::GetServerUrlPath());

		ShowNotification(
			bWritten
				? FText::Format(LOCTEXT("ConfigWritten", "{0} の接続設定を書き出しました。反映には {0} の再起動が必要です。"), ClientLabel)
				: FText::Format(LOCTEXT("ConfigFailed", "{0} の接続設定を書き出せませんでした。Output Log を確認してください。"), ClientLabel),
			bWritten);
	}

	void RefreshTools()
	{
		if (IModelContextProtocolModule* Module = GetMcpModule())
		{
			Module->RefreshTools();
			ShowNotification(LOCTEXT("ToolsRefreshed", "ツールを再登録しました。"), true);
		}
	}

	void OpenSettings()
	{
		const UModelContextProtocolSettings* Settings = GetDefault<UModelContextProtocolSettings>();
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->ShowViewer(
				Settings->GetContainerName(),
				Settings->GetCategoryName(),
				Settings->GetSectionName());
		}
	}
}

void FMaruyamaEditorExtensionModule::StartupModule()
{
	// UToolMenus はモジュール起動時にはまだ準備できていないので、コールバックで待つ。
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMaruyamaEditorExtensionModule::RegisterMenus));
}

void FMaruyamaEditorExtensionModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FMaruyamaEditorExtensionModule::RegisterMenus()
{
	// このモジュールが追加したメニュー項目を ShutdownModule でまとめて外せるようにする。
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* MenuBar = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
	if (!MenuBar)
	{
		return;
	}

	FToolMenuSection& Section = MenuBar->FindOrAddSection(NAME_None);

	FToolMenuEntry& Entry = Section.AddSubMenu(
		"MaruyamaExtension",
		LOCTEXT("MaruyamaMenu", "丸山拡張"),
		LOCTEXT("MaruyamaMenuTooltip", "MCP サーバーの操作と AI クライアント設定の生成"),
		FNewToolMenuDelegate::CreateStatic(&FMaruyamaEditorExtensionModule::FillMaruyamaMenu));

	Entry.InsertPosition = FToolMenuInsert("Help", EToolMenuInsertType::After);
}

void FMaruyamaEditorExtensionModule::FillMaruyamaMenu(UToolMenu* Menu)
{
	using namespace MaruyamaEditorExtension;

	{
		FToolMenuSection& Section = Menu->AddSection("McpServer", LOCTEXT("ServerSection", "MCP サーバー"));

		Section.AddMenuEntry(
			"ToggleMcpServer",
			LOCTEXT("ToggleServer", "MCP サーバーを有効にする"),
			LOCTEXT("ToggleServerTooltip", "Unreal の MCP サーバーを開始 / 停止します。Claude Code や Codex はこのサーバー経由でエディタを操作します。"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&ToggleServer),
				FCanExecuteAction::CreateStatic(&IsMcpAvailable),
				FIsActionChecked::CreateStatic(&IsServerRunning)),
			EUserInterfaceActionType::ToggleButton);

		Section.AddMenuEntry(
			"ToggleAutoStart",
			LOCTEXT("ToggleAutoStart", "エディタ起動時に自動で開始する"),
			LOCTEXT("ToggleAutoStartTooltip", "EditorPerProjectUserSettings.ini に保存される個人設定です。チームには共有されません。"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&ToggleAutoStart),
				FCanExecuteAction::CreateStatic(&IsMcpAvailable),
				FIsActionChecked::CreateStatic(&IsAutoStartEnabled)),
			EUserInterfaceActionType::ToggleButton);
	}

	{
		FToolMenuSection& Section = Menu->AddSection("McpClients", LOCTEXT("ClientSection", "クライアント設定"));

		Section.AddMenuEntry(
			"GenerateClaudeCodeConfig",
			LOCTEXT("GenerateClaude", "Claude Code の設定を生成"),
			LOCTEXT("GenerateClaudeTooltip", "プロジェクト直下に .mcp.json を書き出します。サーバーは Claude Code と Codex で共通なので、一度書き出せば以降は不要です。"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(
				&GenerateClientConfig,
				EModelContextProtocolClient::ClaudeCode,
				LOCTEXT("ClaudeCodeLabel", "Claude Code"))));

		Section.AddMenuEntry(
			"GenerateCodexConfig",
			LOCTEXT("GenerateCodex", "Codex の設定を生成"),
			LOCTEXT("GenerateCodexTooltip", "プロジェクト直下に .codex/config.toml を書き出します。既にファイルがある場合は上書きされません。"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(
				&GenerateClientConfig,
				EModelContextProtocolClient::Codex,
				LOCTEXT("CodexLabel", "Codex"))));
	}

	{
		FToolMenuSection& Section = Menu->AddSection("McpMaintenance", LOCTEXT("MaintenanceSection", "メンテナンス"));

		Section.AddMenuEntry(
			"RefreshTools",
			LOCTEXT("RefreshTools", "ツールを再登録"),
			LOCTEXT("RefreshToolsTooltip", "ツールセットのプラグインを有効にした直後など、ツール一覧が古いときに実行します。"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&RefreshTools),
				FCanExecuteAction::CreateStatic(&IsMcpAvailable)));

		Section.AddMenuEntry(
			"OpenSettings",
			LOCTEXT("OpenSettings", "MCP 設定を開く..."),
			LOCTEXT("OpenSettingsTooltip", "ポート番号や URL パス、ツール検索モードを変更します。"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&OpenSettings)));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMaruyamaEditorExtensionModule, MaruyamaEditorExtension)
