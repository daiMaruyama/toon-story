# UEエディタでのステージ編集

ステージの正本はContent内の保存済みマップです。PythonやC++による本番ステージの再生成は行いません。
既存の家具・壁・ドア・照明は通常のActorなので、UEエディタで選択・移動・複製・削除できます。
対戦ルール、通信、キャラクター操作、ステージ選択は引き続きC++が担当します。

## 開いて編集する

1. `ToonStory.uproject` を開く。起動時には `/Game/Maps/TB_KidsRoom` が開きます。
2. アパートを編集する場合はContent Browserの `Content/Maps/TB_ArchViz` を開く。
3. Playしていない状態でViewportまたはWorld OutlinerからActorを選び、DetailsのTransformで位置・回転・スケールを変更する。
4. 家具を追加する場合はContent BrowserからStatic MeshをViewportへドラッグする。
5. マップと変更したアセットを保存し、Playで通行・表示・開始地点を確認する。

Play中の一時的な位置変更ではなく、Playを停止した編集状態で配置を保存してください。
既存の試合マップにはTBBoxが配置されているため、C++の検証用Arena生成は実行されません。

各室の家具は用途別に変更しています。KidsRoomは寝室・遊び場・作業部屋・読書スペース、ArchVizはリビング・食事部屋・オフィス・読書ラウンジです。組み替えた家具は `RoomThemes` フォルダに置き、机上の小物やクッションも家具と一緒に移動してください。新しいPythonファイルは不要です。

## World Outlinerで探すもの

| 名前・種類 | 編集する内容 |
| --- | --- |
| `Bedroom_` / `Playroom_` / `Study_` / `Guestroom_` | 子ども部屋ステージの4室の家具・建物 |
| `ApartmentA_` / `ApartmentB_` / `Study_` / `Guestroom_` | アパートステージの4室の家具・建物 |
| `Hall_` / `Corridor_` | 部屋をつなぐ床・壁・天井 |
| `Collision/Walls` フォルダの `Collision_` | 壁の厚みを持つ衝突専用Actor。ゲーム中は非表示。壁を動かす場合は対応する衝突Actorも動かす |
| `Spawn_00` などのPlayerStart | プレイヤーの開始位置。壁・家具との重なりと床の有無を確認 |
| `ToyBox_` のTBBox | おもちゃの収納・救助用の箱。移動時は外側の操作位置への経路も確認 |
| `Collectible_` のTBPickup | 収集アイテム。家具の中や到達不能な位置に置かない |
| `Lobby_Overview` | ロビー背景のCameraActor。位置・回転・画角を編集 |
| `RoomExposure` | PostProcessVolume。露出やブルームを編集 |
| `WarmRoomLight_` | 室内のPointLight。明るさ・色・範囲を編集 |

Actor名は検索用です。カメラの識別には名前ではなく `TBLobbyCamera` のActor Tagを使います。

## 維持する条件

- 家具付きの部屋を4室とし、部屋同士・箱の操作位置への通路を確保する。今回、各室の横幅・奥行きを1.4倍、廊下幅を約3mへ拡張した。
- 扉は開いた回転角度で配置し、MobilityをStaticにする。`DoorFixedOpen` のActor Tagを保持する。
- World SettingsのGameMode Overrideは `TBGameMode`。TBBoxは1つ、収集物は試合の必要数以上（既定5個）。
- PlayerStartは現在8個。定員を変更する場合は `Config/DefaultGame.ini` のMaxPlayersと合わせて見直す。
- `TBLobbyCamera` タグ付きCameraActorは1つ。名前だけ変更してもタグがあれば動作する。
- 床・壁・大きい家具には通行を制限する衝突を設定する。30cm未満の配置小物はCollision EnabledをNo Collisionにする。
- Static Meshアセット自体を編集すると、同じメッシュを使う別の配置にも影響する。配置ごとの変更はActorのDetailsで行う。

