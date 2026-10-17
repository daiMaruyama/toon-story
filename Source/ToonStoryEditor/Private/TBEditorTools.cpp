#include "Modules/ModuleManager.h"
#include "TBEditorAssets.h"
#include "Editor.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "TBEditorTools"

namespace
{
	const FName OwnerName(TEXT("ToonStoryEditorTools"));
	FDelegateHandle StartupHandle;

	void OpenAsset(const TCHAR* Path)
	{
		if (GEditor)
		{
			if (auto* Asset = LoadObject<UObject>(nullptr, Path))
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
			}
		}
	}

	void BuildSelectedGrips()
	{
		TArray<USkeletalMesh*> Meshes;
		TArray<FAssetData> Selected;
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().GetSelectedAssets(Selected);
		for (const FAssetData& Asset : Selected)
		{
			if (auto* Mesh = Cast<USkeletalMesh>(Asset.GetAsset()))
			{
				Meshes.AddUnique(Mesh);
			}
		}
		if (Meshes.IsEmpty())
		{
			FMessageDialog::Open(EAppMsgType::Ok,
			                     LOCTEXT("SelectMesh", "Content BrowserでおもちゃのSkeletal Meshを選択してください。"));
			return;
		}
		const FScopedTransaction Transaction(LOCTEXT("GripTransaction", "おもちゃの持ち位置を作成"));
		int32 Count = 0;
		for (auto* Mesh : Meshes)
		{
			if (TBEditorAssets::BuildGripSocket(Mesh))
			{
				++Count;
			}
		}
		FMessageDialog::Open(
		    EAppMsgType::Ok,
		    FText::Format(LOCTEXT("GripResult",
		                          "{0}/{1}個のメッシュの持ち位置を更新しました。TB_CarryCenterと左右のTB_CarryLeft / "
		                          "TB_CarryRightをSkeletal Mesh "
		                          "Editorで位置を微調整して保存できます。Ctrl+"
		                          "Zで元に戻せます。\n対象にはspineまたはpelvisボーンが必要です。"),
		                  FText::AsNumber(Count), FText::AsNumber(Meshes.Num())));
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(OwnerName);
		auto& Section = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools")->FindOrAddSection("ToonStory");
		Section.AddSubMenu(
		    "ToonStory", LOCTEXT("Menu", "ToonStory"), LOCTEXT("MenuTip", "ゲームの編集"),
		    FNewToolMenuDelegate::CreateLambda(
		        [](UToolMenu* Menu)
		        {
			        auto& Stages = Menu->AddSection("Stages", LOCTEXT("Stages", "ステージ編集"));
			        Stages.AddMenuEntry(
			            "ExpandedDemo", LOCTEXT("ExpandedDemo", "増築版Demoを開く"),
			            LOCTEXT("ExpandedDemoTip", "こども部屋と隠れ場所をレベルエディタで編集します。"), FSlateIcon(),
			            FUIAction(
			                FExecuteAction::CreateLambda(
			                    [] { OpenAsset(TEXT("/Game/Maps/TB_ToyWorldDemoExpanded.TB_ToyWorldDemoExpanded")); }),
			                FCanExecuteAction::CreateLambda([] { return GEditor && !GEditor->PlayWorld; })));
			        auto& Tools = Menu->AddSection("Editing", LOCTEXT("Editing", "キャラクター編集"));
			        Tools.AddMenuEntry(
			            "CharacterSettings", LOCTEXT("Character", "キャラクター設定を開く"),
			            LOCTEXT("CharacterTip", "Class Defaultsで速度とおもちゃの種類を編集します。"), FSlateIcon(),
			            FUIAction(FExecuteAction::CreateLambda(
			                [] { OpenAsset(TEXT("/Game/Characters/Toys/BP_ToyCharacter.BP_ToyCharacter")); })));
			        Tools.AddMenuEntry(
			            "HumanPose", LOCTEXT("Pose", "人間の補正アニメーションを開く"), FText::GetEmpty(), FSlateIcon(),
			            FUIAction(FExecuteAction::CreateLambda(
			                []
			                { OpenAsset(TEXT("/Game/Characters/ChildAnimation/ABP_TBHumanPose.ABP_TBHumanPose")); })));
			        Tools.AddMenuEntry(
			            "BuildGrip", LOCTEXT("Grip", "選択したおもちゃの持ち位置を作成"),
			            LOCTEXT("GripTip", "選択したSkeletal "
			                               "Meshに胴体中央と両手の接触点を作成・更新します。既存の手動調整は置き換わり"
			                               "ます。保存は手動です。"),
			            FSlateIcon(),
			            FUIAction(FExecuteAction::CreateStatic(&BuildSelectedGrips),
			                      FCanExecuteAction::CreateLambda([] { return GEditor && !GEditor->PlayWorld; })));
		        }));
	}
} // namespace

class FToonStoryEditorModule : public IModuleInterface
{
public:
	void StartupModule() override
	{
		StartupHandle =
		    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMenus));
	}
	void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(StartupHandle);
		UToolMenus::UnregisterOwner(OwnerName);
	}
};
IMPLEMENT_MODULE(FToonStoryEditorModule, ToonStoryEditor)
#undef LOCTEXT_NAMESPACE
