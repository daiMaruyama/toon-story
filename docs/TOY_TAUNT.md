# おもちゃの「音を鳴らす」

試合中、自由に動けるおもちゃで **F** を押すと、自分の位置から仮の「プッ！」というSEを1回鳴らす。
人間・おもちゃの両方に聞こえる。距離で減衰し、既定の最大距離は1800cm。
凍結中・運搬中・収納中、人間、ロビー・リザルトでは使用できない。
長押しで自動連発はせず、サーバーで検証・CD管理して既存の音声通知経路から各プレイヤーへ一度ずつ配信する。

## 調整

- キー：Project Settings → Input → Action Mappings → `Taunt`（既定F）。
  `Config/DefaultInput.ini` に保存する。実行中のプレイヤー向けキー変更画面は今回追加していない。
- CD：`Content/Data/DA_TBRules` → おもちゃ → 「音を鳴らすクールダウン」（既定0.1秒、最小0.1秒）。
- SE：Project Settings → Game → Toon Story Audio → Sounds → `ToyTaunt`。
  Sound、Volume、Pitch、MaxDistanceなどを変更できる。現在のPitchは1.5（高めの音）。
- 仮素材：`Content/Audio/SE/toy_taunt`。元WAVは `Content/Audio/Source/toy_taunt.wav`。
  合成したモノラル・約0.09秒の短いクラクション音。Fを押し直すたびに発音でき、最短0.1秒間隔で連打できる。

現在のBGM・SE一覧は `UTBAudioSettings`（DeveloperSettings）のMusic／Soundsマップ。
参照・音量等は `Config/DefaultGame.ini` に保存し、音声サブシステムが読み込む。DataAssetやDataTableではない。
今回もこの経路へ登録し、SE音量設定・ラジオによるおもちゃ行動音のマスクに対応している。
音の発生位置はサーバーが決定する。聞こえる範囲を広げると人間へ居場所が伝わりやすくなる。

HUDとタイトルの「あそびかた」にF操作を追記。
`ToonStory.Audio.ToyTaunt` で操作条件、連打制限、受信チーム、入力設定、音素材を検証する。
LANでの方向・距離感は、人間とおもちゃで別接続して確認する。
