> 2026-09-30: この文書のゲーム仕様・導入手順は旧版です。扉処理は削除済み、時間切れは人間勝利、おもちゃ同士の衝突あり、Steam 480設定済みです。現在の手順は [PLAYTEST.md](PLAYTEST.md) を参照してください。

> この資料はZIP同梱文書を移植用にパス調整したものです。元の検証環境・未検証という記述は配布時点の記録です。このブランチの変更・検証結果は [INTEGRATION.md](INTEGRATION.md) を参照してください。コマンドはリポジトリのルートで実行します。

# おもちゃ箱：UE5.8 制作PC向け実装パック v2

更新：2026-09-20 / Windows / C++ / Steam・Null / リッスンサーバー / 最大8人

## 最初に読むこと

本パックは、制作環境を整えたPCで開発を続けるための**コードと手順の引き継ぎ資料**です。UE本体・完成したゲーム・ビルド済みEXE・モデル・音源・umap/uassetは含みません。C++のソースは省略せず同梱しています。

仕様の根拠は、添付の `toybox-multiplayer-spec.pdf` と、その後の会話で確定した変更です。**空中落下、即時解除、手動での扉閉鎖、接触3秒による収集は、旧PDF・旧版ガイドより本版を優先**してください。ネットワーク資料内の実装手順は参考として扱っています。

この作業環境にはUE5.8、UHT、Windows SDK、Steamクライアントがありません。**UEでのコンパイル・エディタ起動・実接続は未検証**です。移植先で行う検証を `docs/toybox-starter/ACCEPTANCE_TESTS.md` にまとめています。UE非依存のルール関数はC++でコンパイルし、実行テストを通過しています。これをUEのビルド成功と混同しないでください。

### 制作PCでの最短ルート

1. ZIPを展開する。
2. C++をビルドする。
3. エディタでArenaマップと照明を作成する。
4. Nullの2人PIEで凍結・捕獲を確認する。
5. 3人以上で救助・扉開閉・自力脱出を確認する。
6. アイテム取得の停止／再開を確認する。
7. Steamを有効にして別PC・別回線で接続する。
8. モデル・UI・アニメーションを置き換え、妨害などを追加する。

別のAIや開発者へ引き継ぐ場合は、`docs/toybox-starter/PC_HANDOFF.md`とソース一式を渡してください。

## 1. 確定仕様

| 項目 | 動作 |
|---|---|
| 視点 | 人間・おもちゃとも一人称 |
| 視線凍結 | いずれかの人間から見られているおもちゃは、自分で移動・ジャンプ・視点操作できない |
| 空中で凍結 | 横方向を停止。上昇中なら上昇速度を0にし、そこから落下。落下中なら下降速度を維持 |
| 凍結解除 | 全員の視線から外れた次のサーバー評価で解除。追加の0.25秒待機はなし |
| 掴み | 凍結・移動中の両方が対象。距離と遮蔽をサーバーで確認 |
| 運搬 | おもちゃに自力脱出操作はない |
| 収納 | 人間が箱の操作位置で長押し。扉が閉じているときだけ可能 |
| 救助 | 基本10秒。複数人で加速。救助者が0になると進捗0 |
| 救助完了 | 扉が開く。箱内のおもちゃは自分で出口を通って逃げる |
| 扉の開放 | 自動で閉じない。人間が閉鎖操作を行うまで開放 |
| 閉鎖操作 | 人間が3秒長押し。視点を操作箇所に固定。途中中断で進捗0 |
| 閉鎖中の視線 | 固定された視点から実際に見えるおもちゃは凍結する。自動的に人間の視線判定を無効にはしない |
| 逃走中 | 箱から出る途中でも見られれば凍結する |
| アイテム | 接触を3秒維持すると自動取得。取得ボタンは不要 |
| 接触解除 | 取得進捗を0に戻す。凍結中に接触を失った場合も0 |
| 取得中の凍結 | 接触が続いていれば進捗を保持して停止。解除後に続きから再開 |
| 妨害 | 追加・変更しやすい入口を用意。初期状態は無効 |

サーバー判定と通信はフレーム単位なので、「即解除」は実時間の遅延ゼロではありません。意図的な解除待機を入れないという意味です。現時点ではサーバーのGameModeで毎Tick視線を評価します。

## 2. 調整可能な仮設定と境界処理

以下は確定仕様を動作させるための仮設定です。制作PCで変更できます。

