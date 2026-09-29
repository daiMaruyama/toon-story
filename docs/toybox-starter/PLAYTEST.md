# 試遊手順（2026-09-30・現在の仕様）

このページが最新です。ZIP由来の文書にある扉開閉・時間切れでおもちゃ勝利の記述は旧仕様です。

確認状況: Windows Developmentのゲーム本体とエディタ用ビルド、ルール単体テストは成功しています。Arenaは作成済みで、保存後の再読み込み・TBGameMode・照明3種類を検証しました。画面上の見た目、パッケージ作成、PIEでの操作、Steam実接続は未検証です。

## ルール

- 全員一人称。人間は450cm/s、おもちゃは350cm/s、運搬中は300cm/s。
- 制限時間0で人間勝利。時間が残っている間に必要アイテムを取得するとおもちゃ勝利。全員収納でも人間勝利。
- 箱は固定壁で閉じており、扉の開閉・閉鎖操作・自力脱出はありません。
- 自由なおもちゃが外の操作位置でEを押し続けると救助。1人10秒、複数人で加速。完了したら箱外へ移動します。移動先が塞がっている場合、その対象は残して再試行します。
- おもちゃ同士は衝突します。人間とおもちゃ、人間同士はすり抜けます。運搬中の対象は衝突しません。
- 新しい通信識別子を使うため、旧ビルドと混ぜず、全員に同じパッケージを配ってください。

## UE側で最初に用意するもの

`Content/Maps/Arena.umap` は作成済みです。Content BrowserのMapsからArenaを開いて、そのまま試遊できます。エディタ上では照明と空が表示され、床・壁・箱などはPlay後に生成されます。以下は作り直す場合の手順です。

エディタで作業を保存して終了し、`Scripts/BuildEditor.ps1`で通常ビルドしてください。新規クラス・削除クラス・衝突チャンネル・プラグインの変更があるため、再起動が必要です。

現在のContentには `ThirdPerson/Lvl_ThirdPerson`、`SandBox/Lvl_ChildRoom`、`Tests/MatchLoop/Lvl_MatchLoop` がありますが、既存レベルのGameMode Overrideや配置物は未検証です。最初は以下の空マップが確実です。

1. File > New Level > Empty Levelを作る。既存レベルの未保存作業は先に保存する。
2. Directional LightとSky Lightを1つずつ配置し、MobilityをMovableにする。Sky Lightは必要に応じてRecaptureする。
3. World SettingsのGameMode Overrideを `TBGameMode` にする。使用するPawnは `TBCharacter`。
4. `/Game/Maps/Arena`、つまり `Content/Maps/Arena.umap` に保存する。
5. Maps & Modesの既定マップがArena、既定GameModeがTBGameModeであることを確認する（設定ファイルは反映済み）。

床・壁・箱・アイテム8個・開始位置8個・仮キャラクターはC++がEngine標準のCube/Sphereで生成します。床やPlayerStartを追加する必要はありません。新しいモデル、アニメーション、外部プラグインの購入は不要です。警報音だけは未設定なので、表示のみでテストできます。

Contentはサブモジュールです。追加したArenaはContent側でコミットし、その後に親リポジトリの参照を更新してください。既存マップは変更していません。

## 1台でルールを確認する（Steamを使わないPIE）

エディタを終了し、プロジェクトルートのPowerShellで実行します。

```powershell
.\Scripts\SetOnlineMode.ps1 -Mode LAN
```

エディタを再起動してArenaを開き、Playの設定をNumber of Players=3、Net Mode=Play As Listen Server、New Editor Windowにします。PIEの接続を使うため、この段階ではTBHost/TBFindを呼びません。

F10でコンソールを開き、ホストで次を入力します。

```text
TBRules 1 2 60 5
TBPrefer 1
```

おもちゃ役のウィンドウで `TBPrefer 2`。全員R、ホストF5で開始。設定変更後はReadyが解除されるので、Rを押し直します。

- WASD移動、マウス視点、Spaceジャンプ。
- 人間: Eで捕獲、箱の外の小さな操作マーカーでE長押し3秒で収納、Qで落とす。
- おもちゃ: アイテムに3秒接触して取得、箱の操作マーカーでE長押しして救助。
- 時間切れを手早く試すなら次の試合で `TBRules 1 2 10 5`。
- 2人なら `TBRules 1 1 60 5`。ただし唯一のおもちゃを収納すると勝利が確定し、救助の検証はできません。
- 結果画面から次の試合を試すときはPIEを停止し、再度Play。

受入確認: 凍結中の落下、視線解除、取得進捗の停止/再開、おもちゃ同士の衝突、収納位置の重なり、救助時の箱外移動、残り0秒でHUMANS表示を確認してください。

## Steam App ID 480で別PCと接続

このリポジトリはOnlineSubsystemSteamとSteamSocketsを有効化済みです。App ID 480は開発者共通のテストIDで、製品公開用ではありません。

1. 2台以上のWindows PCと、それぞれ別のSteamアカウントを用意。救助とおもちゃ同士の衝突まで試すなら3台・3人で行う。
2. 各PCでSteamを起動してオンライン状態でログイン。エディタ内の複数窓によるSteam実接続テストは避け、Developmentパッケージを使う。
3. エディタを終了し、以下でSteamへ切り替えてパッケージする。

```powershell
.\Scripts\SetOnlineMode.ps1 -Mode Steam
.\Scripts\PackageWindows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

4. `Builds/Windows` 以下のパッケージ全体を各PCへコピー。exeだけをコピーしない。
5. スクリプトはDevelopmentテスト用 `steam_appid.txt`（内容480）を実行ファイルのフォルダーへ配置する。手動パッケージの場合は必要に応じて同じファイルを置く。Steamへの製品アップロードには含めない。
6. 全員がパッケージのToonStory.exeを起動。ホストはF10で `TBHost`、参加者は `TBFind`、結果が表示されたら `TBJoin 0`（目的の部屋の番号）を実行。
7. 接続後にホストが `TBRules 1 1 60 5`（2人）または `TBRules 1 2 60 5`（3人）を設定。全員R、ホストF5。
8. 終了・退出は `TBLeave`。次の試合ではホストが再度 `TBHost` し、参加者も再参加する。

480は共有IDなので、部屋一覧の所有者名を確認してください。コードは独自の識別子で結果を絞りますが、部屋検索・Steam招待・異なる回線での接続は実機検証が必要です。

接続できない場合は `-log` を付けて起動し、`LogOnline`・`STEAM`・`SteamSockets`と `[ToyBoxSession]` を確認します。Steamログイン、同じビルド/モード、Arenaの保存とCook、Windows Firewallの許可を確認してください。LANに切り替えた場合は再パッケージが必要です。

手元のUE5.8には `Engine/Binaries/ThirdParty/Steamworks/Steamv164/Win64/steam_api64.dll` が存在します。初回テストに向けたSteamworks SDKの差し替えは不要です。配布先でDLLエラーが出る場合はパッケージ全体のコピーを確認してください。

## 公式資料

- [Epic: Online Subsystem Steam（App ID 480、steam_appid.txt）](https://dev.epicgames.com/documentation/unreal-engine/online-subsystem-steam-interface-in-unreal-engine)
- [Epic: Steam Sockets（プラグインとNetDriver）](https://dev.epicgames.com/documentation/unreal-engine/using-steam-sockets-in-unreal-engine)

旧 `Profiles/Steam` の設定例を追加コピーする必要はありません。現在は `SetOnlineMode.ps1` でDefaultEngine.iniを切り替えます。
