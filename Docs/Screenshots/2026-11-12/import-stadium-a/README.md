# 2026-11-12 センターコート A の仮の形を UE に取り込む（A-STD-010）

| 項目 | 内容 |
|---|---|
| 対応内容 | `SM_Stadium_A_Stands`（LOD 2 つ）・`SM_Stadium_A_Barrier`・`SM_Stadium_A_Floor` を `import_exported_fbx` で `Content/Tennis/Art/Stadium/` に取り込む。取り込みの規則に `SM_Stadium_` を足し、`_LOD1` を元のメッシュの LOD に入れる |
| ブランチ | `art/import-stadium-a` |
| 撮影 | 取り込んだ 3 つを、テストマップ `L_TestCourt` の原点に**仮に置いて**撮った（撮った後に消した。マップには入れていない。練習コートのフェンス・ベンチ・照明柱は、撮る間だけ隠した）。`Tools/Screenshots/Capture-Window.ps1`。色は Blender の確認用の仮の色 |

| 画像 | 見えるもの |
|---|---|
| スタジアム（画像：公開版には未収録） | 上の層の隅から。コートの周りに看板の壁（緑）、下の層・横の通路・上の層の観客席（青い席、白い縦の通路）。四隅は出入口 |