| 項目 | 初期値・仮処理 |
|---|---|
| 人数 | 人間1＋おもちゃ4。ロビーで変更、合計8以下 |
| 開始 | 指定人数がそろい、全員Ready、ホストが開始 |
| 陣営 | 希望優先。競合時は抽選 |
| 制限時間 | 600秒 |
| アイテム | 8個配置・5個取得で勝利。チーム共有数 |
| 複数人が同じアイテムに接触 | 各自の進捗を独立計算。最初に完了した1人だけ取得 |
| 複数アイテムに接触 | 現在の対象を維持。対象がなくなったら最も近い対象へ。新対象は0から |
| 掴み距離 | 180cm、正面、遮蔽なし |
| 掴み演出時間 | 0.25秒。これは凍結解除待機とは別 |
| 収納時間 | 3秒。閉鎖時間とは別設定 |
| 収納中断 | ボタンを離す／範囲外／救助で扉が開くと落とす |
| 閉鎖中の移動 | 人間も移動停止。接地中のみ開始できる |
| 運搬中の閉鎖 | おもちゃを持ったまま閉鎖可能。閉鎖後にEを押し直して収納 |
| 閉鎖の担当 | 同時に人間1人。途中中断後は別の人間が開始可能 |
| 閉鎖位置にキャラがいる | 100%で扉を開いたまま保留。キャラがどくと閉まる。キーを離せば中断 |
| 閉鎖時に箱内へ入り直したおもちゃ | Freeでも箱内ならBoxedへ戻す |
| 開放中の箱内で掴む | Boxedの対象も掴める。逃走開始前にも再捕獲可能 |
| 全員収納の勝利 | 全員Boxedかつ扉が閉じている場合のみ。開放中は未確定 |
| Pawn同士の衝突 | プロトタイプでは無効。床・壁とは衝突 |
| カメラ | 水平FOV90度・16:9固定。判定は水平半角40度、垂直は画角より5度内側 |
| 地形 | 静止した床・壁を前提。移動床、ラグドール、Root Motion未対応 |

出口判定と扉の占有判定は同梱の箱形状に合わせています。箱はScale=(1,1,1)、水平設置で使ってください。箱の大きさをスケールで変更する場合は判定も修正します。閉鎖妨害・押し出し・閉鎖保留のバランスは実機で調整する部分です。

## 3. ファイル構成

| ファイル | 役割 |
|---|---|
| `Source/ToonStory/TBGame.h/.cpp` | 状態、移動、捕獲、扉、救助、収集、勝敗、簡易HUD |
| `Source/ToonStory/TBRuleMath.h` | UE非依存のルール関数。ゲーム本体とテストが共用 |
| `Source/ToonStory/TBSession.h/.cpp` | OSSの部屋作成・検索・参加・退出・起動中の招待受理 |
| `Source/ToonStory/ToonStory.Build.cs` | モジュール依存 |
| `Source/ToonStory.Target.cs` / `ToonStoryEditor.Target.cs` | ゲーム／エディタのビルドターゲット |
| `Config/` | Nullの初期設定、入力、パッケージするマップ |
| `Profiles/Steam/` | Steamに切り替える設定 |
| `Scripts/BuildEditor.ps1` | 制作PCでエディタターゲットをビルドする補助 |
| `Scripts/PackageWindows.ps1` | 制作PCでDevelopment版をパッケージする補助 |
| `Tests/RuleTests.cpp` | 実際のルール関数を呼ぶ小さなC++テスト |
| `docs/toybox-starter/PC_HANDOFF.md` | PCでの作業再開・既存プロジェクトへの移植・変更箇所 |
| `docs/toybox-starter/ACCEPTANCE_TESTS.md` | UE実機での受入テスト |
| `docs/toybox-starter/VALIDATION.md` | 実施した検証／未実施の検証 |

## 4. 制作PCへの導入

### 4-1. 環境を用意する

UE5.8に対応したVisual StudioとWindows SDKを用意します。C++のゲーム開発・デスクトップ開発ワークロードを追加してください。UEが要求するツールチェーンはインストールした版のUBT出力と[公式セットアップ](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine)を基準にします。

### 4-2. ビルドする

1. ZIPを例`C:\UnrealProjects\ToyBoxStarter`へ展開。
2. `ToonStory.uproject`を右クリックし、UE5.8への関連付けとVisual Studioプロジェクト生成を行う。
3. ソリューションを開き、`Development Editor` / `Win64`でToonStoryをビルド。
4. 成功後にuprojectを開く。

補助スクリプトを使う場合は、プロジェクトフォルダーでPowerShellから実行します。EngineRootは自分のPCの実際のUEフォルダーへ変更します。

