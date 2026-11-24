# 2026-11-18 既定の選手を MetaHuman の M01 にする（W-3.10）

| 項目 | 内容 |
|---|---|
| 対応内容 | Project Settings の Tennis Player の `Appearance`（`Config/DefaultGame.ini`）を、仮の男性 `DA_Appearance_Player_Male01` から MetaHuman の `DA_Appearance_Player_M01` に替えた（利用者の判断、2026-11-18）。設定を替えるだけで、PIE の選手が M01 になり、その体で焼いた値でフォアを振る。服はまだない（テニスウェアは ART-03 §6 の手順 5） |
| ブランチ | `feat/default-metahuman-player` |
| 撮影 | テストマップ `L_TestCourt` の PIE（中継カメラ）。設定の差し替えはしていない（既定のまま）。`tennis.TimeScale` を 0.25 にして（撮った後に 1 に戻した）、打球ツール（`shoot_ball`、トップスピン、着地 (−9.0, 1.0) m）で打ち、すぐに J を押し続けて、`Tools/Screenshots/Capture-Window.ps1` で続けて撮った |

| 画像 | 見えるもの |
|---|---|
| 構え（画像：公開版には未収録） | PIE を始めたところ。既定の選手が M01（髪・ひげ付き）になっている |
| スイングの流れ（画像：公開版には未収録） | 選手のまわりを拡大して並べた（構え → テイクバック → インパクト → フォロースルー） |
