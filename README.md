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

## どんなゲームか

子ども（人間）とおもちゃに分かれて戦う、非対称のオンライン対戦。

- おもちゃは人間に見られている間は動けない。見られていない隙に、部屋に散らばった電池を集める
- 人間はおもちゃを掴んで運び、おもちゃ箱に収納する
- 収納されたおもちゃは、仲間が箱の外から救助できる。救助を待つ間は、箱の中のテレビで外の様子を見られる
- 制限時間内に必要数の電池を集めればおもちゃの勝ち。時間切れか全員収納で人間の勝ち

## 担当

### 丸山

| 範囲 | 内容 | 主なファイル |
| --- | --- | --- |
| カメラ | 運ばれているおもちゃの三人称カメラ、掴まれた／降ろされた時の視点ブレンド | `Character/TBCharacter.*` |
| 箱内テレビ | 収納されたおもちゃが見るテレビ。固定カメラと他プレイヤー視点のチャンネル、砂嵐。床のボタンを踏んで操作するリモコン | `GamePlay/TBBoxTV.*` `TBTVRemote.*` `TBTVButton.*` `TBTVCamera.h` `Tests/TBTVTests.cpp` |
| 画面演出 | 視点切替や移動を隠すトランジション | `UI/TBScreenTransition.*` |
| エディタ拡張 | MCP サーバーの操作と AI クライアント設定の生成（下記） | `Plugins/MaruyamaEditorExtension/` |
| 開発フロー | /quiz による PR 前の理解度チェック（下記） | `.agents/` `.github/workflows/quiz.yml` |
| 環境構築 | リポジトリ構成（Content の submodule 分離）、Windows / macOS のセットアップ手順、clangd 設定 | `docs/GIT_SETUP.md` `.clangd` |

### 寺田

| 範囲 | 内容 | 主なファイル |
| --- | --- | --- |
| 対戦ルール | 陣営の割り振り、視線による凍結、掴む・運ぶ・収納、救助、電池の収集、制限時間と勝敗判定 | `Core/TBGameMode.*` `Character/TBCharacter.*` `GamePlay/TBBox.*` `TBPickup.*` `TBRuleMath.h` |
| オンライン | 部屋の作成・検索・参加（Steam / LAN）、切断時の処理、同じ部屋での再戦 | `Core/TBSession.*` `TBController.*` |
| キャラクター | 子ども・ロボットの見た目とアニメーションの組み込み、ダッシュ | `Character/TBMovement.*` |
| ステージ | 子ども部屋・アパートの 2 ステージの配置、検証用の Arena | `docs/STAGE_EDITING.md` |
| HUD | 試合状況・操作案内の表示 | `UI/TBHUD.*` |
| テスト・ツール | ステージ・再戦の Automation テスト、ルール計算の単体テスト、受入テスト表、ビルド・パッケージ用スクリプト | `Tests/` `Scripts/` `docs/ACCEPTANCE_TESTS.md` |

### 薮田

| 範囲 | 内容 | 主なファイル |
| --- | --- | --- |
| タイトル | タイトル画面と、部屋の作成・参加・退出までの画面遷移（開発中） | `UI/TBTitleMenu.*` `Core/TBTitleGameMode.*` |

## 開発フローの取り組み

AI コーディングエージェント（Claude Code / Codex）を開発に取り入れている。
そのうえで、コードの中身をチーム全員が説明できる状態を保つため、次の 2 つを自作した。

### /quiz — PR 前の理解度チェック

エージェントを使うと、書いた本人も中身を把握しないまま PR を出せてしまう。
そこで PR の前に、そのブランチの差分から 4 択 3 問を出題し、2 問以上正解しないと push とマージに進めない仕組みにした。

| 段階 | 仕組み | ファイル |
| --- | --- | --- |
| 出題 | 差分の重要な動作変更から「どう動くか／なぜ必要か／影響範囲」を問う。回答は本人だけが行い、合格すると `.git/` にコミットハッシュつきの記録を残す。Claude Code と Codex で手順を共通化 | `.agents/skills/quiz/SKILL.md` |
| push | pre-push フックが合格記録と push するコミットを照合し、未合格・合格後にコミットが変わったブランチを止める | `.agents/githooks/pre-push` |
| PR | GitHub Actions が PR 本文の結果行（`Quiz: 2/3 @ <hash>`）を検査し、点数と head コミットの一致を確認する | `.github/workflows/quiz.yml` |

制限: ローカルのフックは無効化でき、PR 本文の点数は自己申告になる。
本人が回答したことを証明する仕組みではなく、理解を確認する手順を習慣にするためのもの。運用ルールは [AGENTS.md](AGENTS.md)。

### MaruyamaEditorExtension — MCP 操作のエディタ拡張

UE 5.8 の MCP サーバーを、エージェントからエディタを操作するために使っている。
サーバーの開始／停止、起動時の自動開始、ツールの再登録、Claude Code / Codex 用の接続設定ファイルの生成を、
レベルエディタのメインメニュー「丸山拡張」にまとめた。設定ファイルは個人の環境に依存するため Git 管理外にしている。

## 構成

```
Source/     ゲームロジック (C++)
  ToonStory/
    Core/         試合管理、プレイヤー状態、部屋作成・検索・参加
    Character/    キャラクター本体、移動、入力
    GamePlay/     捕獲・収納・救助・収集、箱内テレビ、共通ルール計算
    UI/           HUD、画面演出
    Data/         陣営・試合状態の型、設定
    Tests/        ステージ・再戦・箱内テレビの Automation テスト
    ToonStory.*   モジュール設定
Tests/      UE に依存しないルール計算の単体テスト
Config/     エンジン・プロジェクト設定
Content/    アセット（別リポジトリを submodule として接続）
Plugins/    エディタ拡張
Scripts/    ビルド・パッケージ・接続方式の切替
docs/       開発ドキュメント
.agents/    /quiz の手順と pre-push フック
```

クラス名の接頭辞 `TB` は ToyBox の略。

## ビルド

Unreal Engine 5.8 が必要。エディタは Windows / macOS どちらでも動く。

`Content/` は private リポジトリのため、アクセス権がないとアセットが取得できずエディタは起動しない。

```bash
git clone --recurse-submodules https://github.com/daiMaruyama/toon-story.git
```

**Windows**（Visual Studio 2022）

1. `ToonStory.uproject` を右クリック →「Generate Visual Studio project files」
2. 生成された `.sln` を開き、`Development Editor` / `Win64` でビルド

**macOS**（Xcode）

`ToonStory.uproject` をダブルクリックし、「modules are missing」のダイアログで **Yes** を押すとその場でビルドされる。

セットアップと開発フローの詳細は [docs/GIT_SETUP.md](docs/GIT_SETUP.md) を参照。

## テスト

**ルール計算**（UE 不要。凍結・接触進捗・救助速度・勝敗判定）

```bash
c++ -std=c++20 Tests/RuleTests.cpp -o RuleTests && ./RuleTests
```

**Automation テスト**（エディタの Session Frontend → Automation で `ToonStory` を実行）

## 試遊・検証

- [試遊手順](docs/PLAYTEST.md)
- [受入テスト](docs/ACCEPTANCE_TESTS.md)
- [UEエディタでのステージ編集](docs/STAGE_EDITING.md)
