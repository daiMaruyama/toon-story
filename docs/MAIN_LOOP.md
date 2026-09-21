# メインループ

## 実装した流れ

参加待ち → 全員Ready → 3秒カウントダウン → 試合 → 結果を8秒表示 → 同じマップを再ロード → 参加待ち。

- 電池ルールでは最低3人。接続順の最初の1人をHuman、残りをToyとする暫定割当。
- Rで自分のReadyを切り替える。PlayerControllerのServer RPCを経由し、サーバーが受理する。
- カウントダウン中のReady解除・参加・退出で開始を取り消す。陣営変更された人はReadyを解除する。
- 試合中・結果中は途中参加を拒否する。試合中の退出は勝者なしで中断して結果表示へ進む。
- 納品7個でToy勝利、全Toy収監または時間切れでHuman勝利。拾っただけでは加算しない。
- 再試合はServerTravelによるマップ再ロード。Ready、スコア、配置されたActorを新しいワールドから作り直す。

## UEで試す

1. ビルド済みのプロジェクトを開く。
2. コンテンツブラウザで `Content/Tests/MatchLoop/Lvl_MatchLoop` を開く。
3. Playの設定でプレイヤー数を **3**、ネットモードを **Play As Listen Server** にする。各参加者を操作できる別ウィンドウで実行する。
4. それぞれのゲーム画面をクリックして **R** を押す。HUDのReadyが3人になったら3秒後に試合開始。
5. テストマップの試合時間は **30秒**。操作せず待つとHuman勝利になり、8秒後に再ロードを試みる。
6. 再ロード後に待機状態とReady解除を確認し、もう一度各画面でRを押す。

キーが届かない場合はゲーム画面にフォーカスを戻す。コンソールから `MatchReady 1` / `MatchReady 0` でも要求できる。

PIEでのマップ再ロードとクライアント再接続は実通信での追加確認が必要。自動テストは遷移先の予約までを確認しており、複数ウィンドウでの描画・再接続完了までは検証していない。

## 設定

`BP_MatchLoopGameMode` のClass Defaultsで変更する。

| 設定 | 通常値 | テストマップ |
| --- | --- | --- |
| Enable Match Loop | BatteryTagでは有効 | 有効 |
| Minimum Players | 3 | 3 |
| Start Countdown Seconds | 3 | 3 |
| Round Duration Seconds | 600（仮） | 30 |
| Result Display Seconds | 8 | 8 |
| Required Batteries | 7 | 7 |

BatteryTagはルール側でも最低3人を要求するため、Minimum Playersを3未満にしても開始しない。増やすことはできる。

専用BPは既存GameModeを複製して入力とPawn設定を継承し、親をBatteryTagGameModeに変更している。GameStateはBatteryTagGameState、PlayerStateはToonStoryPlayerState、HUDはToonMatchHUD。既定マップの変更はしていない。

## 担当別の接続

- **ロビー担当**：外部マッチングは未実装。今回のReady・陣営割当を置き換える場合、PlayerStateと開始判定を統合する。
- **納品担当**：所持・距離・操作をサーバー検証後、`TryRecordBatteryDeposit(PlayerState, BatteryId)` を呼ぶ。
- **捕獲・救出担当**：成立時に `SetToyCaptured(PlayerState, true/false)` を呼ぶ。
- **UI担当**：GameStateの状態と進捗、PlayerStateの陣営・Readyを表示する。`GetPhaseRemainingSeconds()` は開始待ち・試合・結果の残り時間を返す。

テストマップは床・光・開始位置のある確認用空間。電池Actor、納品地点、捕獲操作、陣営別Pawn、試合外の移動制限は今回追加していない。既存勝敗APIとゲーム進行の接続までが今回の範囲。

## 構成と拡張

- ToonStoryGameMode：参加者、Ready、フェーズ、タイマー、再ロード。
- ToonStoryGameState：全員に同期する進行状態。
- ToonStoryPlayerState：各人の陣営とReady。
- BatteryTagGameMode：今回の陣営割当と勝敗。新ルールは共通GameModeを継承して追加する。
- ToonMatchHUD：確認用の簡易表示。

## 検証

2026-09-19：Development Editor / Win64ビルド成功。`ToonStory.Match` の7件成功、失敗0件。既存の勝敗境界テストに、Readyによる開始・解除・人数不足・退出と再割当・結果後の再ロード予約を追加。レポートは `Saved/Automation/MatchLoop/index.json`。

専用BPのコンパイルとマップ保存、ヘッドレスゲーム起動を確認。実際の複数クライアント通信とHUDの見た目は未確認。

Contentは別Gitリポジトリ。今回追加した `Tests/MatchLoop/` の2アセットはContent側で管理する。既存の未コミットController・入力アセットは今回変更していない。コミット・pushは未実施。push前の理解度クイズは本人が受ける。
