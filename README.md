# toon-story

UE5 / C++ で開発中の対戦アクション（PvP）。


## 概要

| - |
| ジャンル | PvP 対戦アクション |
| エンジン | Unreal Engine 5.8 / C++ |
| プラットフォーム | Windows |
| 開発期間 | TODO: 2026.09- |
| 制作人数 | TODO: 現在4名 |
| 担当 | TODO: 例）ゲームプレイ実装、ネットワーク同期、UI |

## 技術的な取り組み

<!-- TODO: 「何を作ったか」ではなく「何を考えて、どう作ったか」を書く。 -->

- TODO: 例）キャラクターのステート管理を GAS ベースで実装し、〜
- TODO: 例）ネットワーク同期のラグ補正に〜を採用し、〜

## 構成

```
Source/     ゲームロジック (C++)
Config/     エンジン・プロジェクト設定
Content/    アセット
```

<!-- TODO: Source/ の主要モジュールが固まったら、ここに1行ずつ説明を足す -->

## ビルド

Unreal Engine 5.8 と Visual Studio 2022 が必要。

1. `ToonStory.uproject` を右クリック →「Generate Visual Studio project files」
2. 生成された `.sln` を開き、`Development Editor` / `Win64` でビルド

