# 仮音素材の登録と再生対応

2026-10-17の選定メモで `selection=keep` の44素材だけを登録。pending 48素材は取り込んでいない。

BGMはCarefreeをタイトル・ロビー・結果に、Sneaky Adventureを試合に使用。同じ曲でも状態が変われば通常のフェード切替を行う。素材は原曲全長のため、ループの継ぎ目は保証しない。

設定は `Config/DefaultGame.ini`。BGM基準音量0.25、通常SE0.7、進行ループ0.45、テレビノイズ0.25。人間足音のおもちゃ向け補正（音量1.75倍・音程0.75倍）は維持。聴感調整はProject Settingsから変更可能。

| 素材（タイトル） | イベント／場面 | 再生条件・補足 |
|---|---|---|
| `foot_human_wood_03`：子ども足音／木床 3 | HumanFootstep | 近くの全員 |
| `foot_toy_wood_01`：おもちゃ足音／木製 1 | Footstep | 近くの全員 |
| `ui_hover`：UI選択 | UIHover | ボタンへのカーソル移動・部屋候補の手動選択 |
| `ui_confirm`：UI決定 | UIClick | 本人 |
| `ui_back`：戻る・キャンセル | UIBack | 本人 |
| `ui_error`：操作不可・接続失敗 | UIError | 操作拒否・接続失敗。0.5秒の連打抑制 |
| `ui_change`：陣営・設定変更 | UIChange | 陣営選択・人数等の設定変更がサーバーで受理された時 |
| `ui_ready`：準備完了 | Ready | ロビー |
| `ui_unready`：準備解除 | Unready | 準備解除操作がサーバーで確定した時 |
| `ui_join`：入室・接続成功 | LobbyJoin | ロビー到着時と参加人数増加時 |
| `ui_leave`：退出・部屋終了 | LobbyLeave | ロビー人数減少時と退出後のタイトル到着時 |
| `ui_notice`：味方の状態通知 | TeamNotice | 味方の捕獲・収納時。対象本人を除くおもちゃへ |
| `grab_soft`：捕獲成功／柔らかい | Capture | 捕獲成功時。現在はこちらのみ使用 |
| `grab_plastic`：捕獲成功／硬い | CaptureHard | 予備。将来おもちゃの硬さ属性で切り替える |
| `carry_lift`：持ち上げ | CarryLift | 捕獲後、持ち上げ完了時に運ぶ側と運ばれる側へ |
| `carry_drop`：手放す | CarryDrop | 手放し確定時に当事者2人へ |
| `land_human`：子どもの着地 | HumanLand | 近くの全員 |
| `land_toy`：おもちゃの着地 | Land | 近くの全員 |
| `jump_toy`：おもちゃのジャンプ | Jump | 近くの全員 |
| `store_finish`：収納完了／箱の衝撃 | Store | 近くの全員 |
| `store_cancel`：収納キャンセル | StoreCancel | 収納中から運搬状態に戻った時（完了・試合終了は除外） |
| `rescue_cancel`：救助中断 | RescueCancel | 進行していた救助の参加者が0人になった時（完了・試合終了は除外） |
| `freeze_alt`：視線停止／電子音候補 | Frozen | 対象のおもちゃ本人 |
| `release_alt`：視線解除／電子音候補 | Thawed | 対象のおもちゃ本人 |
| `tv_button`：リモコン踏下 | TVButton | 受理されたリモコン操作時 |
| `tv_channel`：チャンネル変更 | TVChannel | 箱内部 |
| `tv_power_on`：テレビ電源ON | TVPowerOn | 箱内部 |
| `tv_power_off`：テレビ電源OFF | TVPowerOff | 箱内部 |
| `tv_glitch`：映像切り替えノイズ | TVGlitch | チャンネル切替時 |
| `time_tick`：残り時間カウント | TimeTick | 残り10～1秒の秒数減少時。フレーム遅延分は連打しない |
| `jingle_pizzi_16`：開始・勝敗ジングル候補 16 | JingleAlternate | 予備。専用の開始・勝敗音があるため自動再生しない |
| `battery_chime`：電池取得／玩具チャイム案 | Pickup | 本人・おもちゃチーム |
| `battery_last`：電池あと1個／通知案 | BatteryLast | 取得後に残り電池が1個になった時 |
| `rescue_success`：救助成功／解放チャイム案 | RescueComplete | おもちゃ側限定 |
| `boxed_transition`：箱内への移動／下降チャイム案 | BoxedTransition | 収納された本人 |
| `match_start`：試合開始／玩具ベル案 | MatchStart | 全員・非定位 |
| `match_win`：勝利／短い玩具ファンファーレ案 | Victory | 勝利側・非定位 |
| `match_lose`：敗北／柔らかい下降音案 | Defeat | 敗北側・非定位 |
| `collect_loop`：電池収集進行／周期音案 | CollectLoop | 本人の収集進捗がある間。停止・中断・完了・行動不能で停止 |
| `store_loop`：収納進行／周期音案 | StoreLoop | 収納する本人 |
| `rescue_loop`：救助進行／周期音案 | RescueLoop | おもちゃ側限定 |
| `tv_static`：テレビ砂嵐／短音案 | TVStatic | 箱内で映像がない状態へ移った時の短音。ループしない |
| `bgm_carefree`：Carefree — タイトル・結果／明るく穏やか | Title / Lobby / Results | 各プレイヤー・非定位 |
| `bgm_sneaky_adventure`：Sneaky Adventure — 試合／少し軽快な探索 | Match | 各プレイヤー・非定位 |

## 再インポート

`Scripts/ImportSelectedAudio.py` をUnrealのPythonScript commandletで実行する。`TOON_AUDIO_SOURCE` 環境変数で素材元を指定可能。選定JSONのSHA-256を確認し、未作成アセットのみインポートする。既存アセットの音データは上書きせず、ループ設定だけ更新する。

採用素材の記録は `docs/AUDIO_SELECTION.json`。Content側の `Audio/Notices/` にもコピーと配布元クレジット・ライセンス資料を保存し、パッケージへ同梱する。配布元CREDITSは候補パック全体の記録で、実際の使用素材は選定JSONの44件。

Contentは別リポジトリなので、共有時は音アセットと親側のコード・設定の両方を反映する。

## 手動確認

- タイトル→ロビー→試合→結果→再戦で曲が切り替わり、古いBGMや進行ループが残らない。
- 人間とおもちゃで足音・着地を確認。おもちゃ側には人間の足音が大きく低く聞こえる。
- 収集・収納・救助を完了／中断し、ループ停止と通知先を確認。
- 箱内でリモコンを踏み、電源・チャンネル・砂嵐の音を確認。箱外にテレビ音や救助音が漏れない。
- 2台接続で、本人・チーム・ロビーの配信先を確認。

## 実行済み検証（2026-10-17）

- UE 5.8 Editorビルド成功。
- `ToonStory.Audio` 6件、`ToonStory.Match.Rematch`、`ToonStory.Television.ChannelLifecycle` の計8件成功。警告・エラー0。`Saved/Automation/SelectedAudio/index.json`。
- `ToonStory.Title.RoundTrip` 成功（ボイスキャプチャ関連の環境警告3件、エラー0）。`Saved/Automation/SelectedAudioTitle/index.json`。
- テストはNullRHI・nosound・OnlineSubsystem Nullで実行。音素材の読込、ループ設定、音量・ピッチ・距離減衰、配信条件、状態遷移を検証。実際の聴感バランス、Steam経由の別PC同時再生、配布パッケージでの試聴は未検証。
