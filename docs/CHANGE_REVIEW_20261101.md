# 2026-11-01 変更・再レビュー記録

## 対象と比較基準

- 親リポジトリ: `feature/20261028_addAnimation`、比較元 `c9454682b059405707131945d50a88fe7cd9a3aa`。
- Content: 比較元 `2c284a8b653f661d555e699d79066a9b6ff8c0a5`。アセットのコミットは `cfe7f758ee5d67fb180e16750b545137450e578a`。ユーザー指定によりmainで管理し、一時的に作成した専用ブランチは削除済み。
- 今回の依頼分と、同じ作業ツリーにあったherdrのアニメーション・リモコン・接触仕掛けの変更を含む。
- 全ファイル一覧は `CHANGE_FILES_20261101.txt`。Mは既存変更、Aは新規追加。
- mainのPR #43・#44は比較元に取り込み済み。Contentの競合ではローカルのマップ調整を保持し、mainのランダム出現地点（House 8個、Demo 5個）を復元した。
- PR #45は確認のみ。取り込みもPR作成も行わない。

## 変更内容と確認する場所

| 機能 | 内容 | 主なコード・アセット |
| --- | --- | --- |
| 通常アニメーション | 人間の視線・首・上体の追従、持ち姿勢と手放しの補間、歩行速度に応じたおもちゃの動作。視線とエモートの状態をサーバー基準で同期し、古いラウンド・所持キャラクターの状態を破棄 | Character/TBCharacter、TBHumanPose、TBToyPresentation、TBToyCarVisual、TBToyFacingPose |
| エモート | 1=手振り、2=喜ぶ、3=ダンス。メニューにも表示。隠し0キーは6秒の激しいダンス。保存済みの0キー割り当てを優先。静止・接地・行動可能などを確認し、移動・捕獲・メニュー等と整合させる | TBCharacter、TBToyPresentation、Input、UI、Content/Input |
| エモートの実装方式 | 新規ダンス動画ではなく、腕・頭・胴体・脚のボーン回転を時間で計算。車は揺れで表現。Actorや当たり判定の位置は踊らせない | TBHumanPose、TBToyPresentation、TBToyCarVisual |
| キー変更 | 競合配列を移動する前に判定結果を確定し、C++の引数評価順序で競合を見逃す問題を修正 | Input/TBKeyRemap.cpp |
| しまう操作 | 箱への視線が45度の範囲から外れると進捗をリセット。持っているおもちゃは保持。NPCも箱を見るよう修正 | GamePlay/TBBox.cpp、Character/TBNPCController.cpp |
| 複製したドアと試合開始 | 代表入口を安定して選び、各ドアの共有スロットを検証。ドアの見た目・動きは維持。開始地点の安全判定とおもちゃNPCの別地点への再試行、開始失敗の診断を追加 | TBHumanEntrance、Core/TBGameMode、TBLobbySettings |
| Houseの通行 | 冷蔵庫・椅子等27コンポーネントの衝突を専用12メッシュで修正。上に乗れることと椅子の下の隙間を両立。収納部屋のワールド配置も補正 | Content/HouseCollision、Maps/TB_StylizedHouse |
| 動かせる小物 | House 57個・Demo 150個。単純衝突、CCD、質量・減衰、移動複製を設定。接触時の押す力はサーバー側で制限 | Character/TBMovement、Content/PushableProps、対象2マップ |
| お皿の落下 | 支持面の衝突を専用メッシュで調整し、皿・ボウルの形状・姿勢・重なり・物理反復数を補正。果物6個は衝突なしでボウルへ取り付け | Content/PropSupports、PushableProps、対象2マップ |
| おもちゃ箱の装飾 | ABC積み木、サイコロ、糸巻き、太鼓、列車、ボール、旗など224装飾。既存アスレチックの足場と最終足場タグは維持し、本の見た目を玩具風に変更。装飾は衝突なし | Content/Maps/TB_ToyBoxRoom、ToyBoxPlay |
| 箱からの脱出 | 高さだけでなく最終足場への着地・範囲・床法線・捕獲状態を確認して脱出。別ラウンドの状態を無効化 | GamePlay/TBBoxEscapeCourse |
| TVリモコン | 正しいキーの上で0.2秒静止して確定。端・同時に複数人・移動中の誤作動を抑止。離れた後に再入力可能。表示プレビューとサーバー確定を分離 | GamePlay/TBTVRemote |
| 接触する仕掛け | ラジオなどの限定回転用コンポーネント。サーバーが接触・周囲の衝突・再作動条件を検証し、姿勢を同期 | TBContactReactiveComponent、Content/ContactReactiveProps、CONTACT_REACTIVE_PROPS.md |

