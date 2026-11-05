# 押して動く小物

`ATBGameState::ConfigurePushableProps` から `TBPushableProps::TryEnable` を呼び、既存配置を実行時に物理化する。レベルアセットの保存や再配置は不要。

- 対象レベルは `TB_StylizedHouse` と `TB_ToyWorldDemoExpanded`。共有のおもちゃ箱ルームは対象外。
- Houseでは缶9種類、シリアル箱3種類、タイマー、クッキージャー、ガラス瓶、洗面所のコップが対象。
- ToyWorldでは電池、ミニカー、トラック、木のおもちゃ、列車、トラクター、カセット、ゲームパッド、リモコン、絵の具、果物、水筒が対象。
- メッシュのフォルダーと名前を許可リストで照合する。家具、床、壁、線路、複数部品のキャラクターは対象外。半径方向の最大半サイズ35cm以下で、物理シミュレーション可能な簡易衝突形状を持つものだけを使う。
- 固定したい個体にはActorまたはStaticMeshComponentに `TBFixedProp` タグを付ける。既存の `TBPushable` 個体やBlueprint、他Actorに取り付けられた個体は変更しない。
- 開始時は物理をスリープさせる。人間が接触して押すと動き、サーバーから位置を同期する。おもちゃ役は従来どおり押し動かさず、すり抜ける。小物には乗れない。
- 既存のメッシュ、マテリアル、Nanite設定をそのまま使う。追加のメッシュ複製やコリジョン生成は不要。

## 検証

各ステージのStandaloneで `Automation RunTests ToonStory.Playroom.ExpandedProps`。実際の物理ボディ、移動、床との衝突、既存の押せるタグ、再設定時に動作をリセットしないことを確認する。

同じステージをListen Serverとクライアント2個で起動し、3プロセスそれぞれで同じテストを実行すると、サーバーが動かした小物がクライアントでも同じテスト台へ移動・着地することを確認できる。テスト台と移動は一時的で、マップには保存しない。

各プロセスには別々の `-SessionId=<GUID>` を指定してAutomation同士の干渉を防ぐ。マップURLはuprojectの直後に置く。テストはGameStateのBeginPlay到着を待ってから小物を調べる。

ガラスの落下SEは `GlassDropLight/Medium/Heavy` の3段階すべて `glass_clink` を使用する。高さごとに音量が変わり、破砕音は鳴らさない。素材・ライセンスは `AUDIO.md` と `Content/Audio/Notices/CREDITS.txt` を参照。
