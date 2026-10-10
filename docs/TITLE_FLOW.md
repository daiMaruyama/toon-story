# Titleと画面遷移

ゲームは `/Game/Maps/Title` から起動します。Titleは独立したレベルで、Pawnを生成しません。デザインは仮の背景・文字・UMGメニューです。エディタの起動先は引き続きTB_KidsRoomです。

## 操作

1. TitleでHost roomを押す、またはFind roomsで検索して一覧から選びJoin selected roomを押す。
2. ステージ内のロビーで役割希望・人数設定・Readyをそろえ、ホストがStart matchを押す。
3. 結果からReturn together to lobbyで同じ部屋のまま再戦する。
4. ロビー・結果のLeave room / Close roomでTitleへ戻る。試合中はEscのメニューから退出できる。ホストの退出は部屋を閉じる。

試合中のメニューはゲームを一時停止しません。PIEではEscがPlay終了に割り当てられていることがあるため、その場合はF10コンソールのTBLeaveで退出処理を確認してください。

部屋作成・検索・参加はUTBSessionの既存処理を使います。Titleの一覧はRoomsを読み、Changedを購読して更新します。待機・試合・結果は従来どおり同じステージのETBPhaseで扱い、再戦のReturnToLobbyは変更していません。

## 失敗時

作成・検索の失敗はTitleで理由を表示して再試行できます。参加失敗・接続失敗・移動失敗ではセッションを片付け、Titleへ戻して理由を表示します。接続している人の退出だけでは、ホストや残る参加者をTitleへ戻しません。

セッション破棄が失敗・タイムアウトした場合もTitleへ戻ります。残るセッションがある間は新しい作成・検索・参加を無効にし、Retry connection cleanupを表示します。ローカルの記録だけを消して破棄成功扱いにする処理はありません。

## 確認手順

1台のTitle試験ではSteamを使わず、エディタを終了してSetOnlineMode.ps1でLANへ切り替えます。Titleを開き、互いに自動接続しない2つのStandalone実行から作成・検索・参加してください。PIEのPlay As Listen Serverによる自動接続だけではTitleの入室経路を検証できません。

- ホストが部屋作成、もう一方が検索・参加できること。
- 2人の場合はホストで `TBRules 1 1 10 5`。全員Ready、ホストStartで試合開始・時間切れ結果へ進むこと。
- 再戦ロビー復帰時に接続を維持し、Readyが解除され、2試合目を開始できること。
- 参加者が退出するとTitleへ戻り、ホストは部屋に残ること。
- ホストが部屋を閉じる、またはプロセスを終了すると、参加者が切断検出後にTitleへ戻ること。
- Titleに戻った後、作成・検索・参加を再度実行できること。
- 存在しない接続先・マップで失敗した場合、Titleで理由が表示され、操作を再開できること。

自動テスト `ToonStory.Title.RoundTrip` はNull環境のTitleから実際にセッション作成・マップ移動を行い、2人目のローカルプレイヤーで試合・結果・再戦を通し、退出、再ホスト、移動失敗からのTitle復帰を確認します。別プロセス間の通信試験とは別です。

### 自動テストの起動方法

エディタを終了し、Editorターゲットをビルドしてから、プロジェクト直下で実行します。レポート内のfailedを確認してください。プロセスの終了コードだけではテスト成功を判断できません。

```powershell
$UE = 'E:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$Project = (Resolve-Path ./ToonStory.uproject).Path
& $UE $Project /Game/Maps/Title -game -NullRHI -nosound -nosteam -unattended -nop4 '-ExecCmds=Automation RunTests ToonStory.Title.RoundTrip' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD/Saved/Automation/TitleRoundTrip"
```

`ToonStory.Title.NetworkPeer` は2つのゲームプロセスで使います。上のテスト名を置き換え、ホスト側に `-TitlePeer=Host -TitleRun=任意の試験ID`、参加側に `-TitlePeer=Client -TitleRun=同じ試験ID` を付け、それぞれ異なるレポート保存先を指定します。ホスト側のロビーが開いてから参加側を起動してください。試験ID付きの部屋だけを選び、10秒の試合を2回行った後、ホスト退出と参加者のTitle復帰を検証します。

LAN検索と参加後の通信を切り分ける場合に限り、参加側へ `-TitleDirectAddress=127.0.0.1:7777` を追加できます。この診断ではFind/JoinSessionを通らないため、検索・一覧からの参加や参加側のオンラインセッション破棄を検証したことにはなりません。

### 2026-10-10の確認記録

- UE 5.8 / Win64 DevelopmentでEditor・ゲーム本体の両ターゲットをビルド成功。Cook・配布パッケージの作成は未実施。
- Titleの画面を描画して仮UIの配置を確認。
- `RoundTrip` 成功（failed=0）。意図して発生させた移動失敗の警告が1件あるため、レポートはSucceeded With Warnings。
- 同一PCの2プロセスを直接接続した `NetworkPeer` 成功（ホスト・参加者ともfailed=0）。2試合、再戦時の接続維持・Ready解除、ホストの部屋終了後の両者のTitle復帰を確認。参加者側は意図した切断の警告が残るためSucceeded With Warnings。
- 同一PC・Null/LANの検索経由試験では、作成した部屋が検索結果に出ずタイムアウト。検索経由での入室は未確認。原因は特定しておらず、今回の変更では検索処理の内部やWindowsのネットワーク設定を変更していない。

Steam・別PCからの検索／参加、参加者自身の退出後に残ったプレイヤーが継続できること、破棄失敗・タイムアウト時の再試行ボタンは追加の実機確認が必要です。

ローカルの詳細レポートは `Saved/Automation/TitleRoundTripVerified/index.json`、直接接続試験は `Saved/Automation/TitleDirectHostVerified/index.json` と `TitleDirectClientVerified/index.json`、検索試験は `Saved/Automation/TitleNetworkHost/index.json` と `TitleNetworkClient/index.json` に保存しています。Saved以下はGit管理外です。

## 配置とパッケージ

Title.umapはContentリポジトリ、コードと設定は親リポジトリです。TitleのGameMode OverrideはTBTitleGameMode、UIはTBTitleMenu、操作担当はTBTitleControllerです。MapsToCookにもTitleを登録しています。

Scripts/CreateTitleMap.pyは新規Titleを一度だけ作るための補助で、存在するTitleは上書きしません。保存済みTitleは他のマップと同じようにエディタから編集してください。本番ステージの再生成には使用しません。
