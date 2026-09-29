> 2026-09-30: この文書のゲーム仕様・導入手順は旧版です。扉処理は削除済み、時間切れは人間勝利、おもちゃ同士の衝突あり、Steam 480設定済みです。現在の手順は [PLAYTEST.md](PLAYTEST.md) を参照してください。

> この資料はZIP同梱文書を移植用にパス調整したものです。元の検証環境・未検証という記述は配布時点の記録です。このブランチの変更・検証結果は [INTEGRATION.md](INTEGRATION.md) を参照してください。コマンドはリポジトリのルートで実行します。

# 制作PCへの引き継ぎ

## 今回の到達点

ソースと設定、静的チェック、UE非依存のルールテストまで。UE5.8でのビルドとプレイは未検証。これは開発の土台であり、完成ゲームではない。

v2では、重力あり凍結・即時解除・人間による3秒閉鎖・自力脱出・接触3秒取得を反映済み。

## 最初の作業順

1. UE5.8で空のC++プロジェクトがビルドできることを確認。
2. 同梱ToyBoxを別フォルダーに展開し、Development Editor / Win64をビルド。
3. 必要な版差だけ修正。UEのAPIエラーとゲームロジックの変更を分ける。
4. /Game/Maps/Arenaを作成し、照明を配置。
5. Nullの2人PIEで移動・凍結を確認。
6. 3人で扉開閉・脱出、3秒接触の停止／再開を確認。
7. Developmentパッケージを別PCで起動。
8. Steam設定へ切り替え、別アカウント・別回線で確認。

## 別の制作支援AIへ渡す指示

> README_ja.mdの確定仕様を優先し、SourceをUE5.8でビルドしてください。最初にUHT/C++/リンクのエラーを解消し、仕様を勝手に変更しないでください。UE非依存のテスト通過はUEビルド成功を意味しません。空中で凍結しても重力は残します。視線解除に追加待機を入れません。扉は救助で開き、人間が視点固定で3秒長押しして閉めるまで収納不可です。アイテムは接触3秒、離れれば0、凍結中は保持して停止です。妨害はApplyInterruptionの入口から後で追加します。まずNullで最低3人の受入テストを行い、その後Steamへ進んでください。

## クラスを分割したい場合

初版は導入しやすいようTBGame.h/.cppへ集めている。安定後、Character / Movement / Box / Pickup / GameMode / State / UIへ分割できる。UCLASSを移す場合は対応generated.h、include、TOONSTORY_API、モジュール依存を合わせる。ファイル分割と機能変更を同じコミットで行わない方が確認しやすい。

## 既存プロジェクトへ移植する場合

- まず既存プロジェクトをバックアップまたはgitでコミット。
- Source内のクラスを既存モジュールへ追加。TOONSTORY_APIをそのモジュールのAPIマクロへ置換。
- Build.csへOnlineSubsystem、OnlineSubsystemUtils、InputCoreを追加。
- iniを丸ごと上書きせず、入力・GameMode・NetDriver・MapsToCookを統合。
- クラスパス /Script/ToonStory.TBGameMode を実際のモジュール名へ変更。
- 既存のEnhanced Inputを使う場合は、CharacterのRequestInteract / ReleaseInteract / RequestDropや移動関数に相当する入力へ接続。旧入力設定との二重入力を避ける。
- Blueprint資産の親クラス・GameMode・DefaultPawnClassを確認。
- 新しくUCLASSを追加したらエディタを閉じて通常ビルド。

## 特に確認する箇所

| 場所 | 確認する理由 |
|---|---|
| UTBMovement::PhysCustom / MoveAutonomous | 重力付き凍結とCMCのサーバー補正の組合せ |
| ATBCharacter::ApplyState | 凍結・運搬・閉鎖・結果画面の状態の区別 |
| ATBBox::OnRep_Door | クライアント側でも開閉の衝突が一致するか |
| ATBBox::BeginClose / EffectiveViewRotation | 人間の実際のカメラとサーバー視線が一致するか |
| DoorwayClear / UpdateEscapes | 箱サイズや出口位置を変えたときの判定 |
| ATBPickup::Touches | SphereとCapsuleのOverlap、Visibility設定 |
| UpdateItemContact | 接触解除、凍結、捕獲、同時取得の境界 |
| TBSession | プラグイン版差、Steamの返却する検索フラグ、各非同期コールバック |

## 仮処理を変更する場所

- 閉鎖中断や攻撃：ApplyInterruption / bAllowInterference。
- 閉鎖・収納・救助時間：箱のEditAnywhereプロパティ。
- 取得時間と接触範囲：PickupのCollectSeconds / Contact。
- 逃げ遅れ／自分で箱へ戻る扱い：箱のTick、UpdateEscapes。
- 凍結の視野：GameMode::Watched。CameraのFOV／アスペクト比と一緒に変更。
- ロビー人数や時間：FTBSettingsとServerRules。
- 見た目：Blueprint、Body、CarryAnchor、Door、AlarmSound。

## 問題が出たときの記録

UEの詳細バージョン、Visual StudioとMSVC、Null/Steam、PIE/パッケージ、人数、ホスト陣営、操作手順、期待結果、実際の結果、最初のビルドエラー、Saved/Logs、必要なら両画面動画。

## ルールだけを再テスト

Visual StudioのDeveloper Command Promptで、プロジェクトルートから：

```bat
cl /utf-8 /std:c++17 /EHsc Tests\RuleTests.cpp /Fe:RuleTests.exe
RuleTests.exe
```

これはUEを使わない小さなテスト。衝突・Replication・実接続は検証しない。assertを無効化するNDEBUGを付けずに実行する。
