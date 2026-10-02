# 試遊手順

ルールの数値は Notion の仕様を参照してください。このページのコマンド引数は動作確認用の例です。
制限時間切れ・全員収納で人間勝利、時間内の必要アイテム収集でおもちゃ勝利となります。
箱の救助は外側の操作位置で行い、完了すると仲間が箱外へ移動します。

## ビルド

```powershell
.\Scripts\BuildEditor.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
.\Scripts\PackageWindows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

## UE側で最初に用意するもの

`Content/Maps/Arena.umap` は作成済みです。Content BrowserのMapsからArenaを開いて、そのまま試遊できます。エディタ上では照明と空が表示され、床・壁・箱などはPlay後に生成されます。以下は作り直す場合の手順です。

エディタで作業を保存して終了し、`Scripts/BuildEditor.ps1`で通常ビルドしてください。新規クラス・削除クラス・衝突チャンネル・プラグインの変更があるため、再起動が必要です。

現在のContentには `ThirdPerson/Lvl_ThirdPerson`、`SandBox/Lvl_ChildRoom`、`Tests/MatchLoop/Lvl_MatchLoop` がありますが、既存レベルのGameMode Overrideや配置物は未検証です。最初は以下の空マップが確実です。

1. File > New Level > Empty Levelを作る。既存レベルの未保存作業は先に保存する。
2. Directional LightとSky Lightを1つずつ配置し、MobilityをMovableにする。Sky Lightは必要に応じてRecaptureする。
3. World SettingsのGameMode Overrideを `TBGameMode` にする。使用するPawnは `TBCharacter`。
4. `/Game/Maps/Arena`、つまり `Content/Maps/Arena.umap` に保存する。
5. Maps & Modesの既定マップがArena、既定GameModeがTBGameModeであることを確認する（設定ファイルは反映済み）。

床・壁・箱・検証用アイテム・定員分の開始位置・仮キャラクターはC++がEngine標準のCube/Sphereで生成します。床やPlayerStartを追加する必要はありません。警報音だけは未設定なので、表示のみでテストできます。

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
- 人間: Eで捕獲、箱の外の小さな操作マーカーでE長押しで収納、Qで落とす。
- おもちゃ: アイテムに接触し続けて取得、箱の操作マーカーでE長押しして救助。
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


## 設定と切断時の確認

- 定員は `Config/DefaultGame.ini` の `[/Script/Engine.GameSession] MaxPlayers` を参照します。変更後は再起動してください。
- 救助の加速割合は `TBBox` の `AdditionalRescuerBonus` で調整できます。自動生成する箱を調整する場合は `TBBox` のBlueprintを作成し、使用するGameModeの `BoxClass` に指定します。
- 試合開始後は部屋の募集・招待を閉じます。開始前に取得した検索結果からの参加もサーバーが拒否します。
- 非ホストが退出しても両陣営が残っていれば継続します。最後の人間が退出するとおもちゃ勝利、最後のおもちゃが退出すると人間勝利です。結果後も部屋は自動解散しません。
- ホストが終了すると参加者は通信失敗後に退出します。ホスト移譲・途中参加・再接続には対応していません。
- `r.DefaultFeature.AutoExposure=False` は、Arenaで視線や遮蔽物による凍結を確認するとき、視点方向による明るさの変化を抑えるための設定です。全マップに適用されるため、本番マップの照明調整時に見直します。

実機での確認は [受入テスト](ACCEPTANCE_TESTS.md) に沿って行ってください。
