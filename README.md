<p align="center">
  <img src="src/data/images/icon.svg" width="112" height="112" alt="Mozlumi icon">
</p>

# Mozlumi（もずるみ）

[日本語](#日本語) | [English](#english)

---

## 日本語

Mozlumi（もずるみ）は、[Mozkey](https://github.com/koyasi777/mozkey)（[google/mozc](https://github.com/google/mozc) をベースにした非公式フォーク）をさらにフォークした、非公式の日本語入力システムです。Curren Lumi Project（カレン ルミリア）が管理しています。Mozkey および Google とは関係ありません。

ダウンロードと使い方は、カレンラボの [Mozlumi（もずるみ）ダウンロード＆使い方](https://note.curren-lumi.blog/mozlumi-download/) にまとめています。

Mozkey の機能を引き継いでいますが、次の点が異なります。Mozkey 本体の説明は [docs/README_mozkey.md](docs/README_mozkey.md) を参照してください。

- ライブ変換と Zenz（AI）ライブ補正は使いません（常にオフで、設定画面からも外しています）
- 句読点（「、」「。」など）を打ったときの自動確定は、初期設定でオフです（設定画面でオンにできます）

> **外部との通信について**: Mozlumi が外部と通信するのは、アップデートの確認とダウンロード（GitHub）のときだけです。AI変換を含め、変換はすべて Mac の中で行い、入力した文字・変換結果・学習データを外部に送ることはありません。

### Mozlumi で追加した機能

#### ローマ字の誤入力補正

ローマ字入力の 1 打鍵の打ち間違いを推測し、補正した候補をサジェスト・予測候補に加えます。

| 入力 | 補正候補の例 |
|---|---|
| `sunkai` | 深海（しんかい） |
| `kkaisya` | 会社 |
| `kqisya` | 会社、記者 |
| `denhsa` | 電車 |
| `gakkkou` | 学校 |
| `syooryuuken` | 昇竜拳（長音を「oo」と打った） |

- 対象とする打ち間違い: 隣のキーを押した、隣り合う 2 キーが入れ替わった、余分に押した（二度押しを含む）、1 文字抜けた、長音を聞こえたとおりに打った（「ou」を「oo」、「ei」を「ee」、またはその逆）
- 補正候補は辞書に実在する読みだけで、最大 3 件です。打ち間違いの起こりやすさと語の使用頻度で並べ、通常の候補より順位を下げて表示します
- 打った読みがそのまま辞書にある場合や、よく使う語の書きかけと判断した場合は補正しません
- 動作するのは、ローマ字入力・ひらがなモードです。打っている途中のサジェストと予測候補（Tab）に出ます
- 打ち間違えたまま Space で変換したときも、2 番目以降の候補に「補正」の印付きで補正候補を並べます（1 番目の変換結果は変わりません）
- 文の途中の打ち間違い（例: 「さかなをたべる」を `sakanaeotaberu` と打った）は、Space 変換のときに、打ち間違いを直した読み（2 か所まで）を文全体として変換し直して比べます。打ったままより十分に自然な変換になるものを、2 番目以降の候補に「補正」の印付きで最大 3 件加え、変換中のバッジを紫の「補正あり」にします（1 番目の変換結果は変わりません）
- 入力の最後に、かなにならないローマ字が 2 文字以上残ったとき（例: `yokuyary` →「よくやry」）は、そこだけを直した変換を 1 番目にします（打ったままの文字は 2 番目）
- 予測候補（Tab）と Space 変換では、8 文字以上の入力で 1 文字の補正が見つからないとき、2 か所までの打ち間違いも探します（打っている途中のサジェストでは探しません）
- 補正候補を選んで確定すると、その打ち間違いの種類（例: a のつもりで q を押した）を覚え、次から同じ間違い方の補正を上位に出します。覚えるのは間違い方の種類と回数だけで、打った文章は保存しません
- Space で変換したあと、確定する前に 1〜2 キーだけ打ち直して確定すると、その直し方を覚えます（例: `issunn` で「一寸」が出たので `issyunn` と打ち直して「一瞬」を確定）。次から同じ打ち方をしたときは、打ったままの変換が自然に見えても、直した変換を 2 番目に「補正」の印付きで出します。覚えるのは、打ち直した部分とその前後 3 キー分のローマ字と回数だけです（最大 200 件、Mac の中に保存）
- AI変換（Zenz）もオンのときは、Space 変換で見つかった補正の候補を、Zenz が前の文の流れをふまえて見直します。流れに合わない補正は外し、Mozc が出さなかった補正を加えることもあります（「AI補正」の印）
- 前の文は、入力中のアプリから読み取ります。LINE のように前の文を返さないアプリでは、Mozlumi が直前に確定した文（64 文字まで）を代わりに使います。これはメモリの中だけに置き、ファイルには保存しません。5 分打たなかったとき、カーソルを動かしたとき、パスワード欄では使いません
- 初期設定で有効です。設定画面の「サジェスト」タブにある「ローマ字の打ち間違いを補正する（誤入力補正）」でオン・オフできます

辞書の語に人工的な打ち間違いを入れた評価（調整に使っていない検証用データ、同梱辞書）では、補正候補 3 件以内に目的の語が入った割合は 94.5%、正しく打っている途中で補正候補が出た割合は 0.8% でした。並び順は簡易的な再現による数値で、実際の表示とは異なる場合があります。

#### 入力履歴からの文のサジェスト

Space で変換して確定した文（2 文節以上）を覚えて、次に打ち始めたときにサジェスト・予測候補に出します（Mozc の入力履歴からのサジェストを調整しています）。

- 「。」「？」などの句読点で終わる文は、Mozc では覚えませんでしたが、Mozlumi では句読点を除いて覚えます（例: 「今日はいい天気で雨は降りません。」→「今日はいい天気で雨は降りません」）
- 同時に覚えた短い文節（「今日は」など）に押し出されないよう、覚えた文を 1 件は必ず候補に残します

#### AI変換（Zenz）

Space で変換すると、ローカルの AI（Zenz）が裏で同じ文を変換し、通常の変換と違う部分があれば「AI」の印付きで 2 番目の候補に加えます。通常の変換結果は変わらないので、AI が外れても入力の邪魔になりません。

- AI の変換が通常の変換と違うときは、変換中のバッジがオレンジの「AI候補あり」になります。確定する前に候補を見てみてください
- 設定の「AI の変換が違うときは、AI の候補を 1 番目にする」をオンにすると、AI の答えが最初から選ばれた状態になります（バッジは「AI」）。AI が外れたときもそのまま確定されるので、初期設定はオフです

打っている途中や変換中に **Option+Tab** を押すと、AI が前後の文脈をふまえて文全体を変換し直します。

- もう一度 Option+Tab を押すか Space を押すと、元の変換に戻ります
- AI 版を採用したか元に戻したかを記録し、次からの変換に反映します（記録は Mac の中だけに保存されます）
- AI はすべて Mac の中で動きます（llama.cpp）。入力内容を外部に送ることはありません
- 初期設定で有効です。設定画面の「入力補助」タブにある「AI変換」でオン・オフできます
- AI のモデル（[zenz-v3.2-small-gguf](https://huggingface.co/Miwa-Keita/zenz-v3.2-small-gguf)、作者 Miwa-Keita）は CC BY-SA 4.0 です

### 動作環境

- macOS（Universal PKG）。動作を確認しているのは Apple Silicon の Mac です。Intel Mac では確認していません
- Windows・Linux では確認していません

### インストール

[Releases](https://github.com/Curren-Lumi-Project/mozlumi/releases) から `Mozlumi_v…_macos_universal.pkg` をダウンロードしてインストールしてください。ソースからビルドすることもできます。

- PKG は未署名です。初回は「システム設定」→「プライバシーとセキュリティ」から「このまま開く」で開いてください
- インストールが終わると、Mozlumi は入力ソースに追加されます。初めてインストールしたときはログアウトが必要です（ログインし直すと入力メニューに出ます）。アップデートでは、ログアウトや再起動は必要ありません
- インストール直後の最初の変換と、AI 変換を初めて使うときは、準備のため少し時間がかかることがあります
- Mozkey・Mozc とは別のアプリとしてインストールされます（`/Library/Input Methods/Mozlumi.app`）。Mozkey と並べて使えます
- 学習データ・設定は Mozkey とは別に保存されます（`~/Library/Application Support/Mozlumi`）。Mozkey からの引き継ぎはありません

### アップデート

- 新しい版があるかを 1 日 1 回 GitHub に確認し、あれば知らせます。「更新する」を選ぶと、PKG をダウンロードして SHA-256 を確かめ、管理者のパスワードを入力するだけで入れ替えます（ログアウトは不要です）
- 入力メニューの「アップデートを確認...」からいつでも確認できます
- 毎日の確認は、設定の「プライバシー」タブの「Mozlumi のアップデートを1日1回確認する」で止められます
- 通信するのはアップデートの確認とダウンロードのときだけで、入力処理（変換・AI変換）は外部と通信しません。確認のときも、入力内容や学習データは送りません

### ビルド（macOS）

必要なもの（Xcode、Bazelisk など）は [docs/build_mozc_in_osx.md](docs/build_mozc_in_osx.md) を参照してください。

```bash
cd src
bazelisk build \
  --config=oss_macos \
  --config=release_build \
  --macos_cpus=x86_64,arm64 \
  --define=MOZKEY_INSTALLABLE=1 \
  //mac:package
```

成果物: `src/bazel-bin/mac/Mozc.pkg`

誤入力補正のテスト:

```bash
cd src
bazelisk test --config=oss_macos -c dbg \
  //prediction:roman_typo_candidates_test \
  //prediction:roman_typo_corrector_test
```

### ライセンス

Mozlumi は Mozc・Mozkey と同じく 3-Clause BSD License です（サードパーティのコードを除く）。著作権表示とライセンス情報は [LICENSE](LICENSE)、[AUTHORS](AUTHORS)、[CONTRIBUTORS](CONTRIBUTORS)、[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) に残しています。

### クレジット

- [Mozc](https://github.com/google/mozc) — Google
- [Mozkey](https://github.com/koyasi777/mozkey) — koyasi777

---

## English

Mozlumi is an unofficial Japanese input method forked from [Mozkey](https://github.com/koyasi777/mozkey), which is itself an unofficial fork of [google/mozc](https://github.com/google/mozc). It is maintained by Curren Lumi Project (Curren Lumilia). Mozlumi is not affiliated with Mozkey or Google.

Download and usage guide (in Japanese): [Mozlumi on カレンラボ](https://note.curren-lumi.blog/mozlumi-download/)

Mozkey features are kept, with these differences. For Mozkey itself, see [docs/README_mozkey.md](docs/README_mozkey.md).

- Live conversion and Zenz (AI) live correction are not used (always off and removed from the settings)
- Committing on punctuation (such as 、 and 。) is off by default (it can be turned on in the settings)

> **Network access**: Mozlumi uses the network only to check for and download updates (GitHub). All conversion, including AI conversion, runs on your Mac; what you type, the conversion results and the learning data are never sent anywhere.

### Added in Mozlumi

#### Romaji typing correction

Mozlumi guesses one-keystroke typos in romaji input and adds corrected candidates to suggestions and predictions.

| Input | Example correction |
|---|---|
| `sunkai` | 深海 (しんかい) |
| `kkaisya` | 会社 |
| `kqisya` | 会社, 記者 |
| `denhsa` | 電車 |
| `gakkkou` | 学校 |
| `syooryuuken` | 昇竜拳 (a long vowel typed as "oo") |

- Covered typos: a neighboring key, two adjacent keys swapped, an extra key (including a doubled key), a missed key, and a long vowel spelled as it sounds ("oo" for "ou", "ee" for "ei", or the other way around)
- Only readings that exist in the dictionary are used, up to 3. They are ranked by how likely the typo is and how common the word is, and are shown below regular candidates
- No correction when the typed reading is already a word, or when it looks like the beginning of a common word
- Works in romaji input, hiragana mode, for suggestions while typing and predictions (Tab)
- When you convert a mistyped reading with Space, corrected candidates marked 「補正」 are added from the second candidate on (the first conversion result is not changed)
- A typo inside a phrase (e.g. `sakanaeotaberu` for 「さかなをたべる」) is checked on Space conversion by converting fixes of up to two keys as whole sentences. Up to three fixes whose conversion is much more natural are added from the second candidate on, marked 「補正」, and the conversion status badge turns purple and shows 「補正あり」 (the first conversion result is not changed)
- When two or more letters that cannot become kana are left at the end (e.g. `yokuyary` → 「よくやry」), the conversion with only those letters fixed comes first (the typed text second)
- On predictions (Tab) and Space conversion, typos at two places are also searched for input of 8 or more letters when no one-place correction is found (not on suggestions while typing)
- When you commit a corrected candidate, the kind of typo (e.g. pressing q for a) is learned, and corrections of the same kind rank higher next time. Only the kinds of typos and their counts are stored, not the text you typed
- When you convert with Space and then fix one or two keys before committing (e.g. 「一寸」 for `issunn`, retyped as `issyunn` and committed as 「一瞬」), the fix is learned. The next time you type the same way, the fixed conversion is offered second, marked 「補正」, even if the typed text converts naturally. Only the retyped part with three keys of romaji around it and a count are stored (up to 200, on your Mac)
- When AI conversion (Zenz) is also on, Zenz reviews the corrected candidates found on Space conversion with the preceding text. It removes those that do not fit and may add one that Mozc did not offer (marked 「AI補正」)
- The preceding text is read from the app you are typing in. In apps that do not provide it, such as LINE, the text Mozlumi committed just before (up to 64 characters) is used instead. It is kept only in memory and never saved to a file, and is not used after 5 minutes without typing, after the cursor moves, or in password fields
- Enabled by default. It can be turned on or off on the Suggest tab of the settings (the checkbox label is in Japanese: 「ローマ字の打ち間違いを補正する（誤入力補正）」)

In an evaluation with synthetic typos of dictionary words (held-out data, bundled dictionaries), the intended word was among the corrected candidates (up to 3) in 94.5% of cases, and corrections appeared while typing correctly in 0.8% of cases. The ranking was simplified in the evaluation, so actual results may differ.

#### Sentence suggestions from input history

Sentences of two or more segments that you convert with Space and commit are remembered and offered in suggestions and predictions when you start typing them again (an adjustment of Mozc's input history suggestions).

- Sentences ending with punctuation such as 「。」 or 「？」, which Mozc did not remember, are remembered without the punctuation
- At least one remembered sentence is always kept in the list, so that it is not pushed out by its own shorter segments (such as 「今日は」)

#### AI conversion (Zenz)

When you convert with Space, the local AI (Zenz) converts the same text in the background and, where its result differs, adds it as the second candidate marked "AI". The normal conversion is not changed, so a wrong AI result does not get in your way.

- When the AI conversion differs, the conversion status badge turns orange and shows 「AI候補あり」 (AI candidate available), so you can check before committing
- With the setting 「AI の変換が違うときは、AI の候補を 1 番目にする」 turned on, the AI result is selected from the start (the badge shows "AI"). A wrong AI result is then committed as well, so it is off by default

While typing or converting, press **Option+Tab** to have the AI reconvert the whole sentence using the surrounding context.

- Press Option+Tab again or Space to go back to the original conversion
- Whether you keep or revert the AI result is recorded and used for later conversions (stored only on your Mac)
- The AI runs entirely on your Mac (llama.cpp). Input is not sent to external servers
- Enabled by default. It can be turned on or off under "AI変換" on the Advanced tab (「入力補助」) of the settings
- The AI model ([zenz-v3.2-small-gguf](https://huggingface.co/Miwa-Keita/zenz-v3.2-small-gguf) by Miwa-Keita) is licensed under CC BY-SA 4.0

### Platforms

- macOS (Universal PKG). Tested on Apple Silicon Macs; not tested on Intel Macs
- Not tested on Windows or Linux

### Installation

Download `Mozlumi_v…_macos_universal.pkg` from [Releases](https://github.com/Curren-Lumi-Project/mozlumi/releases) and install it. You can also build from source.

- The PKG is not signed. On first launch, open it from System Settings → Privacy & Security → Open Anyway
- When the installation finishes, Mozlumi is added to your input sources. A first installation needs a logout (Mozlumi appears in the input menu after you log in again). Updates need no logout or restart
- The first conversion after installing, and the first AI conversion, may take a moment while Mozlumi gets ready
- Mozlumi is installed as a separate app from Mozkey and Mozc (`/Library/Input Methods/Mozlumi.app`), so it can be used side by side with Mozkey
- Learning data and settings are stored separately from Mozkey (`~/Library/Application Support/Mozlumi`). Nothing is migrated from Mozkey

### Updates

- Once a day, Mozlumi checks GitHub for a new version and lets you know. If you choose to update, it downloads the PKG, verifies its SHA-256, and replaces Mozlumi after you enter an administrator password (no logout needed)
- You can also check at any time from "Check for Updates..." in the input menu
- The daily check can be turned off on the Privacy tab of the settings (the checkbox label is in Japanese: 「Mozlumi のアップデートを1日1回確認する」)
- Only the update check and download use the network; input processing (conversion and AI conversion) never does. The update check does not send what you type or any learning data

### Build (macOS)

See [docs/build_mozc_in_osx.md](docs/build_mozc_in_osx.md) for requirements (Xcode, Bazelisk, etc.).

```bash
cd src
bazelisk build \
  --config=oss_macos \
  --config=release_build \
  --macos_cpus=x86_64,arm64 \
  --define=MOZKEY_INSTALLABLE=1 \
  //mac:package
```

Output: `src/bazel-bin/mac/Mozc.pkg`

Tests for typing correction:

```bash
cd src
bazelisk test --config=oss_macos -c dbg \
  //prediction:roman_typo_candidates_test \
  //prediction:roman_typo_corrector_test
```

### License

Like Mozc and Mozkey, Mozlumi is licensed under the 3-Clause BSD License (except for third-party code). Copyright and license information is kept in [LICENSE](LICENSE), [AUTHORS](AUTHORS), [CONTRIBUTORS](CONTRIBUTORS), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

### Credits

- [Mozc](https://github.com/google/mozc) — Google
- [Mozkey](https://github.com/koyasi777/mozkey) — koyasi777
