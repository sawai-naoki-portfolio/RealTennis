# 2026-11-19 Motion Matching のロコモーション（仮のデータ。W-3.6、DD-03 §6.5）

| 項目 | 内容 |
|---|---|
| 対応内容 | `ABP_TennisPlayer` を作り直した：親を `UTennisAnimInstance` にし、Motion Matching（`PSD_Locomotion`：待機と、歩き・ジョグの 8 方向ずつの仮のデータ）→ Pose History（C++ が作る軌跡）→ Slot `DefaultSlot` →（腰と脚は Motion Matching、上半身はモンタージュ）と全身のモンタージュを、重み `LowerBodyLocomotionAlpha` で混ぜる。テイクバックの間は脚がロコモーション、スイングからは全身がストローク。PIE のログ：`DA_Stroke_FH_Mid_Drive_Square_Dip`、`impact distance=0.0093 hit=1` |
| ブランチ | `art/w36-locomotion-abp` |
| 撮影 | テストマップ `L_TestCourt` の PIE（中継カメラ。既定の選手 M01）。打球ツール（`shoot_ball`、トップスピン、着地 (−9.0, 2.0) m：選手は横へ動いて打つ）で打ち、J（トップスピン）を短く押した。`tennis.TimeScale 0.1` にして `Tools/Screenshots/Capture-Window.ps1` で続けて撮った（撮った後に 1 に戻した）。選手のまわりを切り出して拡大した |

| 画像 | 見えるもの |
|---|---|
| ロコモーションからスイングへ（画像：公開版には未収録） | 左上から順に。00〜03：横へ動きながら、脚は Motion Matching で踏み出し（仮の歩き・ジョグ）、上半身はテイクバック。04〜09：スイングからは全身のモンタージュで打ち、フォロースルーへ |
