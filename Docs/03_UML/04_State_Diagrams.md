# UML-04：状態図

| 項目 | 内容 |
|---|---|
| 文書 ID | UML-04 |
| 版 | 1.0（2026-11-04） |
| 関連 | [ルール判定仕様](../01_Specification/05_Rules_Specification.md)、[操作仕様](../01_Specification/04_Controls_Specification.md)、[画面遷移](../06_UI_UX/01_Screen_Flow.md) |

> 各状態機械には**時間切れ（タイムアウト）**を必ず設ける（NFR-QUAL-003：試合が進まなくなる不具合を防ぐ）。時間切れの値は各図の下の表に書く。

---

## ST-01 試合の進行（`UTennisMatchFlowComponent`／`ETennisMatchPhase`）

```mermaid
stateDiagram-v2
    [*] --> Walkon
    state "入場" as Walkon
    state "コイントス" as CoinToss
    state "ウォームアップ" as Warmup
    state "ポイント" as Point
    state "ポイント間" as BetweenPoints
    state "エンドチェンジ" as ChangeEnds
    state "セットブレイク" as SetBreak
    state "試合終了" as MatchEnd
    state "ポーズ" as Paused

    Walkon --> CoinToss : 入場の演出が終わった／スキップ
    CoinToss --> Warmup : 選択が決まった
    Warmup --> Point : 時間切れ／スキップ
    Point --> BetweenPoints : ポイントが決まった（ゲームは続く）
    Point --> ChangeEnds : ゲームが決まり、エンドチェンジがある
    Point --> SetBreak : セットが決まった（マッチは続く）
    Point --> MatchEnd : マッチが決まった
    BetweenPoints --> Point : ショットクロック 25 秒／ボタン
    ChangeEnds --> Point : 90 秒／ボタン（第 1 ゲームの後とタイブレーク中は即時）
    SetBreak --> Point : 120 秒／ボタン
    MatchEnd --> [*]

    Point --> Paused : ポーズ（オフライン）／アプリの中断
    BetweenPoints --> Paused : ポーズ
    Paused --> Point : 再開
    Paused --> BetweenPoints : 再開
    Paused --> MatchEnd : リタイア
```

| 状態 | 時間切れ | 時間切れの動作 |
|---|---|---|
| 入場 | 20 s | コイントスへ |
| コイントス | 15 s | 既定の選択（勝者がサーブ） |
| ウォームアップ | 300 s（Rule 29e） | ポイントへ |
| ポイント | 60 s（ボールが静止しない異常） | ルールエンジンに「やり直し」を出させ、ログに残す |
| ポイント間 | 25 s（設定で無制限） | 次のポイントへ |

## ST-02 ポイントの判定（`FTennisRuleJudge`）

```mermaid
stateDiagram-v2
    [*] --> Serve1
    state "第 1 サーブ" as Serve1 {
        [*] --> S1Ready
        state "準備" as S1Ready
        state "飛行中" as S1Flight
        S1Ready --> S1Flight : Hit（サーブ）
        S1Ready --> S1Ready : TossCaught
        S1Flight --> S1Ready : Let
    }
    state "第 2 サーブ" as Serve2 {
        [*] --> S2Ready
        state "準備" as S2Ready
        state "飛行中" as S2Flight
        S2Ready --> S2Flight : Hit（サーブ）
        S2Ready --> S2Ready : TossCaught
        S2Flight --> S2Ready : Let
    }
    state "ラリー" as Rally {
        [*] --> AwaitBounce
        state "相手コートへの飛行中" as AwaitBounce
        state "1 回バウンドした" as Bounced
        AwaitBounce --> Bounced : 正しい区域でバウンド
        Bounced --> AwaitBounce : 相手が打った
        AwaitBounce --> AwaitBounce : 相手がノーバウンドで打った
    }
    state "ポイント終了" as Ended

    Serve1 --> Serve2 : Fault
    Serve1 --> Rally : サーブが入った
    Serve2 --> Rally : サーブが入った
    Serve2 --> Ended : Fault（ダブルフォルト）
    Serve1 --> Ended : サーブがレシーバーに当たった／レシーバーがノーバウンドで返した
    Serve2 --> Ended : サーブがレシーバーに当たった／レシーバーがノーバウンドで返した
    Rally --> Ended : PointWon（R02〜R12、R17）
    Ended --> [*]
```