```powershell
.\Scripts\BuildEditor.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

PCのポリシーでスクリプトが実行できない場合は、Visual Studioから同じビルドを行ってください。スクリプト自体もこの環境ではWindows上で実行していません。

初回やUCLASS変更後はエディタを閉じてビルドしてください。Live Codingだけでクラス構造を更新しないでください。Target.csのビルド設定で版差エラーが出た場合は、手元のUE5.8で新規生成した空のC++プロジェクトの設定と比較します。

### 4-3. Arenaを作る

1. 空のレベルを新規作成し、`Content/Maps/Arena`へ保存する。
2. Directional LightとSky Lightを配置。必要ならSky Atmosphereを追加。
3. World SettingsのGameMode OverrideをNoneまたはTBGameModeにする。
4. Maps & Modesで既定マップArena・既定GameMode TBGameModeを確認。
5. 保存してPlay。

床・遮蔽物・箱・アイテム8個・PlayerStartはC++が生成します。生成マップは60m四方です。箱は約(1300,1000,0)、操作マーカーは約(1180,1220,20)、出口は箱の西側中央です。箱内には照明があります。

## 5. PIEで試合を確認する

### 2人：凍結・捕獲・収納

1. Play設定を2人、Play As Listen Server、New Editor Windowにする。
2. ホスト側のF10コンソールで`TBRules 1 1 600 5`。
3. 両方のウィンドウでR、ホストでF5。
4. 人間でおもちゃを見る。おもちゃの移動・視点が止まる。
5. おもちゃがジャンプ中に見られた場合、上昇と横移動が止まり、落下する。
6. 人間が視線を外すと、追加待機なしで操作できる。
7. 人間が近づいてE。掴んだ後は箱へ運ぶ。
8. 扉が閉まった箱の操作マーカーでEを3秒長押し。
9. 2人では唯一のおもちゃが収納されるので、人間勝利になる。

### 3人以上：救助・扉・逃走

1. 3人なら`TBRules 1 2 600 5`、4人なら`TBRules 1 3 600 5`。
2. おもちゃを1人だけ収納する。
3. 自由なおもちゃが箱のマーカーでEを10秒長押し。複数人なら短縮。
4. 扉が上がり、通り抜け可能になる。
5. 箱内のおもちゃを操作して西側出口を通る。全身が外へ出るとFreeになる。
6. 人間で操作マーカーへ行きEを長押し。視点が操作箇所へ固定される。
7. 3秒で扉が閉じる。閉鎖後は一度Eを離して、次の収納操作を行う。
8. 閉鎖途中でEを離し、進捗が0・扉が開いたままになることも確認する。

人間の固定視点から出口が見えすぎる場合、マーカーと立ち位置を調整します。視点固定は「その間すべてのおもちゃが無条件で動ける」という処理ではありません。実際に視線が外れたおもちゃだけ動けます。

### アイテム

1. 自由なおもちゃで球へ近づき、接触状態を3秒維持する。Eは押さない。
2. HUDのItem進捗が100%になり、球が消え、共有収集数が1増える。
3. 途中で離れると0になる。
4. 接触したまま人間に見られるとPAUSEDになり、進捗が止まる。
5. 視線が外れると続きから進む。

| 入力 | 動作 |
|---|---|
| WASD・マウス・Space | 移動・視点・ジャンプ |
| E | 掴み／救助・収納・扉閉鎖の長押し |
| Q | 運搬中のおもちゃを落とす。閉鎖中は不可 |
| R / F5 | Ready / ホスト開始 |
| F10 | コンソール |
| `TBRules H T 秒 必要数` | ホストがルール変更。Readyを解除 |
| `TBPrefer 1 / 2 / 0` | 人間希望／おもちゃ希望／希望なし |
| `TBHost` / `TBFind` / `TBJoin 0` / `TBLeave` | 部屋作成／検索／結果番号で参加／退出 |

PIEが自動接続した状態でTBHostを呼ぶ必要はありません。セッション自体の確認はStandaloneまたはパッケージ版で行います。新しい試合は退出→再ホストで始めます。

## 6. 実装の読み方

### 状態と役割

GameModeが権威判定、GameStateが試合全体の同期、PlayerStateが陣営・凍結・捕獲状態を担当します。Characterは人間／おもちゃ共通で、Teamに応じてカプセル・カメラ・速度を変更します。

### 凍結と落下

`ATBGameMode::EvaluateGaze`が毎サーバーTick、各おもちゃを見ている人間がいるか調べて`SetFrozen(Seen)`します。頭・胴・足の3点とVisibilityトレースを使います。Boxed状態も判定するため、開いた扉越しに見られたおもちゃも止まります。閉じた扉や壁はVisibilityをBlockにします。

`IsMovementLocked`は操作を止める判定、`IsPhysicsLocked`は運搬・閉鎖・試合終了の完全停止を表します。視線凍結では後者を使いません。`UTBMovement::PhysCustom`のカスタム移動モード1で鉛直方向だけを計算します。

```cpp
Velocity.X = 0;
Velocity.Y = 0;
Velocity.Z = TBRuleMath::FrozenVerticalSpeed(Velocity.Z);
```

鉛直速度は正なら0、負なら維持。その後重力を加え、Sweep付き移動で床への衝突を処理します。位置を毎Tick元のXYZへ戻す処理は削除しました。解除時はFallingへ戻し、着地すると通常歩行へ復帰します。

CMCのネットワーク予測経路を使用しますが、独自SavedMoveや先行凍結予測は未実装です。サーバー上の凍結状態で移動入力を拒否します。移動床・Root Motion・物理押し出しへの対応は制作PCで追加する対象です。[公式CharacterMovement](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-networked-movement-in-the-character-movement-component-for-unreal-engine)

### 捕獲・収納

`ServerInteract`は操作要求を受け、サーバーで距離・向き・遮蔽・状態を検証します。Free→Grabbed→Carried→Storing→Boxedと進みます。運搬はCarryAnchorへアタッチし、カプセル衝突を無効化します。収納開始と収納完了の両方で扉状態を検証します。

収納は箱内部の座標へ移す方式です。**救助時はテレポートしません。** ドアが開いた後は自分で歩いて出ます。

### 扉と閉鎖

`OpenDoor`でbDoorOpenを同期し、`OnRep_Door`でパネル位置と衝突を変更します。プロトタイプでは瞬時に上下するパネルです。見た目の補間は制作PCで加えられます。

`BeginClose`で担当人間を1人設定し、LockedViewへ操作箇所方向を保存。CharacterとControllerの両方で視点を固定します。サーバーの視線判定も`EffectiveViewRotation`から同じ固定方向を読むため、古いクライアントの視線でおもちゃを凍結し続ける構成を避けています。

進捗は箱がサーバーで加算し、完了時に出口にキャラがいないことを確認。閉鎖前に脱出を確定し、閉鎖後に箱内のFreeなおもちゃをBoxedへ変更します。全員Boxedでも扉が開いている間は人間勝利になりません。

### 接触収集

アイテムはUSphereComponentの接触領域を持ちます。サーバーで実際のOverlapと遮蔽を調べ、CharacterごとにContactItemとItemProgressを保持します。進捗は所有クライアントだけへ送ります。

```cpp
ItemProgress = TBRuleMath::AdvanceContact(
    ItemProgress, Dt, ContactItem->CollectSeconds,
    true, PlayerState->bFrozen, true);
