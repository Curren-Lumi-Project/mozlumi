<p align="center">
  <img src="src/data/images/icon.svg" width="112" height="112" alt="Mozlumi icon">
</p>

# Mozlumi（もずるみ）

[日本語](#日本語) | [English](#english)

---

## 日本語

Mozlumi（もずるみ）は、[Mozkey](https://github.com/koyasi777/mozkey)（[google/mozc](https://github.com/google/mozc) をベースにした非公式フォーク）をさらにフォークした、macOS 用の非公式の日本語入力です。Curren Lumi Project（カレン ルミリア）が管理しています。Mozkey および Google とは関係ありません。

ダウンロードと使い方は、カレンラボの [Mozlumi（もずるみ）ダウンロード＆使い方](https://note.curren-lumi.blog/mozlumi-download/) にまとめています。

> **外部との通信について**: 通信するのは、アップデートの確認とダウンロード（GitHub）のときだけです。AI変換を含め、変換はすべて Mac の中で行い、入力した文字や学習データを外部に送ることはありません。

### 主な機能

- **ローマ字の誤入力補正**: 隣のキーを押した、キーが入れ替わった、余分に押した・抜けた、長音を「oo」「ee」と打った、といった打ち間違いを推測して、直した候補を出します。Space で変換したときは、文全体を直した候補を「補正」の印付きで 2 番目以降に加えます（1 番目の変換は変わりません）
- **クセの学習**: 選んだ補正の打ち間違い方や、変換したあと確定する前に打ち直した直し方を覚え、次から出しやすくします
- **AI変換（Zenz）**: Space で変換すると、Mac の中で動く AI が同じ文を変換し、違う部分があれば「AI」の印付きで候補に加えます。Option+Tab で、文全体を AI で変換し直せます
- **AI による補正の見直し**: 誤入力補正の候補を、AI が前の文の流れから見直し、合わないものを外したり、足したりします（「AI補正」の印）。前の文を返さないアプリでは、直前に確定した文を使います（メモリの中だけに置き、保存しません）
- **入力履歴からの文のサジェスト**: 確定した文（句読点で終わる文も）を覚えて、次に打ち始めたときに候補に出します

| 入力 | 補正候補の例 |
|---|---|
| `sunkai` | 深海 |
| `gakkkou` | 学校 |
| `kkaisya` | 会社 |
| `syooryuukenwoutu` | 昇竜拳を打つ |

誤入力補正は設定の「サジェスト」タブ、AI変換は「入力補助」タブでオン・オフできます（どちらも初期設定でオン）。

Mozkey との違い: ライブ変換と Zenz ライブ補正は使いません。句読点を打ったときの自動確定は、初期設定でオフです。Mozkey 本体の説明は [docs/README_mozkey.md](docs/README_mozkey.md) にあります。

### インストール

[Releases](https://github.com/Curren-Lumi-Project/mozlumi/releases) から `Mozlumi_v…_macos_universal.pkg` をダウンロードして開いてください。

- PKG は未署名です。初回は「システム設定」→「プライバシーとセキュリティ」から「このまま開く」で開いてください
- 初めてインストールしたときだけ、最後にログアウトが必要です
- Mozkey とは別のアプリとして入るので、並べて使えます。学習データ・設定も別です（`~/Library/Application Support/Mozlumi`）
- 動作を確認しているのは Apple Silicon の Mac です

### アップデート

1 日 1 回 GitHub を確認し、新しい版があれば知らせます。入力メニューの「アップデートを確認...」からも確認できます。更新は管理者のパスワードを入れるだけで、ログアウトは不要です。毎日の確認は、設定の「プライバシー」タブで止められます。

### ビルド（macOS）

必要なもの（Xcode、Bazelisk など）は [docs/build_mozc_in_osx.md](docs/build_mozc_in_osx.md) を参照してください。

```bash
cd src
bazelisk build --config=oss_macos --config=release_build \
  --macos_cpus=x86_64,arm64 --define=MOZKEY_INSTALLABLE=1 //mac:package
```

成果物: `src/bazel-bin/mac/Mozc.pkg`

### ライセンス・クレジット

Mozc・Mozkey と同じく 3-Clause BSD License です（サードパーティのコードを除く）。詳しくは [LICENSE](LICENSE)、[AUTHORS](AUTHORS)、[CONTRIBUTORS](CONTRIBUTORS)、[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を参照してください。

- [Mozc](https://github.com/google/mozc) — Google
- [Mozkey](https://github.com/koyasi777/mozkey) — koyasi777
- AI のモデル [zenz-v3.2-small-gguf](https://huggingface.co/Miwa-Keita/zenz-v3.2-small-gguf) — Miwa-Keita（CC BY-SA 4.0）

---

## English

Mozlumi is an unofficial Japanese input method for macOS, forked from [Mozkey](https://github.com/koyasi777/mozkey), which is itself an unofficial fork of [google/mozc](https://github.com/google/mozc). It is maintained by Curren Lumi Project (Curren Lumilia) and is not affiliated with Mozkey or Google.

Download and usage guide (in Japanese): [Mozlumi on カレンラボ](https://note.curren-lumi.blog/mozlumi-download/)

> **Network access**: Mozlumi uses the network only to check for and download updates (GitHub). All conversion, including AI conversion, runs on your Mac; what you type and the learning data are never sent anywhere.

### Features

- **Romaji typing correction**: guesses typos such as a neighboring key, swapped keys, an extra or missed key, and long vowels typed as "oo" or "ee", and offers corrected candidates. On Space conversion, whole-sentence fixes are added from the second candidate on, marked 「補正」 (the first conversion is not changed)
- **Learning your habits**: the kinds of typos you pick corrections for, and fixes you make by retyping after converting and before committing, are learned and offered more readily next time
- **AI conversion (Zenz)**: on Space conversion, an AI running on your Mac converts the same text and, where it differs, adds its result marked "AI". Option+Tab reconverts the whole sentence with the AI
- **AI review of corrections**: the AI reviews typing corrections with the preceding text, removing those that do not fit and adding others (marked 「AI補正」). In apps that do not provide the preceding text, the text committed just before is used (kept only in memory, never saved)
- **Sentence suggestions from input history**: committed sentences (including those ending with punctuation) are offered when you start typing them again

| Input | Example correction |
|---|---|
| `sunkai` | 深海 |
| `gakkkou` | 学校 |
| `kkaisya` | 会社 |
| `syooryuukenwoutu` | 昇竜拳を打つ |

Typing correction is on the Suggest tab of the settings, and AI conversion on the Advanced tab (「入力補助」). Both are on by default.

Differences from Mozkey: live conversion and Zenz live correction are not used, and committing on punctuation is off by default. For Mozkey itself, see [docs/README_mozkey.md](docs/README_mozkey.md).

### Installation

Download `Mozlumi_v…_macos_universal.pkg` from [Releases](https://github.com/Curren-Lumi-Project/mozlumi/releases) and open it.

- The PKG is not signed. The first time, open it with "Open Anyway" in System Settings → Privacy & Security
- Only a first installation asks you to log out at the end
- Mozlumi is installed as a separate app from Mozkey, so both can be used. Learning data and settings are separate too (`~/Library/Application Support/Mozlumi`)
- Tested on Apple Silicon Macs

### Updates

Mozlumi checks GitHub once a day and lets you know when a new version is available. You can also check from "Check for Updates..." in the input menu. Updating only needs an administrator password, with no logout. The daily check can be turned off on the Privacy tab of the settings.

### Build (macOS)

For requirements (Xcode, Bazelisk, and so on), see [docs/build_mozc_in_osx.md](docs/build_mozc_in_osx.md).

```bash
cd src
bazelisk build --config=oss_macos --config=release_build \
  --macos_cpus=x86_64,arm64 --define=MOZKEY_INSTALLABLE=1 //mac:package
```

Output: `src/bazel-bin/mac/Mozc.pkg`

### License and credits

3-Clause BSD License, like Mozc and Mozkey (except third-party code). See [LICENSE](LICENSE), [AUTHORS](AUTHORS), [CONTRIBUTORS](CONTRIBUTORS) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

- [Mozc](https://github.com/google/mozc) — Google
- [Mozkey](https://github.com/koyasi777/mozkey) — koyasi777
- AI model [zenz-v3.2-small-gguf](https://huggingface.co/Miwa-Keita/zenz-v3.2-small-gguf) — Miwa-Keita (CC BY-SA 4.0)
