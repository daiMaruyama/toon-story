# toon-story

UE5 / C++ で開発中の対戦アクション（PvP）。

<!-- TODO: スクリーンショットか30秒のGIFを1枚置く。ここが一番見られる。
     docs/images/ に配置して ![](docs/images/xxx.png) で参照 -->

## 概要

| | |
|---|---|
| ジャンル | PvP 対戦アクション |
| エンジン | Unreal Engine 5 / C++ |
| プラットフォーム | Windows |
| 開発期間 | TODO: 2026.xx - |
| 制作人数 | TODO: x 名 |
| 担当 | TODO: 例）ゲームプレイ実装、ネットワーク同期、UI |

## 技術的な取り組み

<!-- TODO: 「何を作ったか」ではなく「何を考えて、どう解いたか」を書く。
     採用側が見るのはここ。各項目1-3行で、実装ファイルへのリンクを添えると強い。 -->

- TODO: 例）キャラクターのステート管理を GAS ベースで実装し、〜
- TODO: 例）ネットワーク同期のラグ補正に〜を採用し、〜
- TODO: 例）〜のボトルネックを、〜により xx ms → xx ms に改善

## 構成

```
Source/     ゲームロジック (C++)
Config/     エンジン・プロジェクト設定
Content/    アセット
```

<!-- TODO: Source/ の主要モジュールが固まったら、ここに1行ずつ説明を足す -->

## ビルド

Unreal Engine 5.x と Visual Studio 2022 が必要。

1. `ToonStory.uproject` を右クリック →「Generate Visual Studio project files」
2. 生成された `.sln` を開き、`Development Editor` / `Win64` でビルド

開発環境のセットアップ手順は [docs/GIT_SETUP.md](docs/GIT_SETUP.md) を参照。
