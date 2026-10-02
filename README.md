# toon-story

UE5 / C++ で開発中の対戦アクション（PvP）。

## 概要

| 項目 | 内容 |
| --- | --- |
| ジャンル | PvP 対戦アクション |
| エンジン | Unreal Engine 5.8 / C++ |
| プラットフォーム | Windows |
| 開発環境 | Windows (Visual Studio 2022) / macOS (Xcode) |
| 開発期間 | 2026.09 - |
| 制作人数 | 3 名 |

## 構成

```
Source/     ゲームロジック (C++)
  ToonStory/
    Core/         試合管理、プレイヤー状態、部屋作成・検索・参加
    Character/    キャラクター本体、移動、入力
    GamePlay/     捕獲・収納・救助・収集、共通ルール計算
    UI/           HUD
    Data/         陣営・試合状態の型、設定
    ToonStory.*   モジュール設定
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

## 試遊・検証

- [試遊手順](docs/PLAYTEST.md)
- [受入テスト](docs/ACCEPTANCE_TESTS.md)