各遷移の条件の詳細は、[ルール判定仕様](../01_Specification/05_Rules_Specification.md) の S01〜S16・R01〜R19 を参照。

## ST-03 サーブの動作（`UTennisStrokeComponent`、サーブ時）

```mermaid
stateDiagram-v2
    [*] --> Setup
    state "立ち位置の調整" as Setup
    state "トス" as Toss
    state "溜め" as Charge
    state "スイング" as Swing
    state "フォロースルー" as Follow
    state "キャッチ" as Catch
    Setup --> Toss : 球種ボタンを押す
    Toss --> Charge : トスが手を離れた
    Charge --> Swing : ボタンを離す／ボールが打点の下限まで落ちた
    Charge --> Catch : トスのやり直し
    Catch --> Setup : キャッチのアニメが終わった
    Swing --> Follow : Impact（打球または空振り）
    Follow --> [*] : フォロースルーが終わった
```

| 状態 | 時間切れ | 動作 |
|---|---|---|
| 立ち位置の調整 | 25 s（ショットクロック。設定でオフ） | 警告（オンラインでは時間の違反） |
| 溜め | ボールが打点の 0.6 m 下まで落ちた | 自動でスイング（判定は Late） |
| キャッチ | 2 回を超えた | 警告を表示する |

## ST-04 選手の行動（`ATennisCharacter`）

```mermaid
stateDiagram-v2
    [*] --> Ready
    state "構え（待機）" as Ready
    state "スプリットステップ" as Split
    state "移動" as Move
    state "ストローク" as Stroke {
        [*] --> Windup
        state "テイクバック" as Windup
        state "スイング" as SwingS
        state "フォロースルー" as FollowS
        Windup --> SwingS : 離した／自動
        SwingS --> FollowS : Impact
    }
    state "リカバリー" as Recover
    state "サーブ" as Serve
    state "ポイント後のリアクション" as React
    state "歩く（エンドチェンジなど）" as Walk

    Ready --> Split : 相手のインパクトの直前
    Split --> Move : 着地（初動ブーストの判定）
    Ready --> Move : 移動の入力
    Move --> Stroke : ストロークの予約
    Ready --> Stroke : ストロークの予約
    Stroke --> Recover : フォロースルーの移動制限が終わった
    Recover --> Ready : センターに着いた
    Recover --> Split : 相手のインパクトの直前
    Ready --> Serve : 自分のサーブの番
    Serve --> Recover : フォロースルーが終わった
    Ready --> React : ポイント終了
    Move --> React : ポイント終了
    Recover --> React : ポイント終了
    React --> Walk : 次のポイントの位置へ
    Walk --> Ready : 位置に着いた
```

## ST-05 ショットの入力（`FTennisShotInputMachine`。`UTennisShotInputComponent` が持つ）

```mermaid
stateDiagram-v2
    [*] --> Idle
    state "待機" as Idle
    state "バッファ（予約待ち）" as Buffered
    state "予約済み・溜め中" as Charging
    state "離した（確定待ち）" as Released
    state "確定（インパクト済み）" as Committed
    Idle --> Buffered : 押した
    Buffered --> Charging : 打点の候補が見つかった（押した瞬間も同じフレームで）
    Buffered --> Buffered : 別の球種・修飾
    Buffered --> Idle : 1.2 s を超えた／離した
    Charging --> Charging : 別の球種・修飾（t* − 0.25 s まで）
    Charging --> Released : 離した
    Charging --> Committed : Impact（押したまま：自動で離した扱い。判定は t* + W_late）
    Charging --> Idle : ストロークが終わった（押したまま）・取り消された
    Released --> Committed : Impact
    Released --> Idle : ストロークが終わった・取り消された
    Committed --> Idle : ストロークが終わった・取り消された
```

