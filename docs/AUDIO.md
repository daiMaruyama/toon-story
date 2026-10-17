# BGM・SE

2026-10-17: 採用メモの44素材を仮登録済み。素材の選定情報はContent側の [AUDIO_SELECTION.json](../Content/Audio/Notices/AUDIO_SELECTION.json) を参照。

音素材が未登録でもゲームは通常どおり動作します。未登録のイベントは無音でスキップされます。素材が届いたら、プロジェクト設定に登録することで既存のゲーム操作から再生されます。

## 素材の登録

1. Unreal Editorで音素材を `Content/Audio` 以下へ取り込む。このフォルダは配布用のCook対象です。
2. **Edit > Project Settings > Game > Toon Story Audio** を開く。
3. `Music` または `Sounds` の該当項目の `Sound` に、Sound Wave・Sound Cue・MetaSound Sourceを登録する。必要な項目だけで構いません。
4. `Volume`・`Pitch` を調整する。立体SEには `Attenuation` と `Concurrency` も指定でき、未指定時は既定の距離減衰と再生数制限を使用します。
5. 設定を保存してPlayを開始する。実行中に設定を変更した場合は、`UTBAudioSubsystem::ReloadAudioSettings()` で再読み込みできます。

設定は `DefaultGame.ini`、実際の音源はContent側で管理します。素材と設定を両方共有してください。別フォルダに置く場合は、そのフォルダもCook対象に追加してください。外部のwavファイルをパスで指定する方式ではありません。

`MusicFadeSeconds` はBGMの切替時間です。BGMと継続SEには、素材側でループを設定した音源を推奨します。Sound WaveではLooping、Sound Cue・MetaSoundではグラフ内で設定します。単発素材も終了後に再生し直しますが、継ぎ目のないループには素材側の設定が必要です。

UIClick・Pickup・Captureなどの単発SEには、ループしない素材を登録してください。音素材はゲーム開始時にまとめて読み込むため、最初は必要な音だけ登録します。

## BGMの対応

| 項目 | 場面 |
|---|---|
| Title | タイトル |
| Lobby | ロビー |
| Match | 試合中 |
| Results | 結果 |
| None | BGMを停止する内部状態。登録不要 |

`GameInstanceSubsystem` の `UTBAudioSubsystem` が現在のワールドと試合状態を監視します。BGMは端末ごとに1系統で管理し、同じ状態を毎フレーム再生し直しません。陣営ごとの別曲は現時点ではありません。マップ移動時には前のワールドの音を停止し、移動先に対応する曲へ切り替えます。素材未登録でも場面の追跡は行います。

## SEの対応

| 項目 | 用途 |
|---|---|
| UIClick | タイトル・HUDのボタン操作 |
| Ready / Unready | 準備完了はロビー全員、準備解除は本人 |
| MatchStart | 試合開始 |
| Victory / Defeat | 自分の陣営の勝利 / 敗北 |
| Pickup | アイテム取得成功。おもちゃチームのみ |
| Capture | 捕獲成功。発生位置から周囲へ |
| Store | 収納完了。発生位置から周囲へ |
| RescueComplete | 救助完了。Toysのみ |
| Frozen / Thawed | 自由状態での凍結 / 凍結解除。当人のみ |
| Footstep | Toysの足音。発生位置から周囲へ |
| HumanFootstep | Childrenの足音。発生位置から周囲へ。Toysには大きく低い音で聞こえる |
| Jump / Land / HumanLand | おもちゃのジャンプ / おもちゃの着地 / 人間の着地 |
| RescueLoop | 救助中の継続音。Toysのみへ（仮素材は非定位） |
| StoreLoop | 収納中の継続音。収納を操作している本人のみ |

再生位置を持つSEは `bSpatial` で立体音を指定します。距離減衰アセットを設定しない場合は `MaxDistance` が使われます。`MaxInstances` は同じイベントの音が過剰に重ならないための上限です。BGM・UI・結果通知などの画面向けの音は2Dで再生します。

足音は移動距離で発生させます。既定の間隔はChildrenが130 cm、Toysが45 cmで、音声コンポーネントから変更できます。アニメーションの接地タイミングや床の材質別音源との連動はまだありません。初回の状態取得、ワープ、収納・捕獲などによる移動方式の変更では、足音・ジャンプ・着地音を発生させません。

### Childrenの足音

`Sounds` の **HumanFootstep** にChildren用の足音素材を登録します。ChildrenもToysも同じ素材を使い、聞いているプレイヤーがToysの場合だけ、音量とピッチに補正を掛けます。**Human Footsteps** の設定項目で変更できます。

