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

clone する**前**に 1 回（マシンごと）、clone した後に 5 行（clone ごと）。

### clone する前に（マシンごとに 1 回）

```bash
git config --global core.longpaths true
```

Windows の既定ではパスが 260 文字を超えると失敗する。UE はアセット名とフォルダが深くなりやすく、
プロジェクトを深い場所（`Documents/GitHub/...` など）に置くと届きうる。現状このリポジトリの最長パスは
94 文字なので今すぐ問題になるわけではないが、アセットが増えてから踏むと `Filename too long` で
clone が途中で止まるので、先に入れておく。

### clone した後に（clone ごとに毎回）

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

下の 5 行は clone ごとのローカル設定なので、**環境を作り直すたびに実行する**。`core.longpaths` はマシン全体の設定なので 1 回でよい。

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

**コミットは 2 回、push は 1 回。**

```bash
# 1. Content 側でコミット（push はしなくてよい）
git -C Content add -A
git -C Content commit -m "キャラクターのマテリアルを調整"


# 2. 親側でコミットして push（Content も一緒に push される）
git add Content
git commit -m "Update content"
git push
```

`git add Content` でコミットされるのは**アセットの中身ではなく「Content リポジトリのどのコミットを使うか」という参照 1 行だけ**（gitlink と呼ぶ）。だから親側の履歴は軽いまま保たれる。

**なぜコミットだけ 2 回なのか。** git には submodule をまたいでコミットする機能が無い。`git add` は submodule の中まで降りていかないので、Content の中身は Content 側でコミットするしかない。

**push が 1 回で済む理由。** 「2. 初回だけ必ずやる設定」の `push.recurseSubmodules on-demand` により、親を push するとき git が「この親コミットが指す Content のコミットはリモートにあるか」を確認し、無ければ先に Content を push する。実行すると次のように出る:

```
Pushing submodule 'Content'
   e12a4c0..45441d6  main -> main      ← Content
   37b1dc8..483ff27  main -> main      ← 親
```

### コードだけ変更したとき

普通に `git add` / `git commit` / `git push`。特別なことは要らない。

### やりがちな失敗: 手順 2 の `git add Content` を忘れる

アセットを Content 側でコミットしただけで満足して、親側で `git add Content` をしないまま `git push` すると:

```
Everything up-to-date
```

で終わる。親に新しいコミットが無いので push するものが無く、**Content の新コミットもリモートに送られない。**

エラーは一切出ない。手元のエディタでは変更が見えているので、「え、入ってないよ?」と言われるまで気づかない。**アセットを変えたら必ず親側もコミットする。**

逆の「Content を push せず親だけ push」で相手の `git submodule update` が壊れるパターンは、`push.recurseSubmodules on-demand` が自動で防ぐので気にしなくてよい。

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
- **`Content` ではブランチを切らない。`main` 一本で運用する**（理由は「7. ブランチ運用」）

---

## 7. ブランチ運用

### 基本

- `main` は**常にビルドが通る状態**を保つ。壊れたものを push しない
- 作業は必ず feature ブランチを切る
- feature ブランチは **1〜3 日で消す**。長生きさせるほどアセットを巻き添えにするリスクが上がる
- **`Content`（submodule）ではブランチを切らない。`main` 一本で運用する**

### ブランチ名

```
feature/<YYYYMMDD>_<PascalCase の内容>
```

例:

```
feature/20260903_DashMove
feature/20260905_DamageSystem
feature/20260910_LockOnCamera
```

日付は着手日。時系列で並ぶので、消し忘れたブランチが一目で分かる。

```bash
git checkout main
git pull
git checkout -b feature/20260903_DashMove
# …作業…
git push -u origin feature/20260903_DashMove
```

GitHub で Pull Request を出す。3 人なら誰か 1 人が見て merge で十分。merge 後はブランチを削除する。

### なぜ Content でブランチを切らないのか

親がどのブランチにいても、アセットのコミットは常に Content の `main` に積まれる。その結果 Content の履歴が一直線に保たれ、**親をマージするとき gitlink が自動解決される**:

```
$ git merge feature/20260903_DashMove
Note: Fast-forwarding submodule Content to 4df8a25
Merge made by the 'ort' strategy.
```

2 人が別のブランチで別のアセットを追加していても、コンフリクトせず両方残る。

逆に Content でブランチを切ると履歴が分岐し、gitlink がコンフリクトする。`.uasset` はマージできないので、手作業でどちらかを捨てるしかなくなる。**Content は `main` 一本。**

### ブランチを切り替えたら Content を確認する

確認は親側から 1 コマンド。

```bash
git submodule status
```

```
 1fe4a6e1fd82192f7fe5e087ba873972ff3eff46 Content (heads/main)
↑                                                  ↑
先頭スペース = 親の記録と一致            heads/main = ブランチに乗っている
```

