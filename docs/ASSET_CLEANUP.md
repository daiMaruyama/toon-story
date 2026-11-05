# 配布素材と保管素材

コンテンツブラウザーの「コレクション」から以下の共有一覧を開けます。2026-11-03 時点の Windows 用 CookList に含まれない素材のうち、制作時の参照・再利用に向くものを選んだ一覧です。ファイルの場所は変えていません。コレクションに入っていても、今後マップなどから参照すれば通常どおり配布されます。

| コレクション | 内容 |
| --- | --- |
| Reference_ExampleMaps | 素材パックの展示・原本マップ（8） |
| Reserve_UnshippedProps | 配置に再利用できる小物・背景のスタティックメッシュ（398） |
| Reserve_CharacterSources | モデル編集・衣装変更で参照できるスケルタルメッシュ（17） |
| Reserve_Lookdev | 予備のマテリアル・テクスチャ（219） |

インポート前の FBX・Blender・ZIP 等は Content サブモジュールの `_Sources` に保管します。今回削除したのは、現行の Play90 版に置き換え済みで、他のアセットから参照されていなかった旧 Poly Haven 宝箱の UE アセット5点です。再利用する場合は `_Sources/PolyHavenChest` にローカルの原本があります。共有用ライセンスは Content の `ThirdPartyNotices/PolyHavenChest` にも残しています。

## 人間を追加・変更したとき

`DefaultGame.ini` の `HumanAppearances` はゲーム側の抽選リストです。設定ファイルから選ぶモデルも配布されるよう、名簿を変えたら次のコマンドをプロジェクト直下で実行し、更新された `/Game/CookLabels/PAL_PlayableHumans` を Content 側の変更に含めてください。既に一致している場合は保存しません。衣装や髪を既存の Blueprint 内で交換するだけなら、追加手順は不要です。

```powershell
& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" (Resolve-Path ToonStory.uproject) -run=pythonscript "-script=$((Resolve-Path Scripts/SyncPlayableHumanCookLabel.py).Path)" -unattended -NullRHI
```

## 配布対象の確認

Windows では `UnrealEditor-Cmd.exe ToonStory.uproject -run=cook -TargetPlatform=Windows -CookList` の `LogCookList` が確認先です。UE のパッケージはメッシュやマテリアル等を数える単位で、配布用ファイル本数とは異なります。

今回の変更で `/Game` の対象は 3,050 → 2,688（除外363・管理用ラベル追加1）になりました。既存 Windows 配布物の IoStore と突き合わせると、除外したうち105パッケージのデータは圧縮後 160,407,341 バイトでした。これは既存版に含まれていたデータ量で、再ビルド後の配布物全体のサイズ差や FPS の測定値ではありません。
