# 2026-11-12 仮の選手のメッシュにラケットを持たせる（DD-03 §7、W-3.6 の着手）

| 項目 | 内容 |
|---|---|
| 対応内容 | `ATennisCharacter` に、Project Settings の Tennis Player（`UTennisPlayerSettings`）が指す仮の選手 `SKM_Manny_Simple` と待機のアニメーション `MM_Idle` を付け、右手のソケット `racket_attach_r` にラケット `SM_Racket` のソケット `grip` を重ねて付ける（`UTennisRacketComponent`）。メッシュがあるのでカプセルは隠す |
| ブランチ | `feat/player-mesh-racket` |
| 撮影 | テストマップ `L_TestCourt` の PIE（中継カメラ）。`Tools/Screenshots/Capture-Window.ps1` で撮った。拡大の画像は、同じ画面を原寸で撮って選手の周りを切り出し、3 倍にした |

| 画像 | 見えるもの |
|---|---|
| PIE（画像：公開版には未収録） | 出現位置（ベースラインの後ろ）の選手。カプセルの代わりに Manny が待機のアニメーションで立つ。手前の横の棒は練習コートのフェンス |
| 拡大（画像：公開版には未収録） | 選手の拡大。右手にラケット（赤いグリップ）を持ち、先端は手の甲の側から斜め前へ出る。足元はコートの面に付いている（メッシュの位置はカプセルの底） |