- 入力は、意図が変わるたびに選手へ渡す（`ATennisCharacter::SubmitShotIntent`）。待機に戻ったら取り消す（`CancelShotIntent`）
- 打点の候補が見つかったら（押した瞬間に見つかった場合も）、選手が入力に知らせる（`ATennisCharacter::OnHitCandidateFound` → `OnCandidateFound`）。候補を知らせる経路を 1 つにするため、`SubmitShotIntent` は値を返さない
- ストロークが終わった・取り消されたら、選手が入力に知らせる（`OnStrokeEnded`）。ストロークが終わるのは、Notify「Tennis Recover Start」の時刻（DD-03 §6.6）。押したまま（溜め中）でも、ストロークが終わったら待機に戻り、次の押下が効く。離しても、ストロークは終わらない（離した時刻を記録するだけ）。振っていないときに離したら、選手はすぐ終えて知らせる。インパクトは、選手が判定と打球の後に入力へ知らせる（`ATennisCharacter::OnStrokeImpact` → `Impact`：離した → 確定、溜め中 → 自動で離した扱いで確定。空振りでも知らせる：DD-03 §6.4 の 8）
- 入力は、コントローラーが選手を操作する間だけ、その選手の出来事を購読する（`BindCharacter`）
- 球種は、押しているボタンと技の修飾の組み合わせで決まる（操作仕様 §3.1）。意図（`FShotIntent`）には、溜めの始まり（予約できた時刻）と離した時刻、押したままインパクトが来た印（`bAutoReleased`。離した時刻は記録しない）を記録する。溜め（0〜1）は、ストロークの ChargeTime を使ってストロークの側で求める
- 時刻はゲームの時刻（ワールドの秒。ポーズとスローで止まる・遅くなる）
- t* はボールのシミュレーション時刻で決まる（DD-03 §6.1）。入力の時刻（ワールドの秒）では測らず、押したまま（溜め中）インパクトが来たら、入力はそこで自動で離したことにして確定へ進む（判定は t* + W_late に離したもの＝Late の端、溜めは最大。操作仕様 §4.1。利用者の判断、2026-11-14。`ReleaseLeadTime` ≤ 0.15 s < `W_late` 0.200 s なので、t* + W_late はいつもインパクトより後）。その後に離しても何も起きない。球種の変更の期限（t* − 0.25 s）は、球種を途中で変えられるようにするとき（W-4.2）に同じ経路で足す。インパクト（`Impact`）は、ストロークが選手を通して知らせる（`OnStrokeImpact`。#154）

| 状態 | 時間切れ | 時間切れの動作 |
|---|---|---|
| バッファ | 押してから 1.2 s（入力バッファ。DATA-02 §5） | 待機へ（予約を取り消す） |
| 溜め中 | インパクト（ストロークが知らせる：DD-03 §6.6） | 自動で離した扱いで確定へ（判定は t* + W_late＝Late の端、溜めは最大。操作仕様 §4.1） |
| 離した・確定 | ストロークの時間切れ（ST-04） | 待機へ |
| どの状態でも | 入力の段階が変わった（操作仕様 §3.2。段階の切り替えは W-3.7）・操作する選手が替わった | 待機へ（予約を取り消す。`Cancel`） |

## ST-06 AI（StateTree：`ST_TennisAI`）

