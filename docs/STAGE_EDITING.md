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

## World Outlinerで探すもの

| 名前・種類 | 編集する内容 |
| --- | --- |
| `Bedroom_` / `Playroom_` | 子ども部屋ステージの各室の家具・建物 |
| `ApartmentA_` / `ApartmentB_` | アパートステージの各室の家具・建物 |
| `Hall_` / `Corridor_` | 部屋をつなぐ床・壁・天井 |
| `Spawn_00` などのPlayerStart | プレイヤーの開始位置。壁・家具との重なりと床の有無を確認 |
| `ToyBox_` のTBBox | おもちゃの収納・救助用の箱。移動時は外側の操作位置への経路も確認 |
| `Collectible_` のTBPickup | 収集アイテム。家具の中や到達不能な位置に置かない |
| `Lobby_Overview` | ロビー背景のCameraActor。位置・回転・画角を編集 |
| `RoomExposure` | PostProcessVolume。露出やブルームを編集 |
| `WarmRoomLight_` | 室内のPointLight。明るさ・色・範囲を編集 |

Actor名は検索用です。カメラの識別には名前ではなく `TBLobbyCamera` のActor Tagを使います。

## 維持する条件

- 家具付きの部屋を2室以上とし、部屋同士・箱の操作位置への通路を確保する。
- 扉は開いた回転角度で配置し、MobilityをStaticにする。`DoorFixedOpen` のActor Tagを保持する。
- World SettingsのGameMode Overrideは `TBGameMode`。TBBoxは1つ、収集物は試合の必要数以上（既定5個）。
- PlayerStartは現在8個。定員を変更する場合は `Config/DefaultGame.ini` のMaxPlayersと合わせて見直す。
- `TBLobbyCamera` タグ付きCameraActorは1つ。名前だけ変更してもタグがあれば動作する。
- 床・壁・大きい家具には通行を制限する衝突を設定する。30cm未満の配置小物はCollision EnabledをNo Collisionにする。
- Static Meshアセット自体を編集すると、同じメッシュを使う別の配置にも影響する。配置ごとの変更はActorのDetailsで行う。

子ども・ぬいぐるみとアニメーションは保存済みです。見た目の参照先は `Config/DefaultGame.ini`、アニメーションは `Content/Characters/ChildAnimation` で管理します。

## 確認と保存対象

試遊方法は [PLAYTEST.md](PLAYTEST.md) を参照してください。PIEでは開いているマップを使い、Host roomを押すとC++が既存の2マップから抽選します。
固定のマップ名を変更・追加する場合は、C++の選択対象とConfigの起動マップ・MapsToCookも変更が必要です。

`ToonStory.Stages.RuntimeLayout` は各マップのゲーム実行で開始位置、通行、箱、収集物、小物の衝突、カメラ、キャラクターを検証します。
現状の戸口座標と8個以上の開始地点を基準にしているため、間取りや定員を変更した場合は `Source/ToonStory/Tests/TBStageTests.cpp` も新しい設計に合わせて更新してください。

配置を変えた `.umap` と編集した素材の `.uasset` はContentリポジトリ側で管理します。
`__ExternalActors__` 等はUEに管理させ、手作業で編集しないでください。

## キャラクター変更の統合待ち

別ブランチの作業と調整するため、今回のステージ・再戦コミットには `TBCharacter.cpp/.h` の見た目の変更と、それを使う `TBStageTests.cpp`、DefaultGame.iniのモデル参照設定は含めていません。これらはローカルの未コミット変更として保持しています。モデルとアニメーションのアセットはContentに準備済みですが、このコミット単体のプレイヤー表示は従来の簡易表示です。統合時はコード・設定・テストをまとめて反映してください。