## 再レビューと検証

2026-11-01の最終レビューで追加修正が必要な問題は見つからなかった。先行作業で見つけたキー競合判定、皿と果物の衝突、BoxEscapeテストの開始位置残留は修正済み。

- Editorビルド成功: `Saved/ToyBoxPolish/BuildVerified.log`。
- 最終再実行: CarryRelease、Emotes、HumanLook、ToyGait、ChannelLifecycle、RemoteSelection、BoundedMotion、RegisteredSurviveGC、RemapApply/Check/Reset/Swap/Works、HumanEntranceの計14件成功。
- 結果: `Saved/FinalReview20261101/{Animation,Remote,Editor,Input,Entrance}/index.json`。
- HumanEntranceの警告2件は、安全でない床・頭上空間とNPC配置失敗を意図的に発生させる負のケース。テスト自体は成功。
- Editor専用2件は最初のゲームモード実行では対象外だったため、Editorモードで実行し直して成功を確認した。
- 先行検証: House/DemoのAuthoredRandomSpawns、AuthoredMatchStart、SharedToyBox、DishStability、HouseInteractions、ArenaのMomArrival/MomAsset/RandomSpawns、BoxEscape、StorageHold、NPC MatchLifecycleが成功。詳細は `Saved/ToyBoxPolish/Verification.txt` と `Saved/HouseInteractionFix`。
- BoxEscapeテストは同期的に連続するラウンド間で参加者が開始地点を塞いでいたため、テスト内で次ラウンド前に待機位置へ移すよう修正した。
- 親・Contentとも `git diff --check` 成功、未解決の競合なし。

Saved以下のログはローカル診断用でコミット対象外。過去の失敗ログも残っているため、上記の最終成功レポートを参照する。

## PR #45との互換性確認

確認時のheadは `12d402499dcb476c1142fb3aca5e5539bad5e212`。タイトル演出関連の親8パスとContentの変更を比較し、作業中の変更ファイルとの重複はなかった（Contentポインタは別途更新が必要）。コード差分の `git apply --check` は成功。タイトル専用モード・コントローラーと今回のゲーム中入力も分離されている。PR #45を取り込んだ状態のビルド・実行までは確認していない。

## 残る確認と公開手順

- 実機間のオンライン対戦による物理同期と、アスレチック全区間の手動操作は未確認。見た目はオフスクリーン描画で確認した。
- 新旧の変更を保持した退避stash `codex-before-main-c945468-20261101` は親・Content両方に残す。
- 親とContentをコミット後、本人のquizを待つ。両リポジトリにpushフックがあり、それぞれ送信するHEADに一致する合格記録が必要。記録の代筆・フック迂回は行わない。
- 合格後はContentを先にpushし、その後でContentコミットを参照する親ブランチをpushする。PRは作成しない。

## 追記: PR #45のmain反映後の統合

上記は最初のレビュー時点の記録。以降、親 `391bc73` とContent/main `cfe7f75` はそれぞれ本人quizに合格してpush済み。その後のユーザー依頼で、親main `5be5ab5a3515ca83f71a66df8f383dbed91a54dd`（PR #45）を作業ブランチへ取り込んだ。

