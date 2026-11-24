# UML-03：シーケンス図

| 項目 | 内容 |
|---|---|
| 文書 ID | UML-03 |
| 版 | 1.0（2026-11-04） |
| 関連 | [システム構成](../02_Architecture/01_System_Architecture.md) §4〜6、[クラス図](02_Class_Diagrams.md) |

---

## SEQ-01 ラリーの 1 打（オフライン、プレイヤー）

```mermaid
sequenceDiagram
    autonumber
    actor P as プレイヤー
    participant IN as ShotInputComponent
    participant CH as TennisCharacter
    participant RC as ReachComponent
    participant SK as StrokeComponent
    participant AN as AnimInstance / Montage
    participant Q as ShotQuality / ErrorModel
    participant SO as ShotSolver
    participant BS as BallSimComponent
    participant EV as EventSubsystem

    Note over BS: 相手の打球の予測軌道はキャッシュ済み（Launch で計算）
    P->>IN: 球種ボタンを押す（トップスピン）
    IN->>CH: SubmitShotIntent（球種。溜めの始まりはまだない＝バッファ）
    CH->>RC: StartSearching
    RC->>BS: GetPrediction()
    BS-->>RC: FTrajectory
    RC->>RC: TennisReach::FindCandidates（軌道, 移動の能力）の先頭を採る
    RC-->>CH: OnCandidateFound（時刻・位置・高さ・到達時間）
    CH-->>IN: OnHitCandidateFound（バッファ → 溜め中）
    IN->>CH: SubmitShotIntent（溜めの始まり＝今）
    CH->>SK: ReserveStroke（Intent, 打点）
    SK->>SK: Chooser Table でストロークを選ぶ
    SK->>CH: 自動移動の目標（打点に入る位置）
    SK->>AN: モンタージュを止めて再生し、毎フレーム、ステッパの位置を置く（開始位置・待つ時間・再生速度の補正はステッパの中。その場で再生する：DD-03 §6.1・§6.2・§6.6）
    SK->>EV: OnStrokeStarted
    loop 押している間（毎フレーム）
        P->>IN: 左スティック（狙い）
        IN->>CH: SubmitShotIntent（狙い・修飾）
        CH->>SK: UpdateIntent（AimTarget, 修飾）
    end
    P->>IN: ボタンを離す
    IN->>CH: SubmitShotIntent（離した時刻 t_rel）
    CH->>SK: ReleaseStroke（t_rel）
    SK->>SK: ストロークの進行が ImpactTime を越えた（固定の刻みで積分。DD-03 §6.4）
    SK->>BS: GetStateAt（正確なインパクトの時刻）
    BS-->>SK: ボールの位置・速度・スピン
    SK->>SK: スイートスポットとの距離を判定（0.25 m 以内か）
    alt 打球できる
        SK->>Q: Evaluate（タイミング誤差, 距離, 体勢, 球威）
        Q-->>SK: 品質・σ・狙いの流れ
        SK->>Q: ApplyError（目標, シード）
        Q-->>SK: 誤差を加えた着地点
        SK->>SO: Solve（FShotRequest）
        SO->>SO: ニュートン法（同じ物理で試行）
        SO-->>SK: FShotSolveResult（打ち出しの状態）
        SK->>BS: Launch（打ち出し, インパクトの時刻）
        BS->>BS: 予測を作り直してキャッシュ
        BS->>EV: OnBallLaunched
        SK->>EV: OnImpact（品質・判定・球速）
    else 空振り
        SK->>EV: OnImpact（空振り）
    end
```

- 今の実装（W-3.5）：1〜9（入力から打点の候補まで）はある。押した瞬間に候補が見つかれば、同じフレームで溜め中になる（ST-05）。自動移動（選び直した打点の体の位置へ）はストロークの `PlayStroke` が始める（#142・#153）。ストローク（`SK`。`UTennisStrokeComponent`）は、候補が見つかると振り始め（選手の `HandleCandidateFound`）、インパクトで判定して打ち（M3 の打球：押した球種のデータで、ニュートラルの狙いへ誤差なしで。DD-03 §6.4。#169〜#171・#173）、そのあと入力に知らせ、RecoverStart で終わる（W-3.5。#153・#154）。狙い・修飾と品質・誤差は M4（W-4.1・W-4.2）で足す

## SEQ-02 サーブ（トス → 判定）