```

接触を失うと対象と進捗を消し、凍結なら進捗だけ維持します。取得完了のTakeでも対象・進捗・接触・陣営・凍結を再検証します。同時完了はサーバーの処理順で1人だけ取得します。複数人の時間は合算しません。

### 妨害を後から追加する

GameModeの`bAllowInterference`は初期false。Characterの`ApplyInterruption()`が追加の入口です。

1. 妨害方法（攻撃、アイテムなど）を別の処理として作る。
2. サーバーで距離・命中・クールダウンを検証する。
3. 有効な妨害と確定したら、対象CharacterのApplyInterruptionを呼ぶ。
4. 現在は閉鎖・救助を中断し、収納中ならおもちゃを落とす。
5. 変更したい結果はこの関数へまとめる。

Clientから直接妨害成立を指定させません。初版には攻撃入力や攻撃判定はなく、入口を用意した段階です。

## 7. 数値・見た目・UIを調整する

Blueprint派生クラスを作り、Class Defaultsで変更します。

| 場所 | 調整項目 |
|---|---|
| TBCharacterの子BP | HumanSpeed、ToySpeed、CarrySpeed、Body、Camera、CarryAnchor |
| TBBoxの子BP | BaseRescueSeconds、StoreSeconds、CloseSeconds、AlarmSound |
| TBPickupの子BP | CollectSeconds、Contact半径、Mesh |
| TBGameModeの子BP | bBuildTestArena、BoxClass、GazeHalfAngle、bAllowInterference |

箱の音源を付ける場合はTBBoxの子BPでAlarmSoundを設定し、GameModeのBoxClassにそのBPを指定。GameModeもBPにした場合はArenaのGameMode Overrideに設定します。

独自マップではbBuildTestArenaをfalseにし、PlayerStartを8人分、箱を1個、必要数以上のアイテム、床・壁・照明を置きます。自動生成マップのPickupはC++クラスなので、子BPの調整を反映したい場合は手動配置へ切り替えるかBuildArenaの生成クラスを変更してください。

Widgetを付ける際の接続先：

| UI | データ・関数 |
|---|---|
| 部屋作成／検索／参加／退出 | GameInstance → TBSession → Host / Find / Join(Index) / Leave |
| 処理中・検索結果・エラー | TBSession.IsBusy / Rooms / Status、ChangedへBind |
| Ready／希望／ルール／開始 | 自分のTBControllerのServerReady / ServerPreference / ServerRules / ServerStart |
| 残り時間・収集数・勝敗 | TBGameState.Remaining / Collected / Winner / Reason |
| 凍結表示 | 自分のTBPlayerState.bFrozen |
| アイテム進捗 | 自分のTBCharacter.ItemProgress、ContactItem |
| 開閉・閉鎖進捗・救助進捗 | TBGameState.Box → bDoorOpen / CloseProgress / RescueProgress |

UIは同期値を表示し、完了を決めません。メニューを開くときはCharacter.ReleaseInteractを呼んで長押し状態を解除してください。Blueprintで利用する場合、この関数にはBlueprintCallableを付けるか、公開したラッパーを追加します。

## 8. Steam接続

初期設定はNullです。まず同一LANの2台でTBHost→TBFind→TBJoinを確認します。

Steamへ切り替える手順：

1. Online Subsystem SteamとSteam Socketsを有効化し再起動。
2. Config/DefaultEngine.iniのDefaultPlatformServiceをSteamに変更。
3. Profiles/Steam/DefaultEngine.append.iniのSteam設定を統合。
4. Profiles/Steam/Windows/WindowsEngine.iniをConfig/Windows/WindowsEngine.iniへコピー。
5. DevelopmentのWindowsビルドを作り直す。
6. 2台で別々のSteamアカウントへログイン。同じビルドを起動。
7. ホストでTBHost、参加側でTBFind→TBJoin 番号。
8. 別回線で接続確認し、その後人数を増やす。

```powershell
.\Scripts\PackageWindows.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