- Contentはmain上で、既存 `cfe7f75` とタイトル側 `b4d4c61` をマージ。統合コミットは `e26c875`。タイトル寸劇・ループのシーケンス、素材、タイトルステージが追加された。
- 親の競合はContent参照のみ。両履歴を含む統合コミットへ解決。C++のテキスト競合はなかった。
- タイトルの寸劇再生・スキップ・再視聴・放置時再生、タイトルカメラ選択、入室失敗時の残存セッション削除とメニュー復帰を取り込んだ。
- ビルドで名前隠蔽エラーが発生したため、TBHUD/TBPaperHUDの `Body` を `EmoteCharacter`、TBTitleMenuの `Slot` / `Cursor` を `BlackoutSlot` / `CursorPosition` に変更。動作は変更しない。
- 統合開始時から未コミットだったHouse/Demoの2マップはコミット対象に加えず保持。バックアップは `Saved/MainIntegration20261101/*.before.umap`、統合後とハッシュ一致を確認。
- タイトル往復テストは「開始」を押した瞬間に試合開始済みと判定し、再試合でも同じマップに留まる想定だったため、ステージ移動・専用ロビーへの復帰・再試合開始それぞれの完了を待つよう修正。移動で作り直される2人目のコントローラーもLocalPlayerから再取得する。プレイヤーとセッションの保持を検証し、ゲーム本体の開始処理は変更しない。
- ビルド成功: `Saved/MainIntegration20261101/BuildFinal.log`。
- 実タイトルマップ・寸劇・入室失敗の後始末・タイトル往復・既存エモートの計5テスト成功。最終レポートは `Saved/MainIntegration20261101/Acceptance/index.json`（4件）と `Emotes/index.json`（1件）。旧RoundTrip失敗は現在のマップ移動仕様にテストを合わせた後の成功で置き換える。
- RoundTripではタイトルステージ2のStaticMeshActor_368/369/370/373/374に「Scale3D is (nearly) zero」の物理初期化警告が計10件出た。タイトル演出アセットは今回変更せず、手動で見た目を確認する際の注意点として残す。
- 今回の統合でHEADが変わるため、以前のquiz合格記録は新しいコミットのpushには使えない。PRは作成しない。

## 追記: 子どもがCubeになる問題のGC対策

- `BeginPlay`で`ChildMesh`/`ChildAnimation`をロードしても、ソフト参照はGCから保護しない。PlayerState到着前やおもちゃ表示中にはSkeletalMeshComponentも子どものメッシュを保持しないため、後の`Get()`がnullになる経路があった。
- `TBCharacter.h`にTransientの強参照を追加し、メッシュとアニメーションクラスをキャラクターの生存期間中保持する。`UpdateAppearance`も保持した参照を使用し、BeginPlay前のPossession経由では必要なアセットをロードして保持する。
- `TBHumanAppearanceGCTests.cpp`を追加。独立した一時メッシュを使い、PlayerState未到着→GC→人間表示、おもちゃ表示→GC→人間への復帰を検証する。保持しない対照メッシュはGCで消えることも確認し、他のキャラクターやEditorの参照で偶然成功するのを避ける。
- Editorビルド成功。HumanAssetsSurviveGC・RandomRound・Emotesが成功（`Saved/HumanAppearanceGC/Tests/index.json`）。
- Steam対応Developmentパッケージのビルドも成功（`Saved/HumanAppearanceGC/Package.log`）。パッケージexeをNull接続の検証モードで実行し、同じ3テストがエラー・警告0で成功。保持しない対照メッシュのGC回収も確認した（`Saved/HumanAppearanceGC/PackagedTests/index.json`）。
- 修正版は `Builds/HumanAppearanceGC/Windows/ToonStory.exe`。起動中の既存 `Builds/Windows` パッケージは上書きしていないため、修正版で遊ぶ場合はこちらを起動する。
- 実際のプレイ中のGCタイミングを捕捉したものではないが、コード上の寿命問題を修正し、強制GCで検証する。見た目・動き・マップアセットはこの修正では変更しない。

## 追記: ロビー移動・エモート、鏡と母親イベントの設定（2026-11-02）

