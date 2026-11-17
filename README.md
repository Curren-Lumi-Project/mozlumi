<p align="center">
  <img src="src/data/images/icon.svg" width="112" height="112" alt="Mozlumi icon">
</p>

# Mozlumi（もずるみ）

[日本語](#日本語) | [English](#english)

---

## 日本語

Mozlumi（もずるみ）は、[Mozkey](https://github.com/koyasi777/mozkey)（[google/mozc](https://github.com/google/mozc) をベースにした非公式フォーク）をさらにフォークした、非公式の日本語入力システムです。Curren-Lumi-Project（かれるみ）が管理しています。Mozkey および Google とは関係ありません。

Mozkey の機能を引き継いでいますが、次の点が異なります。Mozkey 本体の説明は [docs/README_mozkey.md](docs/README_mozkey.md) を参照してください。

- ライブ変換と Zenz（AI）ライブ補正は使いません（常にオフで、設定画面からも外しています）
- 句読点（「、」「。」など）を打ったときの自動確定は、初期設定でオフです（設定画面でオンにできます）

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

- 対象とする打ち間違い: 隣のキーを押した、隣り合う 2 キーが入れ替わった、余分に押した（二度押しを含む）、1 文字抜けた
- 補正候補は辞書に実在する読みだけで、最大 3 件です。打ち間違いの起こりやすさと語の使用頻度で並べ、通常の候補より順位を下げて表示します
- 打った読みがそのまま辞書にある場合や、よく使う語の書きかけと判断した場合は補正しません
- 動作するのは、ローマ字入力・ひらがなモードのサジェストと予測候補だけです。Space キーでの変換結果は変わりません
- 初期設定で有効です。設定画面の「サジェスト」タブにある「ローマ字の打ち間違いを補正する（誤入力補正）」でオン・オフできます

辞書の語に人工的な打ち間違いを入れた評価（調整に使っていない検証用データ、同梱辞書）では、補正候補 3 件以内に目的の語が入った割合は 94.5%、正しく打っている途中で補正候補が出た割合は 0.8% でした。並び順は簡易的な再現による数値で、実際の表示とは異なる場合があります。

#### AI変換（Zenz）

打っている途中や変換中に **Option+Tab** を押すと、ローカルの AI（Zenz）が前後の文脈をふまえて文全体を変換し直します。

- もう一度 Option+Tab を押すか Space を押すと、元の変換に戻ります
- AI 版を採用したか元に戻したかを記録し、次からの変換に反映します（記録は Mac の中だけに保存されます）
- AI はすべて Mac の中で動きます（llama.cpp）。入力内容を外部に送ることはありません
- 初期設定で有効です。設定画面の「入力補助」タブにある「AI変換」でオン・オフできます
- AI のモデル（[zenz-v3.2-small-gguf](https://huggingface.co/Miwa-Keita/zenz-v3.2-small-gguf)、作者 Miwa-Keita）は CC BY-SA 4.0 です

### 動作環境

- macOS（Apple Silicon / Intel、Universal PKG）で動作を確認しています
- Windows・Linux では確認していません

### インストール

現在、ビルド済みのパッケージは配布していません。ソースからビルドしてください。

- PKG は未署名です。初回は「システム設定」→「プライバシーとセキュリティ」から「このまま開く」で開いてください
- Mozkey・Mozc とは別のアプリとしてインストールされます（`/Library/Input Methods/Mozlumi.app`）。Mozkey と並べて使えます
- 学習データ・設定は Mozkey とは別に保存されます（`~/Library/Application Support/Mozlumi`）。Mozkey からの引き継ぎはありません

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

Mozlumi is an unofficial Japanese input method forked from [Mozkey](https://github.com/koyasi777/mozkey), which is itself an unofficial fork of [google/mozc](https://github.com/google/mozc). It is maintained by Curren-Lumi-Project (Curren Lumi). Mozlumi is not affiliated with Mozkey or Google.

Mozkey features are kept, with these differences. For Mozkey itself, see [docs/README_mozkey.md](docs/README_mozkey.md).

- Live conversion and Zenz (AI) live correction are not used (always off and removed from the settings)
- Committing on punctuation (such as 、 and 。) is off by default (it can be turned on in the settings)

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

- Covered typos: a neighboring key, two adjacent keys swapped, an extra key (including a doubled key), and a missed key
- Only readings that exist in the dictionary are used, up to 3. They are ranked by how likely the typo is and how common the word is, and are shown below regular candidates
- No correction when the typed reading is already a word, or when it looks like the beginning of a common word
- Works only for suggestions and predictions in romaji input, hiragana mode. Conversion with the Space key is not changed
- Enabled by default. It can be turned on or off on the Suggest tab of the settings (the checkbox label is in Japanese: 「ローマ字の打ち間違いを補正する（誤入力補正）」)

In an evaluation with synthetic typos of dictionary words (held-out data, bundled dictionaries), the intended word was among the corrected candidates (up to 3) in 94.5% of cases, and corrections appeared while typing correctly in 0.8% of cases. The ranking was simplified in the evaluation, so actual results may differ.

#### AI conversion (Zenz)

While typing or converting, press **Option+Tab** to have the local AI (Zenz) reconvert the whole sentence using the surrounding context.

- Press Option+Tab again or Space to go back to the original conversion
- Whether you keep or revert the AI result is recorded and used for later conversions (stored only on your Mac)
- The AI runs entirely on your Mac (llama.cpp). Input is not sent to external servers
- Enabled by default. It can be turned on or off under "AI変換" on the Advanced tab (「入力補助」) of the settings
- The AI model ([zenz-v3.2-small-gguf](https://huggingface.co/Miwa-Keita/zenz-v3.2-small-gguf) by Miwa-Keita) is licensed under CC BY-SA 4.0

### Platforms

- Tested on macOS (Apple Silicon / Intel, Universal PKG)
- Not tested on Windows or Linux

### Installation

No prebuilt packages are distributed at the moment. Please build from source.

- The PKG is not signed. On first launch, open it from System Settings → Privacy & Security → Open Anyway
- Mozlumi is installed as a separate app from Mozkey and Mozc (`/Library/Input Methods/Mozlumi.app`), so it can be used side by side with Mozkey
- Learning data and settings are stored separately from Mozkey (`~/Library/Application Support/Mozlumi`). Nothing is migrated from Mozkey

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