App ID480は開発用共有IDです。初期化で必要なら実際のゲームEXEの隣にsteam_appid.txtを置き、内容を480にします。自分のゲームとして配信する際は自分のApp IDへ変更し、開発用txtを配信物から除きます。

Steam Socketsと旧SteamNetDriverの設定を混ぜないでください。Nullへ戻す場合、DefaultPlatformServiceだけでなくWindowsEngine.iniのSteamドライバー設定も戻します。[公式Steam Sockets](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-steam-sockets-in-unreal-engine)・[公式Steam OSS](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine)

部屋作成・検索・参加・破棄は非同期で、完了通知を待ちます。Join成功後はGetResolvedConnectString→ClientTravelでホストへ移動します。[公式Session Interface](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-session-interface-in-unreal-engine)

## 9. 現段階の制約

- UE5.8上のビルド・PIE・Steam接続は制作PCで実施が必要。
- マップ／音源／美術／完成版Widget／扉アニメーションは別途制作。
- 凍結の独自SavedMove・視線巻き戻し・クライアント先行凍結は未実装。
- 切断後の状態復元、ホスト移行、試合内リマッチは未実装。
- 接続が維持された短い通信停止はUEに任せる。切断確定後は解散。
- 人間全員離脱はサーバーが生きていればおもちゃ勝利。ホスト消失時は勝敗確定できず解散。
- 試合中の部屋が検索に出る場合があるが、PreLoginで参加拒否。
- 起動中の招待受理は実装。未起動から招待で立ち上げる処理は未実装。
- 非同期サービスの無応答監視・キャンセル・自動回復は未実装。
- Pawn同士の衝突は無効。移動床・傾斜での横滑り・ラグドールは対象外。
- 箱の閉鎖位置判定は同梱の形状専用。改造する場合はDoorwayClearとUpdateEscapesも変更。
- Always Relevantの簡易構成。一般公開時は負荷と隠れた相手の情報保護を検討。

ビルドエラーが出たら、最後の「ビルド失敗」行ではなく最初のC++／UHTエラーから確認します。`docs/toybox-starter/PC_HANDOFF.md`の記録項目を使ってください。
