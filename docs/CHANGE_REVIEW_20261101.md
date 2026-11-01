# 2026-11-01 変更・再レビュー記録

## 対象と比較基準

- 親リポジトリ: `feature/20261028_addAnimation`、比較元 `c9454682b059405707131945d50a88fe7cd9a3aa`。
- Content: 比較元 `2c284a8b653f661d555e699d79066a9b6ff8c0a5`。アセットのコミットは `cfe7f758ee5d67fb180e16750b545137450e578a`、ブランチは `codex/20261101-animation-props-assets`。
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
