# Screenshots — 対応ごとの画面の記録

画面に関わる対応（見た目・レベル・UI・アセット・PIE での動き）のスクリーンショットを、**日付別・対応内容別**に置きます。同じ画像を PR の本文にも載せます（開発ルール集（非公開） §1 の 9）。

## 置き場所と名前

```
Docs/Screenshots/<YYYY-MM-DD>/<対応内容>/
├─ README.md          … 対応内容・ブランチ・PR・画像ごとの説明（カメラの位置など）
└─ <見ているもの>.png  … 例：court_top.png、court_oblique.png、pie_first_bounce.png
```

| 項目 | 決まり |
|---|---|
| 日付 | 撮影した日（`YYYY-MM-DD`） |
| 対応内容 | ブランチ名の `/` より後ろ（例：`fix/unreal-import-nanite-and-lights` → `unreal-import-nanite-and-lights`） |
| ファイル名 | 英小文字・数字・`_`。何を見ているかが分かる名前 |
| 形式 | PNG、幅 1280 px 以下（Git LFS に入る：`.gitattributes`） |
| 撮り方 | [`Tools/Screenshots/Capture-Window.ps1`](../../Tools/Screenshots/README.md) で、エディタのウィンドウだけを撮る |

## 格納しない場合

- 画面に変化がない対応（数値の計算・ルール・テストだけ・文書だけ）
- 前に格納した画面と差が少ない場合（例：同じ視点で、見た目が変わらない）
- 見せたいものが画像では分からない場合（例：小さなボールの動き。このときはログの値を PR に書く）

このときは、PR の本文の「スクリーンショット」の節に「載せない」と理由を 1 行で書きます。

## 載せてはいけないもの（開発ルール集（非公開） §5）

- 秘密情報（トークン・鍵・パスワード）、個人のフォルダのパス、ほかのアプリの中身
- デスクトップ全体（タスクバー・通知）。ウィンドウだけを撮る
- 撮った後に画像を見て確かめてから、コミットする

## PR への載せ方

画像をコミットしてから、そのコミットの SHA を使った URL で本文に載せます（squash merge の後もリンクが切れないように）。

```markdown
![真上](https://github.com/dorachan121/RealTennis/blob/<コミットの SHA>/Docs/Screenshots/2026-11-04/<対応内容>/court_top.png?raw=true)
```