```mermaid
sequenceDiagram
    autonumber
    actor P as サーバー
    participant SK as StrokeComponent
    participant BS as BallSimComponent
    participant MF as MatchFlowComponent
    participant RJ as RuleJudge
    participant EV as EventSubsystem
    participant UM as 主審（Presentation）

    MF->>RJ: BeginPoint（サーバー, サイド, 第 1 サーブ）
    MF->>EV: OnPointStarted
    P->>SK: 球種ボタンを押す
    SK->>BS: Toss（トスの初速。ぶれは能力値の σ）
    loop トス中
        P->>SK: 狙い・溜め
        opt トスが乱れた
            P->>SK: トスのやり直し
            SK->>BS: キャッチ（ボールを手に戻す）
            SK->>RJ: TossCaught（判定なし）
        end
    end
    P->>SK: ボタンを離す
    SK->>BS: Launch（サーブ）
    BS->>MF: OnHit（サーバー, サーブ）
    MF->>RJ: OnHit
    BS-->>MF: Bounce / NetTouch / PostHit / BodyHit
    MF->>RJ: OnBallEvent
    alt 正しいボックスに入った（ネットに触れていない）
        RJ-->>MF: Continue（ラリーへ）
    else ネットに触れて正しいボックスに入った
        RJ-->>MF: Let
        MF->>UM: 「Let」をコール
        MF->>MF: 同じサーブをやり直す
    else ボックスの外・ネット・スティックに当たった
        RJ-->>MF: Fault
        MF->>UM: 「Fault」をコール
        alt 第 1 サーブ
            MF->>RJ: BeginPoint（第 2 サーブ）
        else 第 2 サーブ
            MF->>MF: EndPoint（ダブルフォルト）
        end
    end
```

## SEQ-03 ポイントの終了 → スコア → 次のポイント

```mermaid
sequenceDiagram
    autonumber
    participant BS as BallSimComponent
    participant MF as MatchFlowComponent
    participant RJ as RuleJudge
    participant SR as ScoreRules
    participant GS as GameState
    participant ST as MatchStats
    participant EV as EventSubsystem
    participant PR as 演出（主審・観客・カメラ・リプレイ）
    participant UI as UI

    BS-->>MF: Bounce（相手コートで 2 回目）
    MF->>RJ: OnBallEvent
    RJ-->>MF: PointWon（勝者, Winner, Rule 24b）
    MF->>SR: AwardPoint（Score, 勝者, Rules）
    SR-->>MF: FScoreDelta（ゲーム・セット・マッチの決着、エンドチェンジ、サーブの交代）
    MF->>GS: Score を更新（OnRep_Score で複製）
    MF->>ST: RecordPoint（結果）
    MF->>EV: OnPointEnded（FPointResult）
    EV->>PR: 主審のコール、観客の拍手、リプレイの保存、カメラ
    GS-->>UI: OnRep_Score → スコアボードを更新
    alt エンドチェンジがある
        MF->>EV: OnChangeEnds
        EV->>PR: カメラを反転、選手が反対のエンドへ歩く
        MF->>MF: 休憩（90 秒。ただし各セットの第 1 ゲームの後とタイブレーク中はなし）
    end
    alt マッチの決着
        MF->>EV: OnMatchEnded
        EV->>UI: 結果の画面
    else 続く
        MF->>MF: ポイント間（ショットクロック 25 秒）
        MF->>RJ: BeginPoint（次のサーバー・サイド）
    end
```

## SEQ-04 オンライン：クライアントが打つ（遅延補償）

```mermaid
sequenceDiagram
    autonumber
    participant CL as クライアント（打つ側）
    participant SV as サーバー（GameMode）
    participant GS as GameState（複製）
    participant OT as 相手のクライアント
    Note over CL,SV: どちらも同じ FTrajectory を持っている（打球イベント Seq=N から再生）
    CL->>CL: 予約（ストロークを開始）
    CL->>SV: ServerStrokeStarted（Seq=N, ストローク, 予定の打点）
    SV->>GS: MulticastStrokeStarted
    GS-->>OT: 相手のアニメを先に始める
    CL->>CL: インパクト（ローカルで品質・誤差・ソルバ → 予測の打球）
    CL->>CL: 予測の打球をすぐに再生する
    CL->>SV: ServerRequestHit（Seq=N, ClientViewTime, Intent, 離した時刻）
    SV->>SV: ClientViewTime のボールの位置を軌道から求める（巻き戻しは最大 150 ms）
    SV->>SV: 打点の判定・品質・誤差（シード = Seq）・ソルバ
    alt サーバーの判定が予測と一致
        SV->>GS: MulticastLaunch（Seq=N+1, ServerTime, 状態）
        GS-->>CL: 予測を確定（差があれば 0.1 秒かけて寄せる）
        GS-->>OT: 再生（ServerTime からの経過分を早送り）
    else 判定 NG（届かない・時刻のずれが大きい）
        SV->>GS: MulticastLaunch または 空振りの通知
        GS-->>CL: サーバーの結果で上書きする
        GS-->>OT: 再生
    end
```

## SEQ-05 オンライン：受ける側の再生とバウンドの補正

```mermaid
sequenceDiagram
    autonumber
    participant SV as サーバー（権威の BallSim）
    participant GS as GameState
    participant CL as クライアント（再生の BallSim）
    SV->>GS: MulticastLaunch（Seq, ServerTime, Pos, Vel, Spin）
    GS-->>CL: 受信（RTT/2 だけ遅れて届く）
    CL->>CL: Now − ServerTime だけ早送りして再生する
    CL->>CL: 表示位置を 0.1 秒かけて実位置へ寄せる（ワープを隠す）
    SV->>SV: 接地を検出
    SV->>GS: MulticastBounce（Seq, 時刻, 位置, 速度）
    GS-->>CL: 受信
    CL->>CL: 自分の軌道の接地点と比べる
    alt 誤差 > 5 cm（x64 と ARM64 の差など）
        CL->>CL: サーバーの状態に置き換え、表示は 0.1 秒かけて補正する
    else 誤差 ≤ 5 cm
        CL->>CL: そのまま続ける
    end
```

