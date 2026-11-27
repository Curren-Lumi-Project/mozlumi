# Mozlumi の不具合の記録（デバッグモード）

Mozlumi 1.3.4 から、Mac 版の環境設定の「プライバシー」に「不具合を調べるための記録を残す（デバッグモード）」があります（初期値はオフ）。オンにすると、変換候補の枠の位置と AI の判定の記録を `~/Library/Logs/Mozlumi` に残します。入力した文字そのものは残しません。

入力メニューの「不具合の記録を書き出す...」で、記録をまとめて開発者の公開鍵で暗号化し、デスクトップに `Mozlumi_debug_log_日付_時刻.mozlumilog` として保存します。Mozlumi はこのファイルを送信しません。ユーザーがメールや DM で送ります。

暗号は Security フレームワークの ECIES（P-256、AES-GCM、`kSecKeyAlgorithmECIESEncryptionCofactorVariableIVX963SHA256AESGCM`）です。読めるのは開発者の秘密鍵だけです。

## 開発者向け

- 鍵を作る（最初の 1 回だけ）: `swift keygen.swift <秘密鍵のファイル> src/mac/mozlumi_debug_public_key.h`
  - 秘密鍵はリポジトリに入れない。バックアップを取っておく（なくすと、書き出された記録を読めない）
- 読む: `swift decrypt.swift <秘密鍵のファイル> <ファイル.mozlumilog> <出力.txt>`