子ども・小型ロボットとアニメーションは保存済みです。見た目の参照先は `Config/DefaultGame.ini`、子どものアニメーションは `Content/Characters/ChildAnimation`、ロボットは `Content/Characters/GumBot` で管理します。
ロボットの身長は約25cm、衝突カプセルは半径10cm・高さ26cm、視点はカプセル底から20cmです。歩行速度180cm/秒、左Shift長押しのダッシュ320cm/秒、段差8cm、ジャンプ初速400cm/秒（標準重力で約82cm）に調整しています。待機・歩行・ジャンプをC++で切り替え、凍結時は現在のアニメーション姿勢を止めます。
カメラの近接面は1cmです。衝突専用Actorは `TBStructuralBarrier` タグで両方向のカプセル衝突テストの対象になります。戸口は衝突形状を分割して開けてあります。
ダッシュの入力受付は `SetSprintRequested` にまとめ、現在は左Shiftを直接バインドしています。Enhanced Inputへの移行時は同じ関数をStarted／Completed／Canceledから呼び、直接バインドを置き換えてください。入力状態はCharacterMovementの保存移動に含め、サーバー側もチーム・凍結・運搬状態に応じた速度制限を適用します。

## 確認と保存対象

試遊方法は [PLAYTEST.md](PLAYTEST.md) を参照してください。PIEでは開いているマップを使い、Host roomを押すとC++が既存の2マップから抽選します。
固定のマップ名を変更・追加する場合は、C++の選択対象とConfigの起動マップ・MapsToCookも変更が必要です。

`ToonStory.Stages.RuntimeLayout` は各マップのゲーム実行で開始位置、通行、箱、収集物、小物の衝突、カメラ、キャラクターを検証します。
通行テストは `TBRouteStart0` / `TBRouteEnd0` などのタグを持つTargetPointの間を検証します。間取りを変えた場合はこれらの位置も戸口に合わせて動かしてください。KidsRoomは4経路、ArchVizは向かい合う戸口を結ぶ2経路です。8個の開始地点と固定ドア4枚も検証します。

配置を変えた `.umap` と編集した素材の `.uasset` はContentリポジトリ側で管理します。
`__ExternalActors__` 等はUEに管理させ、手作業で編集しないでください。

## 子どもの色と電池の編集

子どもの色は `Content/child_test_2/Materials/unreal_file` のMaterial Instanceで `Base Color Tint` を編集します。肌はHead／Body／Arm／Legをそろえ、髪はhairとScalpの両方を調整します。歩行判定は `Child_ABP_Unarmed` のEventGraph内のShouldMoveで、GroundSpeedが3cm/秒を超えると有効になります。

電池は `Content/Items/Battery/SM_Battery`、配置はWorld Outlinerの `Gameplay/Batteries` にあります。高さ12cm、Actorの中心から底面まで15cmなので、床に置く場合はActorのZを床面+15cmにします。取得範囲と3秒の取得時間はTBPickupの設定です。ゲーム実行時の共通メッシュ参照はDefaultGame.iniの `[/Script/ToonStory.TBPickup]` で管理します。

## ドアとおもちゃ箱（2026-10-05）

KidsRoomの扉はArchVizの既存ドア素材を使用し、広い開口部に合わせて両開きにしています。World Outlinerの `Architecture/Doors` に枠・扉・取っ手をまとめ、扉は90度開いた位置でStatic固定。`DoorFixedOpen` タグと通行テスト用TargetPointを維持してください。

箱は `Gameplay/ToyChest` のTBBoxです。木箱の正面はActorの-X、幅約160cm・奥行約87cm・閉じた蓋の高さ約103cmです。親コードのTBBoxが木箱の衝突を管理します。収納先は木箱内部ではなく、StorageRoomコンポーネントの別室です。両マップで変更前の別室の位置・形状・収納座標を復元しています。Actorを拡大縮小せず位置・Yawで配置し、箱だけを移動する場合はStorageRoomのワールド位置も確認してください。前面に人間が立つスペースとおもちゃが救助する通路を空けてください。メッシュは `Content/Items/ToyChest/SM_DetailedToyChest`、共通参照はDefaultGame.iniのTBBoxセクションです。収納はE長押し3秒のままです。

## キャラクター変更の統合状況

mainの運搬カメラ変更を取り込み、TBCharacterのモデル・アニメーション・視点・移動処理を統合済みです。コード、DefaultGame.ini、TBStageTestsとContentの参照コミットを一緒に反映してください。運搬位置はモデルの縮尺・向きの補正を受けず、運搬中は自分のおもちゃも三人称カメラに表示します。
