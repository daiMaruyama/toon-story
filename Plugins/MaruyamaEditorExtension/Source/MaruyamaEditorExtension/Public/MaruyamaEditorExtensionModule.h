#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class UToolMenu;

/**
 * メインメニューバーに「丸山拡張」メニューを追加するエディタモジュール。
 *
 * 中身はEpic公式のModelContextProtocolプラグインの薄いラッパーで、
 * コンソールコマンド（ModelContextProtocol.StartServerなど）で出来ることを
 * メニューから押せるようにしているだけ。独自の状態は一切持たない。
 */
class FMaruyamaEditorExtensionModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	/** メニューバーに「丸山拡張」のサブメニューを差し込む。UToolMenusの起動後に呼ばれる。 */
	void RegisterMenus();

	/** サブメニューを開いたときに中身を組み立てる。 */
	static void FillMaruyamaMenu(UToolMenu* Menu);
};
