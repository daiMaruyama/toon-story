# toon-story

UE5 / C++ で開発中の対戦アクション（PvP）。

## ToyBoxスターター v2 の統合

`toybox-ue58-starter.zip` の実装を既存の `ToonStory` モジュールへ移植しています。
現在の既定GameModeは `TBGameMode`、両陣営とも一人称、入力は通常入力です。
2026-09-30: 時間切れは人間勝利。箱は固定壁で開閉せず、救助完了で仲間を箱外へ移します。おもちゃ同士は衝突します。
Steam App ID 480を設定済みです。PIE用LAN設定への切替と実機手順は [試遊手順](docs/toybox-starter/PLAYTEST.md) を参照してください。
試合は `TBGame.h/.cpp` と `TBSession.h/.cpp` を使用します。重複する旧ToyBox実装は削除済みです。
三人称テンプレートの `ToonStoryCharacter`・`ToonStoryGameMode`・`ToonStoryPlayerController` は既存Blueprintから参照されるため保持しています。

**試遊用ArenaマップをContent側に追加しています。** `/Game/Maps/Arena` を開いてください。
照明とTBGameModeを設定済みで、床・壁・箱・アイテムはPlay時に自動生成します。
詳しい操作は [試遊手順](docs/toybox-starter/PLAYTEST.md) を参照してください。
`Content` は別リポジトリなので、マップはそちらで管理します。

- [統合内容・検証結果](docs/toybox-starter/INTEGRATION.md)
- [スターターの仕様・操作手順](docs/toybox-starter/README_ja.md)
- [受入テスト](docs/toybox-starter/ACCEPTANCE_TESTS.md)

```powershell
.\Scripts\BuildEditor.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
.\Scripts\PackageWindows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

## 概要

| 項目 | 内容 |
| --- | --- |
| ジャンル | PvP 対戦アクション |
| エンジン | Unreal Engine 5.8 / C++ |
| プラットフォーム | Windows |
| 開発環境 | Windows (Visual Studio 2022) / macOS (Xcode) |
| 開発期間 | 2026.09 - |
| 制作人数 | 4 名 |

## 構成

```
Source/     ゲームロジック (C++)
  ToonStory/
    TBGame.h/.cpp     試合・陣営・移動・捕獲・箱・収集・HUD
    TBSession.h/.cpp  部屋作成・検索・参加・退出
    TBRuleMath.h      共通ルール計算
    ToonStory*        モジュール設定・既存Blueprint用テンプレート
Config/     エンジン・プロジェクト設定
Content/    アセット（別リポジトリを submodule として接続）
Plugins/    エディタ拡張
docs/       開発ドキュメント
```

## ビルド

Unreal Engine 5.8 が必要。エディタは Windows / macOS どちらでも動く。

```bash
git clone --recurse-submodules https://github.com/daiMaruyama/toon-story.git
```

**Windows**（Visual Studio 2022）

1. `ToonStory.uproject` を右クリック →「Generate Visual Studio project files」
2. 生成された `.sln` を開き、`Development Editor` / `Win64` でビルド

**macOS**（Xcode）

`ToonStory.uproject` をダブルクリックし、「modules are missing」のダイアログで **Yes** を押すとその場でビルドされる。

セットアップと開発フローの詳細は [docs/GIT_SETUP.md](docs/GIT_SETUP.md) を参照。
