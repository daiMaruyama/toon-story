# 試遊手順

ルールの数値は Notion の仕様を参照してください。このページのコマンド引数は動作確認用の例です。
制限時間切れ・全員収納で人間勝利、時間内の必要アイテム収集でおもちゃ勝利となります。
箱の救助は外側の操作位置で行い、完了すると仲間が箱外へ移動します。

## ビルド

```powershell
.\Scripts\BuildEditor.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
.\Scripts\PackageWindows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

## マップ

| マップ | 用途 |
| --- | --- |
| `TB_KidsRoom` | 本番ステージ。子ども部屋2室と、廊下の先の箱の部屋 |
| `TB_ArchViz` | 本番ステージ。アパート2戸と、通路の先の箱の部屋 |
| `Arena` | 検証用。床・壁・箱・アイテム・開始位置をPlay時にC++が生成する |

部屋を作る（Host room / `TBHost`）と、ホストが本番ステージ2つから1つを選び、参加者も同じマップへ入ります。
PIEは部屋を作らずエディタで開いているマップをそのまま使うため、本番ステージを試すときは `TB_KidsRoom` か `TB_ArchViz` を開いてからPlayします。
マップに箱（TBBox）がなく、GameModeの `bBuildTestArena` が有効な場合だけ、検証用の床・箱・アイテムが生成されます。

本番ステージは保存済みの `.umap` をUEエディタで直接編集します。家具・壁・ドア・照明・ロビーカメラは配置済みのActorです。
エディタ起動時は `TB_KidsRoom` を開きます。別ステージはContent BrowserのMapsから開いてください。
編集方法と維持する配置条件は [ステージ編集](STAGE_EDITING.md) を参照してください。Pythonによる生成・実行手順はありません。

## UE側で最初に用意するもの

`Content/Maps/Arena.umap` は作成済みです。Content BrowserのMapsからArenaを開いて、そのまま試遊できます。エディタ上では照明と空が表示され、床・壁・箱などはPlay後に生成されます。以下は作り直す場合の手順です。

エディタで作業を保存して終了し、`Scripts/BuildEditor.ps1`で通常ビルドしてください。新規クラス・削除クラス・衝突チャンネル・プラグインの変更があるため、再起動が必要です。

現在のContentには `ThirdPerson/Lvl_ThirdPerson`、`SandBox/Lvl_ChildRoom`、`Tests/MatchLoop/Lvl_MatchLoop` がありますが、既存レベルのGameMode Overrideや配置物は未検証です。最初は以下の空マップが確実です。

1. File > New Level > Empty Levelを作る。既存レベルの未保存作業は先に保存する。
2. Directional LightとSky Lightを1つずつ配置し、MobilityをMovableにする。Sky Lightは必要に応じてRecaptureする。
3. World SettingsのGameMode Overrideを `TBGameMode` にする。使用するPawnは `TBCharacter`。
4. `/Game/Maps/Arena`、つまり `Content/Maps/Arena.umap` に保存する。
5. ArenaのWorld SettingsでGameModeがTBGameModeであることを確認する。ゲーム起動時の既定マップはTB_KidsRoomなので、Arenaの検証では明示的にArenaを開く。

Arenaの床・壁・箱・検証用アイテム・定員分の開始位置はC++が生成します。床やPlayerStartを追加する必要はありません。キャラクターは設定された子ども／ぬいぐるみを使い、素材がない場合だけCubeで代替します。警報音は未設定なので、表示のみでテストできます。

Contentはサブモジュールです。マップや素材はContent側で管理し、その後に親リポジトリの参照を更新してください。

## 1台でルールを確認する（Steamを使わないPIE）

エディタを終了し、プロジェクトルートのPowerShellで実行します。

```powershell
.\Scripts\SetOnlineMode.ps1 -Mode LAN
```

エディタを再起動して `TB_KidsRoom` または `TB_ArchViz` を開き、Playの設定をNumber of Players=3、Net Mode=Play As Listen Server、New Editor Windowにします。単純な地形でルールを確認する場合はArenaを使います。PIEの接続を使うため、この段階ではTBHost/TBFindを呼びません。

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
- 結果画面でホストが「Return together to lobby」を押すと、接続した全員が同じロビーに戻ります。コンソールではホストの `TBLobby` も使えます。
- 戻った後は全員のReadyが解除されます。役割希望・人数設定・ステージは保持されるので、必要なら変更して全員Ready→ホストStartで再戦します。
- 結果画面の「Leave room」は自分だけ退出します。ホストの「Close room」はサーバーを終了するため、再戦したい場合は押さないでください。

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
8. 再戦はホストの「Return together to lobby」または `TBLobby`。全員同じ接続のままロビーに戻り、Readyして再開始する。部屋を退出・終了したい場合だけ `TBLeave` を使う。

480は共有IDなので、部屋一覧の所有者名を確認してください。コードは独自の識別子で結果を絞りますが、部屋検索・Steam招待・異なる回線での接続は実機検証が必要です。

接続できない場合は `-log` を付けて起動し、`LogOnline`・`STEAM`・`SteamSockets`と `[ToyBoxSession]` を確認します。Steamログイン、同じビルド/モード、対象マップの保存とCook、Windows Firewallの許可を確認してください。LANに切り替えた場合は再パッケージが必要です。


## 設定と切断時の確認

- 定員は `Config/DefaultGame.ini` の `[/Script/Engine.GameSession] MaxPlayers` を参照します。変更後は再起動してください。
- 救助の加速割合は `TBBox` の `AdditionalRescuerBonus` で調整できます。自動生成する箱を調整する場合は `TBBox` のBlueprintを作成し、使用するGameModeの `BoxClass` に指定します。
- 試合開始後は部屋の募集・招待を閉じます。開始前に取得した検索結果からの参加もサーバーが拒否します。
- 非ホストが退出しても両陣営が残っていれば継続します。最後の人間が退出するとおもちゃ勝利、最後のおもちゃが退出すると人間勝利です。結果後も部屋は自動解散しません。
- ホストが終了すると参加者は通信失敗後に退出します。ホスト移譲・途中参加・再接続には対応していません。
- 同じ部屋での再戦は、通常の試合終了・中断結果からホストが実行できます。収集物、収納・運搬・凍結、取得進捗、救助・警報、勝敗・時間をリセットします。ロビー復帰後は部屋検索・招待による参加も再開します。
- `r.DefaultFeature.AutoExposure=False` は、Arenaで視線や遮蔽物による凍結を確認するとき、視点方向による明るさの変化を抑えるための設定です。全マップに適用されるため、本番マップの照明調整時に見直します。

実機での確認は [受入テスト](ACCEPTANCE_TESTS.md) に沿って行ってください。

## 再戦処理の確認記録（2026-10-04）

- `ToonStory.Match.Rematch`：両ステージで3回連続の開始→終了→ロビー復帰を確認。収集済み・収納・凍結・運搬と中断結果を含む。
- 同一PCのLANホスト／クライアントで、再接続なしのロビー復帰と2試合目の開始・終了を確認。
- 参加側の既定DirectX 12起動では再戦操作前のロビー読み込み中にRendererのアクセス違反が1回発生。参加側だけ `-dx11` を付けた検証では完了。プロジェクトの描画設定は変更していない。
- 別PC間のSteam接続、再戦ロビーの第三者による検索・招待参加は未検証。
