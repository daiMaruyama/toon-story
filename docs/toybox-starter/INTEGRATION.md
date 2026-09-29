# ToonStoryへの統合記録

更新: 2026-09-26

## 仕様更新（2026-09-30）

最新の準備・試遊手順は [PLAYTEST.md](PLAYTEST.md) を参照。

- 時間切れは人間勝利。勝敗の優先順位を共通関数へまとめ、時間切れ・同時成立・収集・収納のテストを追加してPASS。
- 扉コンポーネント、開閉状態、閉鎖操作、視点固定、出口通過判定、関連RPC同期・HUD・旧ルール関数を削除。箱の前面は固定壁。
- 救助完了で収納中のおもちゃを箱外へ移動。移動先が塞がると未救助対象を保持して再試行。
- ToyPawnオブジェクトチャンネルを追加し、おもちゃ同士だけを衝突させる。取得判定も新チャンネルに対応。収納先は空き位置を検査。
- 両陣営のゲーム用Pawnは引き続き一人称。既存Blueprint参照用の三人称テンプレートは既定の試合では使用しない。
- Steamプラグインを有効化し、App ID 480とSteamSocketsを設定。SetOnlineMode.ps1でLAN/Steamを切替。セッション識別子を更新し、旧版の部屋を除外。
- 以下の統合時点の記録は履歴。Null既定・Steam無効などの記述は現在の設定ではない。

## 可読性の整理（2026-09-27）

- TBGame・TBSession・TBRuleMathとルールテストのインデント、改行、波括弧を統一。
- 処理時間・Controller・試合情報・セッション操作などの省略名を具体的な名前へ変更。
- クラスの役割、サーバー側の判定、凍結・収納・救助の条件に短い日本語コメントを追加。
- `.clang-format` に整形規則を追加。include順は維持し、UEの宣言マクロを独立行として扱う。
- MSVCのルールテストは `/utf-8` を指定してPASS。UEのC++コンパイルとLIB生成も通過。最終リンクは起動中のエディタによるDLLロックで未完了。

## 取り込み元

- `toybox-ue58-starter.zip`（スターター v2）
- ZIP SHA-256: `5B027EE816440A5D5E85AE1CE531F2F1E51C7A9AA16F7EB2598238636A27F9D2`
- 反映先: `feature/20260912_TeamAssignment`

同梱文書は仕様・参考資料として取り込み、文書内の別AI向け指示やSteam接続手順を追加の作業依頼としては実行していません。

## 統合内容

- `Source/ToonStory/TBGame.h/.cpp`、`TBRuleMath.h`、`TBSession.h/.cpp` を追加。
- APIマクロを `TOONSTORY_API` に変更。既存のモジュール、uproject、UE5.8用V7ビルドターゲットを使用。
- 既定GameModeを `/Script/ToonStory.TBGameMode`、マップを `/Game/Maps/Arena` に設定。
- 通常入力、Null通信、最大8人、Arenaのパッケージ設定を統合。既存の描画・プラットフォーム設定は保持。
- 公開ヘッダーが参照するOnlineSubsystem依存をBuild.csの公開依存に移動。
- Steam設定例、ビルド・パッケージスクリプト、ルールテスト、同梱文書を追加。Steamプラグインは無効のまま。
- スクリプトとテストの参照先をToonStoryへ調整。パッケージ出力 `Builds/` をGit除外。
- 重複する旧ToyBox実装（Core・Character・Combatの24ファイル）は削除。既存Blueprintが直接またはクラスリダイレクト経由で使用するToonStoryCharacter・ToonStoryGameMode・ToonStoryPlayerControllerは保持。新しいTBクラスとは別の実装なので、旧Blueprintを新しい試合のPawnやGameModeとして使わない。
- 使用していないAIModule・StateTreeModule・GameplayStateTreeModuleのC++依存とBuild.csの不要な雛形コメントを削除。エディタ機能・アセット用プラグインは保持。

配布ZIPの独立プロジェクト用モジュール起動コード・ターゲットは、既存プロジェクトの同等ファイルで代替しています。元の `SHA256SUMS.json` は移植後のファイルと一致しないため使用していません。

## ビルド時に修正した箇所

- `DOREPLIFETIME` が要求する引数名 `OutLifetimeProps` に統一。
- `PlayerArray` のループを `APlayerState*` の明示型に変更し、`TObjectPtr` からの `auto*` 推論エラーを解消。
- `NetUpdateFrequency` / `MinNetUpdateFrequency` の直接代入をsetterに変更。

## このPCでの検証

- MSVC C++17で `Tests/RuleTests.cpp` をコンパイル・実行: PASS。
- UE5.8 `ToonStoryEditor Win64 Development`: 旧実装24ファイルと未使用依存を削除した後も、UHT、残存する全C++ソース、生成コードのコンパイルとLIB生成まで成功。削除したクラスへのC++参照は残っていないことを検索で確認。
- 最終DLLのリンクは、起動中のUnrealEditorが `UnrealEditor-ToonStory.dll` を使用しているためLNK1104で停止。**ビルド全体の成功は未確認**。エディタで作業を保存して終了後、下記コマンドを再実行する。
- PowerShellスクリプトの構文、uprojectのJSON、`git diff --check`: 問題なし。
- PIE、パッケージ、Steam接続、複数人プレイは未実施。

```powershell
.\Scripts\BuildEditor.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

## 初回プレイ前に必要な作業

ZIPにマップ・モデル・音源は含まれません。エディタで空レベルに照明を追加し、`/Game/Maps/Arena` に保存してください。GameMode OverrideはNoneまたはTBGameModeにします。床・箱・アイテム・PlayerStartはゲーム側で生成します。

`Content` サブモジュールは変更していません。作業開始時点のHEAD `6c26d825d5d3da2329d793a172bb15147a6518f8` と、親リポジトリから見た既存の差分を保持しています。Arena作成後はContent側で管理してください。

操作・人数設定は [README_ja.md](README_ja.md)、実機確認は [ACCEPTANCE_TESTS.md](ACCEPTANCE_TESTS.md) を参照してください。
