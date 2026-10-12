# Titleと画面遷移

ゲームの起動先は `/Game/Maps/Title` です。TitleはPawnを生成しない独立したレベルで、背景・文字・UMGメニューは仮デザインです。エディタの起動先は引き続きTB_KidsRoomです。

## 担当範囲

Title UIから既存のUTBSessionのHost / Find / Join / Leaveを呼び、Rooms・Status・IsBusyを表示に使い、Changedで表示を更新します。通信処理の内部は別担当のため変更しません。

UTBSessionの変更は、ReturnOfflineの移動先をTB_KidsRoom（未配置時はArena）からTitleへ変更する箇所だけです。検索条件、オンライン設定、作成・参加・退出の非同期制御、切断・移動失敗処理は実装前のものを維持しています。待機・試合・結果は従来どおり同じステージのETBPhaseで扱い、再戦のReturnToLobbyも変更していません。

初回実装で追加していた通信状態管理、失敗時の自動後始末、タイムアウト処理、内部セッションにアクセスするNetworkPeerテストは取り消しました。以前の通信テスト結果は取り消し前の実装の記録であり、現在の切断処理の検証結果としては扱いません。

## 仕様との照合で残った事項

[Notionのタスク](https://app.notion.com/p/3f2c02ee3d45816ea82eed41a9e0e892)には「退出・セッション破棄・通信失敗・トラベル失敗は、すべてReturnOfflineを通る」とあるものの、現行のTravelFailedはStatusの更新のみで、自動退出しません。またNetworkFailedの退出予約はIdle時に限られ、現在のWorldのタイマーを使います。このため、戻り先の変更だけで失敗時のTitle復帰を保証することはできません。ネットワーク担当との確認事項であり、Title側に代替の通信制御は追加していません。

完了条件にある「PIE 2人で作成／検索／参加から退出まで一周」と「ホストを閉じたときのクライアントのTitle復帰」は、現在の実装での受け入れ確認が残っています。以下のローカルテスト成功だけで、このタスク全体を完了とは判定しません。

## 手動確認

1. コンテンツブラウザでMaps/Titleを開き、Standalone Game・Play Standaloneで再生する。
2. Host roomでステージ内のロビーへ移動し、Close room / TitleでTitleへ戻る。再度作成できることを確認する。
3. 2人で確認する場合は、もう一方がFind roomsで検索し、一覧から選んでJoin selected roomを押す。
4. ロビーで役割希望・人数設定・Readyをそろえ、ホストがStart matchを押す。2人の短時間試験ならホストで `TBRules 1 1 10 5` を使う。
5. 結果からReturn together to lobbyを選び、同じ部屋のまま再戦できることを確認する。
6. ロビー・結果のLeave room / Close room、または試合中のEscメニューから退出してTitleへ戻る。

試合中のメニューはゲームを一時停止しません。PIEではEscがPlay終了に割り当てられている場合があるため、その場合はF10コンソールのTBLeaveで退出を確認してください。

TitleのLeave / Reset roomは既存のLeaveを呼ぶためのボタンです。失敗時の扱い・再試行は既存処理に従い、画面にStatusを表示します。Title側で独自の切断監視や自動再接続、セッション破棄は行いません。

切断・参加失敗・移動失敗の挙動調整が必要な場合はネットワーク担当者と連携します。同一PC・Null/LANの試験では検索結果が0件だったため、検索経由の入室は未確認です。原因の断定や検索処理・Windowsネットワーク設定の変更は行っていません。

## 自動確認

`ToonStory.Title.RoundTrip` はTitleのFind・HostボタンのイベントとステージHUDの操作処理を通し、検索結果の表示、部屋作成・ステージ移動、10秒の時間切れによる結果、再戦、試合中・ロビーからの退出、再作成を確認します。試合中メニューの入力制限・再開と、Title UIの破棄・再生成も確認します。

参加者は2人目のローカルプレイヤーです。マウス座標からのクリック判定、実際のEscキー配送、検索結果からのJoin、ネットワーク障害処理、別プロセス／PIE通信の試験は含みません。ボタンのイベント経由と、実際のマウス・キー操作による検証は区別します。

エディタを終了し、Editorターゲットをビルドしてから、Null/LAN設定のプロジェクト直下で実行します。

```powershell
$UE = 'E:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$Project = (Resolve-Path ./ToonStory.uproject).Path
& $UE $Project /Game/Maps/Title -game -NullRHI -nosound -nosteam -unattended -nop4 '-ExecCmds=Automation RunTests ToonStory.Title.RoundTrip' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD/Saved/Automation/TitleExistingApi"
```

プロセスの終了コードだけではなく、Saved/Automation/TitleExistingApi/index.jsonのfailedが0であることを確認してください。Saved以下はGit管理外です。

2026-10-10のレビュー後、UE 5.8 / Win64 DevelopmentのEditor・ゲーム本体をビルド成功。上記のUIイベント経由テストは成功1・警告0・失敗0でした。実行時のレポートは `Saved/Automation/TitleReview/index.json`、ログは `Saved/Logs/TitleReview.log` に保存しています。コンパイラの推奨バージョンとEngine内の非推奨APIについてはビルド時の警告が残っています。

レビューでの修正は、マップ生成前のGameMode存在確認と、UIの呼び出し経路・時間切れ・再戦・退出を通すテストの追加です。通信コードの内部、陣営選択、結果集計には追加変更を行っていません。

現在のTitleを1280×720で描画し、文字・ボタンの欠けや重なりがないことも確認しました。確認画像は `Saved/Screenshots/WindowsEditor/ScreenShot00001.png` です。ほかの解像度と実入力による操作確認は、この描画確認には含みません。

## 配置とパッケージ

Title.umapはContentリポジトリ、コードと設定は親リポジトリです。TitleのGameMode OverrideはTBTitleGameMode、UIはTBTitleMenu、操作担当はTBTitleControllerです。MapsToCookにもTitleを登録しています。Cook・配布パッケージの作成確認は未実施です。

Scripts/CreateTitleMap.pyは新規Titleを一度だけ作るための補助で、存在するTitleは上書きしません。GameModeが未ビルドの場合はマップを作成する前に停止します。保存済みTitleは他のマップと同じようにエディタから編集してください。本番ステージの再生成には使用しません。
