# 接触で動く小物 Component

`UTBContactReactiveComponent` は、接触をserverで受けて単一Box proxyを有限角度だけ回す部品です。自由物理、破壊、無接触の常時演出、エモートは行いません。マップ・Contentへの実配置はユーザーが行います。

## 既存Actorへの設定

1. 既存ActorのBlueprintまたはinstanceにComponentを追加します。可動mesh自体がrootの場合は、固定のScene rootを用意し、その下に可動`BoxComponent`を作ります。rootそのものを回す構成は初単位の対象外です。
2. Actorの **ReplicatesをON、Replicate MovementをOFF**、ContactReactiveComponentのreplicationをONにします。Owner専用relevancyは使用せず、level Actorをclientにもロードしてください。
3. `MotionTarget`に同Actorのchild Boxを選びます。Movable、Simulate Physics OFF、Object Type WorldDynamic、Query有効、Human `Pawn`と既存`ToyPawn`両方への応答をBlockにします。見た目のmeshはBoxのchildにしてNoCollisionにします。
4. 別の固定`BoxComponent`等を接触sensorとして作り、`ContactTrigger`へ指定します。**QueryOnly / Object Type WorldDynamic / Generate Overlap Events ON / PawnとToyPawnをOverlap**、その他はIgnoreを推奨します。Pawn Object Typeのsensorは使用しません。現Character capsuleはoverlap eventsを生成しますが、Pawn channel同士はIgnoreなので、この設定が必要です。
5. sensorは対象表面に沿う薄い形にします。`MaxContactGapCm`（既定2cm、最大5cm）以内に実capsuleがある場合だけ反応します。大きな接近volumeにすると「中へ入った時は遠すぎて拒否され、そのまま近づいても新しいBeginOverlapが来ない」ため、正しい接触判定になりません。
6. `LocalAxis`と`LocalPivot`は**固定parent座標**で設定します。`AngleDegrees`は符号付き1～110度、`MotionSeconds`は片道の目安です。負の角度も使用できます。parent chainを含め正の均一scale、固定parent、固定Boxサイズが必要です。
7. 同Actorのcollision有効Primitiveはtargetとsensorだけにします。固定frame・床・支持壁は別Actorにします。複数の反応Component、Timeline、Sequencer、physics、既存door driver等で同じtargetを動かしてはいけません。外部移動や設定変更を検出するとDisabledで停止します。

対象のmeshを自動置換したり、既存Actorのcollision/mobility/replicationを勝手に変更する処理はありません。Componentの問題表示を確認してから使用してください。

## 動作と設定例

- 本棚のTip例：底辺付近のpivot、横軸、±60～90度、`bReturnAfterContact=false`。動くBoxの全軌跡に余白が必要です。自由落下する本棚ではありません。
- 扉のHinge例：蝶番のpivot、Z軸、±60～90度。最初の完成単位は接触から一度動き、その姿勢を保持し、round resetで戻します。汎用の押す／引く／鍵／NavLinkシステムではありません。
- 時計の揺れ例：上端pivot、横軸、数度、`bReturnAfterContact=true`。接触が開始した片道＋復帰だけを実行します。無接触の自動往復はありません。

既定`bOncePerRound=true`です。接触後に往復し、同roundでも再接触させたい場合だけfalseにします。再反応には全actorの離脱と`CooldownSeconds`経過、その後の新しい接触が必要です。同actorのmesh／capsuleや同frameの重複通知では増えません。速度・衝撃力の下限は設けず、ゆっくりした接触も対象です。大きさの違う物すべてで同じ設定が成立する保証はありません。

初単位は **Playingかつ準備終了後、CanActを満たすpossessed Character** の実capsule接触だけを採用します。10秒準備中のおもちゃ、Frozen、運搬中／収納中、Boxed、Lobby、Results、Abortedでは開始しません。準備中から触れたまま解除されても勝手には始まらず、一度離れて再接触が必要です。

## 通行・reset・複製の制限

回転のsweepやchild sweepを安全性の根拠にしていません。半度以下の各角区間について、中間姿勢のBoxを `2 R sin(|Δθ| / 4)` と微小余白で膨張した保守包絡を調べます。Rはpivotから最遠cornerまでのworld距離です。端点だけでなく途中の軌跡を含みます。接触した本人も除外せず、Character実capsuleは応答Ignoreでも占有として扱います。

塞がった場合は直前の承認済み姿勢でBlocked／ResetPendingになります。閉動作・復帰・round resetも同じ検査を通し、Pawnを押し出したり強制snapしたりしません。`TimeoutSeconds`到達ではDisabledとしてその姿勢を保持し、正常終了やreset完了には数えません。次roundでも安全なresetが終わるまでは新接触を受けません。床・壁の近くでは保守判定のため動けない場合があります。余白・pivot・角度をユーザーが調整してください。床を無条件に無視して動かす機能はありません。

replicated owner/componentが、baseline・承認pose・round/generation・statusのsnapshotを保持します。clientはsnapshotを適用するだけで、server承認進捗を越える時刻予測や自動再生をしません。late joinも最新snapshotから復元します。通信遅延中のclient予測Pawnとの全frame非重複まで保証するものではありません。

round識別は既存GameStateのPhaseとHidePreparation.RoundをComponentから監視し、既存Mode／Characterには手を加えません。`GetMotionSnapshot`と`GetMotionProblem`で姿勢・状態・停止理由を参照できます。HUDや通知音の追加は初単位に含みません。

## 検証

`ToonStory.ContactReactive.BoundedMotion` は一時native World／Actor／primitiveで接触・重複・cooldown境界・reset占有・有限timeout・offset pivot・負角・中間軌跡障害物・非均一scale拒否・snapshot適用を検査します。実overlapからの開始と直接handler／時計境界注入を区別し、後者を実ネット配送や実待ちの証明にはしません。これは既存Modeでの開始／再戦を通す統合試験ではなく、Phase／round payloadを変えるComponent単体境界試験です。

2026-10-28の独立QAで初回UHT成功（5.224秒、3生成ファイル）、fixture修正後のcompile/link成功を確認しました。`BoundedMotionRetry1` はJSON Success、errors 0／warnings 0、process exit 0です。両Actorのinitialized／BeginPlay、componentの登録／physics／query／eventsが成立し、raw shape queryもactual capsuleを1件検出した上で、元の実overlap・authority開始assertと後続検査を完走しました。接触者の占有拒否、角区間途中の障害物、往復・reset占有・timeout、承認snapshot適用はこの単体試験の実証範囲です。

初回のFail 3件／warning 1件は、WorldのActor初期化を通さずBeginPlayだけ通知したfixture不適合として分離・保全しています。Spawn前の`InitializeActorsForPlay`とcleanupの`EndPlay`を追加した修正版で成功しました。製品不良の証拠とは扱いません。証拠は`Saved/QA/20261028-addAnimation/QAResult.json`と`BoundedMotionRetry1/index.json`・対応logです。JSONの成功とraw logの起動前メッセージ（原因未特定を含む）は区別します。

実remote／late join配送、通常Modeでの開始・再戦reset、ユーザー設置sensorでの実接触・通行、支持床近傍の専用測定、実画像・映像は未検証です。人工時刻／Phase／role／snapshot注入を含む単体成功から、床沿いの棚・扉・時計すべての実演成功やネット全frame安全性は主張しません。