- ロビーで常に入力を止め、固定カメラへ切り替えていた処理を変更。設定・ステージ選択・オプション・プレイヤー一覧・チャット操作中とステージ移動中だけ移動を止める。設定を閉じるとWASDとマウスで探索でき、Escで設定を再表示する。チャットを開く際の入力モード変更は入力欄へフォーカスする前に行う。
- 陣営未確定のロビーでも子どもの見た目でエモートを使えるようにした。標準キーは1・2・3、既存の隠し0も同じ制限で使用可能。設定・チャット操作中はキー入力からエモートを発火しない。試合中の捕獲・収納などの行動条件は変更していない。
- エモートの同期スナップショットへフェーズを追加し、試合開始・ステージ移動でロビーのエモートを持ち越さない。BuildTagを `ToonStory_LobbyEmotes_20261102` へ更新し、旧同期形式の版を検索・参加・招待時に拒否する。全員が同じ新しい配布物を使うこと。
- Houseの既存鏡 `Mirror_Hallway` はX=456から446へ10cm戻し、既存家具に鏡面が埋まる状態を修正。Demo寝室の西壁に `Mirror_Bedroom` を追加（座標 `-805,50,151`、Yaw=0、Scale=1）。家具より上の壁に置き、鏡面の中央・上下・左右の遮蔽と正面に立てる床を確認した。既存家具は移動・削除していない。
- 母親イベントの共有DBは既に有効・イベント候補登録済みだった。不足していた地点をHouse `(-300,-650,20.95)`、Demo `(-450,-175,-7)` に各1つ配置し、床と人間の入れる範囲を確認した。赤い境界線用素材も設定済み。母親本体のモデルは既存仕様の仮キューブのまま。
- Contentはmainのまま。以前から未コミットだったHouse/Demoの内容を保持し、その上へ依頼された配置を追加した。作業前バックアップは `Saved/MirrorMomLobby/*.before.umap`。今回の変更は未コミット・未push。
- Editorビルド成功: `Saved/MirrorMomLobby/BuildAcceptance.log`。ロビーで実際の移動入力による位置変化・エモートの骨姿勢生成・試合移行時の失効・設定/チャット開閉後の入力復帰を検証。GC対策、既存エモート、旧版拒否も成功: `Acceptance/index.json`（4件）。チャットの既存4テストは `Lobby/index.json` に成功記録あり。
- Houseの母親イベント開始/説得進捗、鏡の配置・反射判定・素材を確認: `House/index.json`（7件）。Demoの最終配置も成功: `DemoFinal/index.json`（2件）。タイトル→ロビー→試合→ロビーの往復も成功: `Title/index.json`（1件）。タイトルステージ2の既存Scale3D警告10件は前回と同じもの。実機間Steam通信と手動プレイでの見た目確認は別途必要。
- Steam対応Developmentパッケージも成功: `Saved/MirrorMomLobby/Package.log`。修正版exeは `Builds/MirrorMomLobby/Windows/ToonStory.exe`。子どもメッシュのGC対策も含む。既存の配布フォルダーは上書きしていない。
- 完成したexeをNull接続の自動テストモードで実行し、ロビー移動・エモート・GC対策・旧版拒否（`PackagedLobby/index.json`、4件）、House配置（`PackagedHouse/index.json`、2件）、Demo配置（`PackagedDemo/index.json`、2件）がすべて成功。8件ともエラー・警告0。Steam実機間接続を検証したという意味ではない。

## 追記: コミット前レビュー（2026-11-02）

- 差分のコード、呼び出し元、入力モードの切り替え、GC保持、エモート同期と旧版拒否を再確認。今回の範囲で追加修正が必要な不具合は見つからなかった。
- 同じコード・アセットで成功済みのEditorビルドとパッケージ版8テストを確認。レビュー中は実装を変更していないため、同一テストの再実行は省略した。Steam実機間通信は未検証。
- ユーザーの明示承認により、以前からの編集を含むHouse/Demoの2マップもContent/mainへコミット・pushする。親は現在のfeature/20261028_addAnimationへコミットし、Steamモードの設定を含める。PRは作成しない。上記の「未コミット・未push」は作業完了直後の状態を示す。