| 設定 | 既定値 | 意味 |
|---|---|---|
| HumanFootstepToyVolumeMultiplier | 1.75 | Toysが聞くChildrenの足音の音量倍率 |
| HumanFootstepToyPitchMultiplier | 0.75 | Toysが聞くChildrenの足音のピッチ倍率 |
| Sounds / HumanFootstep / MaxDistance | 2400 cm | 減衰アセット未指定時の届く距離 |

Childrenの足音は発生位置から立体音で鳴り、近づくほど大きく聞こえます。減衰アセットを登録しない場合は、発生位置で最大、2400 cmで無音になる直線的な距離減衰を使います。`MaxDistance` で距離を変更でき、独自の `Attenuation` アセットを指定した場合はその設定を優先します。既定の同時再生数上限は16です。

音を聞く位置は、オンラインIDの有無にかかわらずローカルキャラへ追従します。ロビーから試合へ移ってキャラの向きが変わる場合も聞く方向の補正を更新します。オフラインでの確認時にも、離れたロビーカメラの位置で距離を測る構成にはしません。

例えば素材の `Volume` が0.8なら、Toysが聞く基準音量は0.8×1.75＝1.4です。ここへ距離減衰・全体音量・SE音量が掛かります。Childrenが聞く場合は素材の音量・ピッチのままで、Toys自身の `Footstep` にはこの補正を掛けません。`HumanFootstep` が空ならChildrenの足音は鳴らず、Toysの素材で代用しません。既存の `Footstep` にChildren用素材を設定していた場合は `HumanFootstep` へ登録し直してください。

## 処理の分担

- `UTBAudioSettings`：イベントと音素材の対応、基準音量、距離減衰、同時再生設定。
- `UTBAudioSubsystem`：BGMの状態管理と切替、SEの共通再生、ユーザー音量の反映、マップ移動時の停止。
- ゲーム処理：取得や捕獲などが成功したことを通知。各所に音源パス・音量・再生用コンポーネントの作成処理を書かない。
- キャラ・箱の音声処理：発生位置と継続状態を管理。中断・破棄・結果・再戦でループを停止。

単発のゲームイベントはサーバーで成功を確定してから対象クライアントへ通知し、各端末の共通処理で再生します。UI音は操作した端末のみで再生します。Dedicated Serverでは音を再生しません。

発生処理は `UTBAudioEvents::Emit` を使い、通知対象を共通判定します。足音と継続音は受信したキャラ・箱の状態を各端末で観測します。過去の単発イベントは途中参加者に再生し直しません。救助音は陣営で制限します。定位の有無は設定で変更できます。1端末1プレイヤーを前提とし、画面分割で陣営の異なる複数プレイヤーが同じ音声出力を使う構成は対象外です。

## 後から付ける音量設定画面

`Get Game Instance Subsystem (TB Audio Subsystem)` から次の関数を呼び出せます。

| 関数 | 対象 |
|---|---|
| SetMasterVolume | 全体 |
| SetMusicVolume | BGM |
| SetEffectsVolume | SE |
| SetVoiceVolume | ボイスチャット |

入力範囲は0〜1で、範囲外の値は制限されます。`GetMasterVolume`・`GetMusicVolume`・`GetEffectsVolume`・`GetVoiceVolume` はスライダー初期値に使えます。全体音量とカテゴリ音量を掛け合わせるため、BGMを0にしてもSEの設定値は変わりません。既に再生中のBGM・継続SEにも音量変更を反映します。

制作者が各素材に設定する `Volume` と、プレイヤーが操作するカテゴリ音量は別です。設定画面やファイルへの保存はまだありません。値は同じGameInstance内のマップ移動・再戦では維持され、アプリの次回起動時には既定値に戻ります。

## 確認

音素材なしでの自動テストと、実際に音を聴く確認は分けます。`ToonStory.Audio` の自動テストは次を対象にします。

- `EmptyAssetsAndVolumes`：空素材・未登録項目の安全な処理、カテゴリ音量と上下限、既存のボイス用コンポーネントへの音量変更。
- `PhaseLifecycle`：ロビー→試合→結果または中止→再戦時のBGM状態と音量設定の維持。
- `AudienceIsolation`：本人向けの通知先、救助通知のToys限定、ローカルで観測する音がサーバー側でも重複送信されないこと。
- `RegisteredClipLifecycle`：テスト用の無音音源をメモリ内で登録し、再生コンポーネントの生成・音量反映・GCでの保持・停止・BGMの重複防止を確認。素材ファイルは追加しません。
- `HumanFootstepPerspective`：同じChildren足音素材に対する陣営別の音量・ピッチ、距離に応じた減衰、独自の減衰設定の維持、再生中の音量変更、素材未登録時の扱いを確認。

