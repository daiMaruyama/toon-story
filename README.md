# toon-story

UE5 / C++ で開発中の対戦アクション（PvP）。

## 概要

| 項目 | 内容 |
| --- | --- |
| ジャンル | PvP 対戦アクション |
| エンジン | Unreal Engine 5.8 / C++ |
| プラットフォーム | Windows |
| 開発期間 | 2026.09 - |
| 制作人数 | 4 名 |

## 構成

```
Source/     ゲームロジック (C++)
  ToonStory/
    Core/         GameMode / GameState / PlayerController / GameInstance
    Character/    キャラクター本体、移動、入力
    Combat/       攻撃、ダメージ、当たり判定
    UI/           HUD、ウィジェット
    Data/         DataAsset / DataTable
    Variant_*/    UE テンプレート由来
Config/     エンジン・プロジェクト設定
Content/    アセット（別リポジトリを submodule として接続）
docs/       開発ドキュメント
```

## ビルド

Unreal Engine 5.8 と Visual Studio 2022 が必要。

```bash
git clone --recurse-submodules https://github.com/daiMaruyama/toon-story.git
```

1. `ToonStory.uproject` を右クリック →「Generate Visual Studio project files」
2. 生成された `.sln` を開き、`Development Editor` / `Win64` でビルド

セットアップと開発フローの詳細は [docs/GIT_SETUP.md](docs/GIT_SETUP.md) を参照。
