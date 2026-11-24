# 2026-11-11 テストマップを練習コートにする（W-3.3 の PIE の確認）

| 項目 | 内容 |
|---|---|
| 対応内容 | テストマップ `L_TestCourt` に、練習コートの小物（フェンス・ベンチ・照明柱：A-STD-001）を置く。打球ツールとボールを相手の側（+X）へ移し、選手に向かって球を出せるようにする。PIE の視点は、フェンスの外の上から見下ろす |
| ブランチ | `feat/practice-court-test-map` |
| 撮影 | `build_test_court_level` で作り直した `L_TestCourt`。PIE は、Unreal MCP の `shoot_ball`（`DA_Shot_Lob` を (−5.5, 0.5) へ）で打ち、`TennisDebug` の `DebugTrajectory` を 1 にして、打ち出しの約 2.4 秒後に撮った |

| 画像 | 見えるもの |
|---|---|
| 全体（画像：公開版には未収録） | エディタで斜め上から。コートの周りにフェンス（余白の外周、高さ 3.05 m）、ネットのポストの後ろにベンチ、サイドラインの外に照明柱 4 本。右の青いカメラが PIE の視点 |
| 球が来る（画像：公開版には未収録） | PIE。相手の側の打点からの軌道（黄色の点線）がネットを越え、サービスラインの手前で弾んで（赤い点）、ベースラインの選手（カプセル）の前へ落ちてくる。フェンスの上の横桟は、選手とベースラインに重ならない |

球が来ている間に J（トップスピン）を押したときのログ（押した瞬間に候補が見つかり、同じフレームで溜め中になる。離すと待機に戻る）：

```
LogTennisBall:   [Launch] seq=0 t=94.2333 pos=X=11.000 Y=0.000 Z=1.000 …
LogTennisShot:   [Tester] DA_Shot_Lob target=(-5.50, 0.50) solved=1 fallback=None landing=(-5.50, 0.50) miss=0.001 m iterations=5
LogTennisInput:  [ShotInput] pressed -> Buffered (ETennisShotType::Topspin)
LogTennisStroke: [Reach] TennisCharacter_0: candidate kind=EHitKind::Smash side=EStrokeSide::Forehand time=97.796 body=-11.82,0.43 reach=0.448 stretch=0 score=0.876
LogTennisInput:  [ShotInput] candidate found -> Charging (ETennisShotType::Topspin)
LogTennisInput:  [ShotInput] released -> Released (ETennisShotType::Topspin)
LogTennisStroke: [Stroke] TennisCharacter_0: ended (released; the stroke is not made yet (W-3.4))
LogTennisInput:  [ShotInput] stroke ended -> Idle (ETennisShotType::Topspin)
```

打点に間に合わない球（深いロブがバウンドの後に頭上を越える、速いスライスが押してすぐ通り過ぎる）では、`no hit candidate within 1.20 s; dropped` で待機に戻ることも確かめた。