テストは登録済み素材を一時的に外し、終了時に戻します。実行時の設定変更をファイルに保存しません。次の例はプロジェクト直下から、エディタを終了してEditorターゲットのビルド後に実行します。

```powershell
$AudioEditor = 'E:/Epic/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
$AudioProject = (Resolve-Path ./ToonStory.uproject).Path
& $AudioEditor $AudioProject /Game/Maps/Arena -game -NullRHI -nosound -nosteam -unattended -nop4 '-ExecCmds=Automation RunTests ToonStory.Audio' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD/Saved/Automation/Audio"
```

終了コードだけでなく `Saved/Automation/Audio/index.json` の結果を確認してください。このテストは別プロセス間の実際のRPC配送や聴感の二重再生まで検証するものではありません。音の種類・聴感音量・フェードの自然さ・ループの継ぎ目は、素材登録後に確認します。

素材を入れた後の手動確認：

1. タイトル→ロビー→試合→結果→再戦→タイトルと進み、各BGMが切り替わり、前の音が残らないことを確認する。
2. 各SEの操作を行い、ホストとクライアントで対象の音が1回だけ鳴ることを確認する。
3. 救助・収納を開始してから中断、距離外へ移動、試合終了、退出を行い、継続音が停止することを確認する。
4. ChildrenとToysの両方で救助中の音を確認し、Childrenへ救助進捗の音が伝わらないことを確認する。
5. BGM・SE・ボイス・全体音量を個別に0、0.5、1へ変更し、再生中の音と新しい音へ反映されることを確認する。
6. 音源を登録した配布パッケージでも同じ操作を確認する。エディタでの再生だけではCook結果を確認できません。
7. Childrenを歩かせ、Children側では通常、Toys側では大きく低い足音として聞こえることを確認する。Toysで距離を変え、近づくほど大きく、設定範囲の外では聞こえなくなることも確認する。Toys自身の足音が変わらないこと、Children用素材が空なら足音が鳴らないことも確認する。
8. オフライン起動でも距離減衰が自分のキャラ位置を基準に変わること、ロビー→Toysでの試合開始後に左右の聞こえる方向が正しいことを確認する。

別PC間の可聴確認・実際の素材を入れたパッケージ確認は、素材登録後の確認項目です。

### 2026-10-14 実行結果

- Development Editor / Win64ビルド成功。
- `ToonStory.Audio` 4件、`ToonStory.Lobby.Counts`、`ToonStory.Match.Rematch` が成功。レポートは `Saved/Automation/AudioFoundation/index.json`。
- `ToonStory.Title.RoundTrip` が成功（環境の警告あり、テストエラーなし）。レポートは `Saved/Automation/AudioTitleFlowVerified/index.json`。
- タイトル往復はSteam設定のまま `-nosteam` で起動すると入室に失敗するため、保存設定を変更せず、下記の起動引数を追加してローカル通信で検証しました。

```text
-ini:Engine:[OnlineSubsystem]:DefaultPlatformService=Null
-ini:Engine:[OnlineSubsystemSteam]:bEnabled=false
-NetDriverOverrides=/Script/OnlineSubsystemUtils.IpNetDriver
```

すべて `-NullRHI -nosound` での確認です。再生経路・状態管理の検証であり、実音やSteam別PC接続での聴感確認を完了したものではありません。

### 2026-10-16 人間の足音

- Development Editor / Win64ビルド成功。
- `HumanFootstepPerspective` が成功。同じ素材に対する聞き手別音量・ピッチ、距離が近いほど大きくなる減衰、範囲外の無音、既存の音量操作、空素材を確認。レポートは `Saved/Automation/HumanFootstepsVerified/index.json`。
- 既存の音声テスト4件と `ToonStory.Match.Rematch` も成功（`Saved/Automation/HumanFootsteps/index.json`）。この初回レポートでは新テストのピッチ比較が小数の丸め差で失敗し、許容誤差修正後に上記の個別テストで再確認しています。
- 今回も無音環境での自動確認です。素材登録後の音色・音量バランス、歩行中の可聴確認、別PC間での聞こえ方は未確認です。
