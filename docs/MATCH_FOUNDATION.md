# 試合基盤：実装と接続手順

この変更は GameMode / GameState の土台です。マッチング・納品アクター・捕獲アクター・HUD は別途接続します。既存マップの GameMode やアセットは変更していません。

## 読む順番

1. `Source/ToonStory/Core/ToonMatchTypes.h`：試合状態のデータ。
2. `Source/ToonStory/Core/ToonStoryGameState.h/.cpp`：データの保持、同期、表示側への通知。
3. `Source/ToonStory/ToonStoryGameMode.h/.cpp`：試合開始・期限・終了の共通処理。
4. `Source/ToonStory/Core/BatteryTagGameState.h/.cpp`：電池と収監人数の共有データ。
5. `Source/ToonStory/Core/BatteryTagGameMode.h/.cpp`：今回の勝敗ルール。
6. `Source/ToonStory/Tests/MatchFoundationTests.cpp`：呼び出し例と境界条件のテスト。

`.h` は使える機能・データの宣言、`.cpp` は処理の実装です。

## 共通部分と固有部分

```
AGameModeBase
  AToonStoryGameMode         Waiting → Playing → Finished、期限、終了の一意化
    ABatteryTagGameMode     納品・収監・救出を受けた勝敗判定

AGameStateBase
  AToonStoryGameState        フェーズ・終了予定時刻・勝者・終了理由
    ABatteryTagGameState    納品数・必要数・人形数・収監数
```

既存の Blueprint 参照を維持するため、既存 `AToonStoryGameMode` は移動・改名していません。新規の基盤クラスは `Core/` に置きました。

別ルールを追加するときは、共通 GameMode / GameState を継承します。共通層には電池・人形固有の勝敗条件を入れません。今回は試合ごとに1ルールを選ぶ前提で、複数ルールの動的合成は対象外です。

## 設定とルール

- `RequiredBatteries` の既定値は7。試合開始時に今回の値を固定。
- `RoundDurationSeconds` の既定値は600秒。10分は仮値なのでチームで確認すること。
- 納品数が目標に到達すると `Toy / BatteryGoal`。
- 全員収監されると `Human / AllCaptured`。
- 時間切れは `Human / TimeExpired`。
- 救出で収監を解除。収監回数による脱落なし。
- 人形0人では試合開始不可。
- 試合の開始条件（Human人数・Readyなど）はロビー担当が追加確認し、`TryStartRound()` を呼ぶ。人形がいることだけで人数構成が完成したとは扱わない。
- 1ワールドで1試合。再試合は新しいワールドをロードする想定で、同じワールドの状態リセットは未実装。

## 未確定箇所の暫定実装

次の項目はユーザーが確定した仕様ではなく、確認・変更が必要な実装上の仮定です。

1. **時刻の境界**：サーバーが処理する時刻が期限より前なら納品を認め、期限ちょうど以降は拒否。タイマーのコールバックが遅れていても同じ判定を行う。クライアントの入力時刻への巻き戻し補償はない。
2. **納品と収監の競合**：サーバーのゲームスレッドで受理した順に処理。既に収監済みなら納品不可。終了が確定した後は別のイベントで結果を上書きしない。同一フレーム内のイベントを集めて優先順位を付ける方式ではない。
3. **切断**：試合中の `Logout` は暫定的に勝者なしの `Aborted` とする。観戦者も含めて現状は中断する。登録PlayerStateが消失した場合も、次のルール処理または期限処理で中断。切断即敗北や再接続ルールはネットワーク担当と決める。
4. **陣営**：PlayerStateの独自陣営型はまだないため、ロビー側がサーバー上で `RegisterToy()` した参加者をToyとして扱う。後でPlayerState担当の型につなぐ。クライアントから名簿を登録させない。

## 他担当が呼ぶAPI

すべてサーバー側の呼び出しです。`BlueprintAuthorityOnly` に加えてC++でも権限を確認します。これらはServer RPCではなく、クライアントからGameModeへ直接呼び出すことはできません。

| 関数 | 呼ぶ側と条件 |
| --- | --- |
| `RegisterToy(PlayerState)` | ロビーが陣営確定後、待機中に呼ぶ。同じPlayerStateの重複登録不可 |
| `UnregisterToy(PlayerState)` | 待機中の退出・陣営変更。試合中は名簿を変更しない |
| `TryStartRound()` | ロビーが必要人数・Readyなどを確認した後に呼ぶ |
| `TryRecordBatteryDeposit(PlayerState, BatteryId)` | 納品システムが所持・距離・操作成立をサーバー上で検証した後に呼ぶ |
| `SetToyCaptured(PlayerState, true)` | 捕獲システムが収監を成立させたとき |
| `SetToyCaptured(PlayerState, false)` | 救出システムが救出を成立させたとき |
| `AbortRound()` | 技術的に続行不能な場合。勝者はNone |

### 納品側の重要な契約

この基盤は納品地点・電池Actor・持ち物を知らないため、それらの物理的な検証は行いません。APIがあることを「誰でも安全に納品要求を送れる」と解釈しないでください。