## SEQ-06 AI の 1 打

```mermaid
sequenceDiagram
    autonumber
    participant EV as EventSubsystem
    participant CX as RallyContext 評価器
    participant ST as StateTree
    participant IC as Intercept タスク
    participant CS as ChooseShot タスク
    participant TB as ShotTable
    participant CH as TennisCharacter（AI）
    participant SK as StrokeComponent
    EV->>CX: OnBallLaunched（相手が打った）
    CX->>CX: ReactionReadyTime = 今 + 反応時間（難易度）
    CX->>CX: 予測の誤差（PredictionNoise）を設定。時間とともに小さくする
    ST->>IC: 反応時間の後に開始
    IC->>CX: 知覚した軌道（誤差あり）
    IC->>IC: 打点の候補を選ぶ（プレイヤーと同じ ReachComponent を使う）
    IC->>CH: 移動の入力（打点へ）
    ST->>CS: 打点の時刻 − 準備時間 になったら開始
    CS->>TB: 候補ごとの打ち出しの近似（球種 × 打点の高さ × 深さ）
    TB-->>CS: 推定のネットの余裕・ミスの確率
    CS->>CS: ユーティリティの点数 × リスク選好 → softmax で選ぶ
    CS->>CH: SubmitShotIntent（球種・狙い・溜め）
    CH->>SK: ReserveStroke（以降はプレイヤーと同じ）
    CS->>CS: 離す時刻 = 理想の時刻 + N(0, タイミングの σ)
    CS->>CH: SubmitShotIntent（離した時刻）
    Note over SK: インパクト以降は SEQ-01 と同じ（品質・誤差・ソルバ）
```

## SEQ-07 リプレイの記録と再生

```mermaid
sequenceDiagram
    autonumber
    participant EV as EventSubsystem
    participant RS as ReplaySubsystem
    participant BS as BallSimComponent（リプレイ用）
    participant CH as 選手（リプレイ用のポーズ）
    participant CM as CameraDirector
    EV->>RS: OnPointStarted → 記録の開始
    loop ポイント中
        EV->>RS: OnBallLaunched / Bounce（打球イベントを保存）
        RS->>RS: 選手の Transform・アニメの状態を 30 Hz で保存
    end
    EV->>RS: OnPointEnded（ハイライトか判定）
    alt ハイライト（エース・ウィナー・長いラリー・際どい判定）
        RS->>CM: SetMode（Replay）
        RS->>BS: 打球イベントから軌道を再生（同じ物理。完全に一致する）
        RS->>CH: 保存したポーズを補間して再生
        opt 際どい判定（ラインから 5 cm 以内）
            RS->>CM: 接地点の拡大（電子ラインコールの再現）
        end
        RS->>CM: SetMode（Broadcast）
    end
```

## SEQ-08 スマートフォン：アプリの中断と再開

```mermaid
sequenceDiagram
    autonumber
    participant OS as OS
    participant PF as PlatformSubsystem
    participant MF as MatchFlowComponent
    participant SV as SaveSubsystem
    participant NET as オンラインのセッション
    participant UI as UI
    OS->>PF: ApplicationWillEnterBackground
    alt オフラインの試合
        PF->>MF: Pause（物理を止める）
        PF->>SV: SaveMatchSnapshot（ポイント間ならその時点、ポイント中ならポイントの開始時）
    else オンラインの試合
        PF->>NET: 切断として扱う（猶予 60 秒）
    end
    OS->>PF: ApplicationHasEnteredForeground
    alt オフライン
        PF->>UI: ポーズの画面を表示
        UI->>MF: 再開
    else オンライン（60 秒以内）
        PF->>NET: 再接続
        NET-->>MF: ポイントはやり直し（Rule 26 の扱い）
    else オンライン（60 秒を超えた）
        NET-->>UI: 不戦敗の通知
    end
```

## SEQ-09 試合の開始（メニュー → コート）

```mermaid
sequenceDiagram
    autonumber
    actor P as プレイヤー
    participant UI as メニュー（CommonUI）
    participant MS as MatchSetupSubsystem
    participant GI as GameInstance
    participant GM as GameMode
    participant MF as MatchFlowComponent
    participant CT as TennisCourt
    P->>UI: モード・選手・会場・形式・難易度を選ぶ
    UI->>MS: SetPlayers / SetVenue / SetRules / SetOpponentKind
    UI->>GI: 会場のレベルを開く（非同期の読み込み）
    GM->>MS: 試合の設定を読む
    GM->>CT: BuildSimContext（面・球・気温・標高・風）
    GM->>GM: 選手を生成し、コントローラー（Player / AI / 2P）を割り当てる
    GM->>MF: StartMatch（設定）
    MF->>MF: 入場 → コイントス → ウォームアップ（任意）
    MF->>MF: 最初のポイント（SEQ-02）
```