| 見え方 | 意味 |
| --- | --- |
| 先頭が半角スペース | 親の記録と一致。正常 |
| 先頭が `+` | 親の記録とズレている |
| 先頭が `-` | 未初期化。`git submodule update --init --recursive` |
| 末尾が `(heads/main)` | `main` に乗っている。正常 |
| 末尾がハッシュ | **detached HEAD**。下の対処へ |

gitlink が異なるブランチへ切り替えると、Content が **detached HEAD** になる:

```
$ git checkout main
$ git -C Content status
HEAD detached at 6a7267d
```

この状態でアセットをコミットすると、どのブランチにも乗らず迷子になる。**アセットを触る前に必ず:**

```bash
git -C Content checkout main
```

gitlink が同じブランチ間の切り替え（`git checkout -b` の直後など）では detach しないので、毎回起きるわけではない。だから余計に忘れやすい。

**副作用を理解しておくこと。** `git -C Content checkout main` は Content を「親が記録しているコミット」ではなく「Content の main の先端」へ動かす。つまり**古い親ブランチに切り替えてからこれを実行すると、親が記録しているアセットの状態と、実際に手元にあるアセットがズレる**。ビルドは通るのに参照が合わない、という分かりにくい壊れ方をしうる。

運用としては `main` 一本で正しいので手順は変えなくてよいが、**「親ブランチを切り替えても、Content は常に最新の main になる。過去のアセット状態には戻らない」**と覚えておく。ある時点のアセットを正確に再現したいときは `git -C Content checkout main` をせず、`git submodule update` で親の記録どおりに戻す。

### Pull Request で見えないもの

Content の変更は gitlink 1 行の差分（`Content @ 21c94cc → 4df8a25`）としか表示されない。**アセットの中身は PR 上でレビューできない。** submodule の構造的な限界なので、アセットのレビューが必要ならエディタ上で見せ合う。

---

## 8. GUI クライアントで使う場合

CLI でなくても構わないが、**submodule を扱えるクライアントを選ぶこと。**

### VS Code

`git.detectSubmodules`（既定 ON）により、submodule を**別リポジトリとして自動検出**する。

1. `Ctrl+Shift+G` でソース管理ビューを開くと、`TOON-STORY` と `CONTENT` が別セクションで並ぶ
   - 表示されない場合はビュー右上の `…` →「ビュー」→ **Source Control Repositories** を有効化
2. アセットを変更したら **CONTENT** セクションでステージ → **コミット**（プッシュはまだしなくてよい）
3. その後 **TOON-STORY** に `Content` が変更として現れるので、ステージ → コミット → **プッシュ**。VS Code も内部で git を呼ぶので、`push.recurseSubmodules on-demand` が効いて Content も一緒に push される
4. 左下ステータスバーのブランチ表示は「今アクティブなリポジトリ」のもの。クリックで切り替わる

### JetBrains Rider

submodule を正式サポートしている。Commit ツールウィンドウで親と Content の変更が階層表示され、Push ダイアログに両方が並ぶ。UE 開発なら一番扱いやすい。

### Fork / SourceTree / TortoiseGit

いずれも submodule 対応。Content を別リポジトリとして開いて操作する。

### GitHub Desktop

**submodule をまともに扱えないので使わないこと。** Content の変更がコミットできず、gitlink の更新も表示されない。

---

## 9. トラブルシュート

| 症状 | 原因と対処 |
| --- | --- |
| `Content/` が空。エディタが起動しない | `git submodule update --init --recursive`。それでも空なら `toon-story-content` へのアクセス権が無い |
| 他の人の環境で `fatal: reference is not a tree` / `Fetched in submodule path 'Content', but it did not contain <sha>` が出た | 誰かが Content を push せずに親を push した。**submodule 運用で一番起きる事故。** その人が `git -C Content push` すれば直る |
| Content で「HEAD detached」と出る | `git -C Content checkout main && git -C Content pull` |
| Content でコミットしたのに GitHub に出ない | detached HEAD でコミットした。`git -C Content log` で SHA を控え、`git -C Content checkout main` してから `git -C Content cherry-pick <sha>` |
| エディタが「アセットが見つからない」と言う | Content が古い。エディタを閉じて `git pull` |
| 初回起動で「The following modules are missing or built with a different engine version」と出る | 正常。`Binaries/` は Git 管理外なので、clone 後の初回だけコンパイルが必要。**Yes** を押す。失敗したらエディタを閉じて Visual Studio から `Development Editor` / `Win64` でビルドする |
| `Plugins/VisualStudioTools/` が勝手に増える | Visual Studio の UE 連携が入れてくる。`.gitignore` 済みなのでコミットされない。使わないならフォルダごと削除してよい。`ToonStory.uproject` に行が追加されていたら、それはコミットしない |
| ビルドは通るがエディタでクラッシュ | `Binaries/` `Intermediate/` を削除して project files を再生成 |
| `.sln` が Git の変更に出てくる | 出ないはず。出るなら `.gitignore` が効いていない。`git rm --cached ToonStory.sln` |
| 画像を追加したのに `git status` に出ない | `Content/` の外に置いている。`Content/` の下に移動する。`docs/images/` だけは例外的に追跡される |