1. 所有するPlayerController/PawnのServer RPC等で要求を受ける。
2. サーバーで実際の所持、納品地点との距離、身体状態、必要なホールド時間を確認する。
3. 電池ごとの**安定したID**で `TryRecordBatteryDeposit()` を呼ぶ。呼び出すたびにIDを生成すると重複防止が働かない。
4. 成功した場合だけ納品済みにして持ち物を外す。同じゲームスレッド上で完結させ、非同期処理を途中に挟まない。
5. GameStateの通知先は表示専用とする。勝利通知はこの呼び出しの戻り前に発生するため、通知中にマップを即時破棄せず、遷移は次のフレーム等へ予約する。

身体の位置・Frozenなどの状態はCharacter側に残ります。ここで持つ収監フラグは勝敗集計用の情報です。Character側の収監状態が確定したときだけ通知し、別々の場所が独自に変更しないようにします。

## HUDの接続

- `GetMatchStatus()`：フェーズ、サーバー基準の終了予定時刻、勝者、終了理由。
- `GetRemainingSeconds()`：同期されたサーバー時刻から算出した残り秒。待機・終了後は0。
- `GetProgress()`：電池・人形・収監の各数。
- `OnMatchStatusChanged` / `OnProgressChanged`：各値の変更通知。

HUD生成時は通知を購読し、**直後に現在値も読んで初期描画**します。遅れて開いたHUDは過去の通知を受け取れません。時計の表示はローカルTimer等で定期更新します。UIは勝敗を決めません。

サーバーで直接値を書いた場合もDelegateを通知するので、リッスンサーバーの画面も更新対象です。クライアントではRepNotifyから同じ通知を出します。Delegate自体はネットワーク通信ではありません。

共通状態とルール進捗は別の同期プロパティです。クライアントでその到着順に依存せず、どちらの通知でも現在値から表示を更新してください。

## エディタで試す手順

1. 既存の `BP_ThirdPersonGameMode` とマップはそのままにする。
2. 必要に応じて専用のテスト用GameMode Blueprintを作り、親を `BatteryTagGameMode` にする。既存BPを複製・親変更する場合は、Pawn / Controller / Input設定を確認する。
3. そのBPの Game State Class を `BatteryTagGameState` にする。
4. テスト用マップの GameMode Override にそのBPを指定する。
5. ロビー側からToyのPlayerStateを登録し、全員準備できたら `TryStartRound()` を呼ぶ。
6. テスト用に試合時間を短くして、納品・収監・時間切れを確認する。
7. PIEのホスト＋クライアントで、各画面の状態が一致することを確認する。

今回はアセット接続・複数プロセス通信の目視試験は未実施です。自動テストは一時ワールド内で本物のGameMode/GameStateを生成してルールを確認しますが、ネットワーク通信試験の代用にはなりません。

## ビルドと自動テスト（このPC）

UEを閉じて、PowerShellで実行します。初回は新しいC++ファイルを認識させるためプロジェクト再生成を行います。

```powershell
& 'E:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' -projectfiles '-project=E:/clony/toon-story/toon-story/ToonStory.uproject' -game -rocket
& 'E:/Epic/UE_5.8/Engine/Build/BatchFiles/Build.bat' ToonStoryEditor Win64 Development '-Project=E:/clony/toon-story/toon-story/ToonStory.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/clony/toon-story/toon-story/ToonStory.uproject' -unattended -nop4 -NullRHI -nosound '-ExecCmds=Automation RunTests ToonStory.Match' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/clony/toon-story/toon-story/Saved/Automation/MatchFoundation' '-abslog=E:/clony/toon-story/toon-story/Saved/Logs/MatchFoundationTests.log'
```

終了コードだけでなく、レポートの実行件数・失敗件数を確認します。テストが0件でもUEプロセスが正常終了することがあります。

## 今回の検証結果（2026-09-18）

- Development Editor / Win64 ビルド成功。
- `ToonStory.Match` の5件が成功、失敗0件、警告付き成功0件。
- 検証内容：開始条件、二重納品、目標到達、4回の収監・救出、全員収監、期限前と期限ちょうどの納品、タイマー単独終了、終了結果の上書き防止、参加者消失時の中断。
- 既存 `Lvl_ThirdPerson` を `-game -NullRHI` で起動し、`BP_ThirdPersonGameMode_C` と `ToonStory GameMode BeginPlay` の実行ログを確認。
- ビルドにはUE既存の非推奨API・推奨外コンパイラ警告あり。画面なしのゲーム起動では、エディタ用ToolsetsのPythonクラスがないというログも出る。ゲーム開始ログの確認と、エディタ用MCPプラグインの動作確認は別であり、後者を正常と判定したものではない。
- レポート：`Saved/Automation/MatchFoundation/index.json`。開始ログ：`Saved/Logs/MatchFoundationSmoke.log`。

## 引き継ぎ

- PlayerState / ロビー担当と陣営・開始条件を接続する。
- 納品 / 捕獲担当とサーバー側APIを接続する。
- 期限境界・イベント競合・切断の暫定ルールをチームで承認または変更する。
- テスト用BP・マップ・HUDを用意し、実通信で同期を確認する。
- 読んで理解した後、対象ファイルだけコミット。クイズは本人が受ける。合格後にpushする。