```mermaid
stateDiagram-v2
    [*] --> Root
    state "AI のルート" as Root {
        [*] --> Wait
        state "待機" as Wait
        state "サーブ" as AIServe {
            [*] --> PickServe
            state "球種と狙いを選ぶ" as PickServe
            state "トスと溜め" as TossCharge
            PickServe --> TossCharge
        }
        state "ラリー" as AIRally {
            [*] --> Recover
            state "リカバリー" as Recover
            state "反応待ち" as Perceive
            state "迎撃（移動）" as Intercept
            state "打球（ショットの選択と実行）" as Strike
            Recover --> Perceive : 相手が打った
            Perceive --> Intercept : 反応時間が過ぎた
            Intercept --> Strike : 打点の時刻 − 準備時間
            Strike --> Recover : インパクト
        }
        state "ネットプレー" as AINet {
            [*] --> Approach
            state "ネットへ詰める" as Approach
            state "ボレーの構え" as VolleyReady
            Approach --> VolleyReady : サービスラインを越えた
        }
        state "ポイント終了" as AIEnd
        Wait --> AIServe : 自分のサーブの番
        Wait --> AIRally : 相手のサーブ
        AIServe --> AIRally : サーブを打った
        AIRally --> AINet : 浅い球を攻めた（スタイル・状況による）
        AINet --> AIRally : ロブで下げられた
        AIRally --> AIEnd : ポイント終了
        AINet --> AIEnd : ポイント終了
        AIEnd --> Wait : 次のポイント
    }
```

## ST-07 カメラ（`UTennisCameraDirector`）

```mermaid
stateDiagram-v2
    [*] --> Broadcast
    state "中継カメラ" as Broadcast
    state "サーブの寄り" as ServeClose
    state "リプレイ" as Replay
    state "エンドチェンジ（反転）" as Flip
    state "入場・試合終了" as Cinematic
    Broadcast --> ServeClose : サーブの準備（設定がオン）
    ServeClose --> Broadcast : サーブを打った
    Broadcast --> Replay : ハイライトのリプレイ
    Replay --> Broadcast : 終了／スキップ
    Broadcast --> Flip : OnChangeEnds
    Flip --> Broadcast : 1.0 s のブレンド
    Cinematic --> Broadcast : 演出が終わった
    Broadcast --> Cinematic : 試合終了
```

- 今（W-3.8）は中継カメラだけで、切り替えはない（`ATennisPlayerCameraManager` がモードを直接持つ：UML-02 §7）。`UTennisCameraDirector` は W-5.10、ほかのモードは W-7.1・W-7.2 で足す

## ST-08 ボール（`UTennisBallSimComponent`）

```mermaid
stateDiagram-v2
    [*] --> Held
    state "手に持っている" as Held
    state "トス中" as Tossed
    state "飛行中（インプレー）" as InPlay
    state "デッド（ポイント後も物理は続く）" as Dead
    state "静止・回収" as Rest
    Held --> Tossed : Toss
    Tossed --> Held : キャッチ
    Tossed --> InPlay : サーブの Launch
    Tossed --> Dead : 打たずに地面に落ちた
    InPlay --> InPlay : Launch（打球）／Bounce／NetTouch
    InPlay --> Dead : ポイントが決まった
    Dead --> Rest : 速さ < 0.3 m/s または 4 s
    Rest --> Held : 次のポイント（サーバーの手へ）
```

## ST-09 オンラインのセッション

```mermaid
stateDiagram-v2
    [*] --> Offline
    state "オフライン" as Offline
    state "ログイン（EOS）" as Login
    state "ロビー" as Lobby
    state "マッチメイキング" as Matchmaking
    state "接続中" as Connecting
    state "試合中" as InMatch
    state "切断（猶予 60 s）" as Grace
    state "結果" as Result
    Offline --> Login : オンラインを選ぶ
    Login --> Lobby : 成功
    Login --> Offline : 失敗
    Lobby --> Matchmaking : クイックマッチ
    Lobby --> Connecting : 招待を受けた
    Matchmaking --> Connecting : 相手が見つかった
    Matchmaking --> Lobby : キャンセル／時間切れ 120 s
    Connecting --> InMatch : 両者の準備ができた
    Connecting --> Lobby : 失敗
    InMatch --> Grace : 相手または自分が切断
    Grace --> InMatch : 再接続（ポイントはやり直し）
    Grace --> Result : 60 s を超えた（不戦勝・不戦敗）
    InMatch --> Result : 試合終了
    Result --> Lobby
```
