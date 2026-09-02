# 開発環境セットアップ

## リポジトリ構成

このプロジェクトは 2 つのリポジトリに分かれている。

| リポジトリ | 公開範囲 | 中身 |
| --- | --- | --- |
| [toon-story](https://github.com/daiMaruyama/toon-story) | public | C++ ソース、Config、`.uproject`、プラグイン |
| [toon-story-content](https://github.com/daiMaruyama/toon-story-content) | **private** | `Content/` 配下のアセット全部（`.uasset` / `.umap`） |

`toon-story-content` は `toon-story` の `Content/` に **Git submodule** としてぶら下がっている。
アセットはバイナリで容量が大きく、コードとは更新の頻度も扱いも異なるため、リポジトリを分けている。

このため、**普段のコミット先が 2 つある**。ここが唯一ややこしい点なので、下の「日常の作業」を読んでほしい。

---

## 0. 事前に必要なもの

- Unreal Engine 5.8
- Visual Studio 2022
  - ワークロード「**C++ によるゲーム開発**」
  - 個別コンポーネント「**Windows 10/11 SDK**」「**.NET Framework 4.6.2 targeting pack**」
- Git
- **`toon-story-content` へのアクセス権**（private のため招待が必要。持っていなければ @daiMaruyama に依頼）

アクセス権が無いと clone 自体は通るが `Content/` が空になり、エディタが起動しない。

---

## 1. clone

```bash
git clone --recurse-submodules https://github.com/daiMaruyama/toon-story.git
cd toon-story
```

`--recurse-submodules` を忘れて clone してしまった場合:

```bash
git submodule update --init --recursive
```

---

## 2. 初回だけ必ずやる設定

clone したディレクトリの中で実行する。**この 5 行をやらないと事故る。**

```bash
git config push.recurseSubmodules on-demand
git config submodule.recurse true
git config status.submoduleSummary true
git config diff.submodule log
git -C Content checkout main
```

| 設定 | 何が起きるか |
| --- | --- |
| `push.recurseSubmodules on-demand` | 親を push したとき、未 push のアセットがあれば Content も自動で push する。**後述の事故を防ぐ本命** |
| `submodule.recurse true` | `git pull` / `git checkout` で Content も自動で追従する |
| `status.submoduleSummary true` | `git status` に Content 側の変更概要が出る |
| `diff.submodule log` | `git diff` で Content の変更がコミットログとして読める |
| `git -C Content checkout main` | submodule は既定で **detached HEAD** になる。そのままアセットをコミットするとどのブランチにも乗らず迷子になるので、`main` に乗せておく |

これらは clone ごとのローカル設定なので、**環境を作り直すたびに実行する**。

---

## 3. ビルド

1. `ToonStory.uproject` を右クリック →「**Generate Visual Studio project files**」
2. 生成された `ToonStory.sln` を開く
3. 構成を `Development Editor` / `Win64` にしてビルド
4. F5 で起動、または `ToonStory.uproject` をダブルクリック

`.sln` は Git 管理外（`.gitignore` 済み）なので、各自の環境で毎回生成する。

---

## 4. 日常の作業

### どっちにコミットするかの判断

| 変更したもの | コミット先 |
| --- | --- |
| `Source/**/*.cpp` `*.h` | toon-story |
| `Config/*.ini` | toon-story |
| `ToonStory.uproject` | toon-story |
| `Content/` 配下の `.uasset` `.umap`（BP、マテリアル、モデル、レベル…） | **toon-story-content** |

UE エディタで作業すると、この 2 つは**同時に変わることが多い**。
たとえば「C++ で新しい Actor を作って、それを継承した BP を置く」なら、コード側と Content 側の両方にコミットが要る。

### アセットを変更したとき

**必ず Content を先に push する。**

```bash
# 1. Content 側
git -C Content add -A
git -C Content commit -m "キャラクターのマテリアルを調整"
git -C Content push

# 2. 親側（Content がどのコミットを指すかを記録する）
git add Content
git commit -m "Update content"
git push
```

`git add Content` でコミットされるのは**アセットの中身ではなく「Content リポジトリのどのコミットを使うか」という参照 1 行だけ**（gitlink と呼ぶ）。だから親側の履歴は軽いまま保たれる。

### コードだけ変更したとき

普通に `git add` / `git commit` / `git push`。特別なことは要らない。

### 順序を間違えるとどうなるか

親だけ push して Content を push し忘れると、**他のメンバーの環境が壊れる。**
親リポジトリが「リモートに存在しないコミット」を指した状態になり、相手側で:

```
fatal: remote error: upload-pack: not our ref <sha>
Fetched in submodule path 'Content', but it did not contain <sha>
```

が出て `git submodule update` が失敗する。手元では正常に見えるので気づきにくい。

**「2. 初回だけ必ずやる設定」の `push.recurseSubmodules on-demand` を入れておけば、親を push したときに Content も自動で push されるのでこの事故は起きない。**

---

## 5. 最新を取り込む

```bash
git pull
```

`submodule.recurse true` を設定してあれば、これだけで Content も追従する。
設定していない場合、または Content が古いままのときは:

```bash
git pull
git submodule update --init --recursive
```

**エディタを閉じてから pull すること。** UE 起動中にアセットが書き換わると、開いているアセットとディスク上の内容がずれて保存時に壊れる。

---

## 6. ルール

- **アセットは必ず `Content/` の下に置く。** `Source/` やプロジェクト直下に画像やモデルを置かない（アセットは Content 側で一元管理する）
- **画像・動画・元データも、すべて `Content/` の下に置く。** `Content/` 配下はファイルの種類を問わず private 側のリポジトリに入る。UE は `Content/` 内の非アセットファイルを無視するので、`.psd` や参考画像を置いてもエディタには影響しない（動画は UE の作法どおり `Content/Movies/` に生ファイルのまま置く）
- **`Content/` の外に置いた画像は追跡されない。** 画像は Content 側で一元管理するため、ルートの `.gitignore` で `.png` `.jpg` `.psd` などを除外してある
- **README 用のスクリーンショットや GIF は `docs/images/` に置く。** 画像を追跡してよいのはここだけ（`.gitignore` で例外指定している）
- `Plugins/**/Content/` は `.gitignore` で除外してある。プラグインが同梱するアセットは追跡しない方針のため。**自作プラグインのアセットを追跡したくなったら**、ルートの `.gitignore` にある `# !Plugins/YourPlugin/Content/` の行を、プラグイン名を入れて有効化する
- `Saved/` `Intermediate/` `Binaries/` `DerivedDataCache/` はコミットしない（`.gitignore` 済み）
- `.uasset` / `.umap` は**マージできない**。同じアセットを 2 人が同時に編集すると、どちらかを捨てるしかない。着手前に声をかけ合う
- Content 側を push せずに親を push しない（設定を入れていれば自動で守られる）

---

## 7. GUI クライアントで使う場合

CLI でなくても構わないが、**submodule を扱えるクライアントを選ぶこと。**

### VS Code

`git.detectSubmodules`（既定 ON）により、submodule を**別リポジトリとして自動検出**する。

1. `Ctrl+Shift+G` でソース管理ビューを開くと、`TOON-STORY` と `CONTENT` が別セクションで並ぶ
   - 表示されない場合はビュー右上の `…` →「ビュー」→ **Source Control Repositories** を有効化
2. アセットを変更したら **CONTENT** セクションでステージ → コミット → **プッシュ**
3. その後 **TOON-STORY** に `Content` が変更として現れるので、ステージ → コミット → プッシュ
4. 左下ステータスバーのブランチ表示は「今アクティブなリポジトリ」のもの。クリックで切り替わる

### JetBrains Rider

submodule を正式サポートしている。Commit ツールウィンドウで親と Content の変更が階層表示され、Push ダイアログに両方が並ぶ。UE 開発なら一番扱いやすい。

### Fork / SourceTree / TortoiseGit

いずれも submodule 対応。Content を別リポジトリとして開いて操作する。

### GitHub Desktop

**submodule をまともに扱えないので使わないこと。** Content の変更がコミットできず、gitlink の更新も表示されない。

---

## 8. トラブルシュート

| 症状 | 原因と対処 |
| --- | --- |
| `Content/` が空。エディタが起動しない | `git submodule update --init --recursive`。それでも空なら `toon-story-content` へのアクセス権が無い |
| `Fetched in submodule path 'Content', but it did not contain <sha>` | 誰かが Content を push せずに親を push した。その人に Content の push を依頼する |
| Content で「HEAD detached」と出る | `git -C Content checkout main && git -C Content pull` |
| Content でコミットしたのに GitHub に出ない | detached HEAD でコミットした。`git -C Content log` で SHA を控え、`git -C Content checkout main` してから `git -C Content cherry-pick <sha>` |
| エディタが「アセットが見つからない」と言う | Content が古い。エディタを閉じて `git pull` |
| 初回起動で「The following modules are missing or built with a different engine version: VisualStudioTools / VisualStudioBlueprintDebuggerHelper」と出る | 正常。`Binaries/` は Git 管理外なので、clone 後の初回だけコンパイルが必要。**Yes** を押す（1〜3分）。失敗したらエディタを閉じて Visual Studio から `Development Editor` / `Win64` でビルドする |
| ビルドは通るがエディタでクラッシュ | `Binaries/` `Intermediate/` を削除して project files を再生成 |
| `.sln` が Git の変更に出てくる | 出ないはず。出るなら `.gitignore` が効いていない。`git rm --cached ToonStory.sln` |
| 画像を追加したのに `git status` に出ない | `Content/` の外に置いている。`Content/` の下に移動する。`docs/images/` だけは例外的に追跡される |
