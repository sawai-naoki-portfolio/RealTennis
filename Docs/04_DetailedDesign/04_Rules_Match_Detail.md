# DD-04：ルールと試合の進行 詳細設計

| 項目 | 内容 |
|---|---|
| 文書 ID | DD-04 |
| 版 | 1.0（2026-11-04） |
| 対象のクラス | `ATennisGameMode`、`UTennisMatchFlowComponent`、`FTennisRuleJudge`、`FTennisScore`、`FTennisScoreRules`、`ATennisGameState`、`UTennisMatchStatsComponent`、`UTennisUmpireComponent` |
| 要件 | FR-RULE-001〜007、FR-SCR-001〜008、FR-MATCH-001〜007、FR-STAT-001〜002 |
| 仕様 | [ルール判定仕様](../01_Specification/05_Rules_Specification.md)（判定表 S01〜S16・R01〜R19） |

---

## 1. 責任の分担

| クラス | 責任 | 持たないもの |
|---|---|---|
| `FTennisRuleJudge`（TennisSim） | 1 ポイントの中の判定（イベント → 判定） | スコア、時間、演出 |
| `FTennisScoreRules`（TennisSim） | スコアの更新、サーバーとサイド、エンドチェンジ、局面、コールの文字列 | ポイントの判定 |
| `UTennisMatchFlowComponent`（Tennis） | 試合の段階、時間、ポイントの開始と終了、選手の配置、イベントの発行 | 判定とスコアの計算そのもの |
| `ATennisGameState`（Tennis） | 複製する状態（スコア、段階、ラリー番号、シード） | ロジック |
| `UTennisMatchStatsComponent` | 統計の記録と分類 | 判定 |
| `UTennisUmpireComponent` | コールの演出（音声・字幕） | 判定 |

## 2. 試合の進行（`UTennisMatchFlowComponent`）

### 2.1 試合の設定（`FMatchSetup`）

```cpp
USTRUCT() struct FMatchSetup
{
    GENERATED_BODY()
    TObjectPtr<UPlayerProfileData> Players[2];
    EOpponentKind                  Opponent;      // AI / LocalHuman / Online / AIvsAI
    TObjectPtr<UAIDifficultyData>  Difficulty[2]; // AI のとき
    TObjectPtr<UVenueData>         Venue;         // 面・球・環境・レベル
    FMatchRules                    Rules;
    ETennisAssistPreset            Assist[2];
    int32                          MatchSeed;     // 0 なら起動時の時刻から作る
    bool                           bSkipWalkon, bSkipWarmup;
    EClockMode                     Clock;         // RealTime / Skippable / None
};
```

### 2.2 段階と処理

状態図は [ST-01](../03_UML/04_State_Diagrams.md)。

| 段階 | 入るときの処理 | 出る条件 |
|---|---|---|
| Walkon | 選手を入場の位置に置き、演出を始める | 演出の終了／スキップ |
| CoinToss | トス（`FRandomStream(MatchSeed)`）。勝者が選ぶ（AI は §8） | 選択の確定 |
| Warmup | AI 対 AI のラリー（スキップ可） | 時間／スキップ |
| Point | `BeginPoint()`：サーバーとサイドを `GetServeInfo()` で求め、選手を配置し、`RuleJudge.BeginPoint()`、ボールをサーバーの手に、入力のコンテキストを切り替え、`OnPointStarted` | 判定が `PointWon` |
| BetweenPoints | ショットクロック（25 s）を始める | 時間／サーバーの準備 |
| ChangeEnds | 選手の担当の側を入れ替える（`CourtSide *= −1`）、`OnChangeEnds`、90 s（第 1 ゲームの後とタイブレーク中は 0 s） | 時間／ボタン |
| SetBreak | 120 s、統計の表示 | 時間／ボタン |
| MatchEnd | `OnMatchEnded`、統計とセーブ | — |

### 2.3 ポイントの開始時の配置

| 選手 | 位置（自分の側、X の絶対値・Y） |
|---|---|
| サーバー | X = 11.985（ベースラインの 0.10 m 後ろ）、Y = デュースサイドなら +0.8 m、アドなら −0.8 m（自分から見て右がデュース。座標の符号は側で反転） |
| レシーバー | X = 12.4（初期値。準備中に動ける）、Y = 対角のサービスボックスの外側の延長 |

## 3. ルールの判定（`FTennisRuleJudge`）

### 3.1 イベントの順序

- 入力は、ボールのイベント（時刻の順）と、打球（`OnHit`）と、選手のイベント（`PlayerNetContact`、`FootFault`）
- **同じステップの中で複数のイベント**が起きた場合は、時刻の順に処理する。最初に `PointWon` を返した時点でポイントは終わり、以降のイベントは無視する（例：白帯 → 自コートに落下）
- `OnHit` は、ボールのシミュレーションの `Launch` より先に呼ぶ（打った瞬間に打者が交代する）

### 3.2 実装の形

```cpp
ERuleVerdict FTennisRuleJudge::OnBallEvent(const FBallEvent& E)
{
    if (PointPhase == EPointPhase::Ended) { return ERuleVerdict::Ignored; }
    switch (PointPhase)
    {
    case EPointPhase::Serve1:
    case EPointPhase::Serve2: return JudgeServeEvent(E);   // 表 S01〜S16
    case EPointPhase::Rally:  return JudgeRallyEvent(E);   // 表 R01〜R19
    default:                  return ERuleVerdict::Ignored;
    }
}

ERuleVerdict FTennisRuleJudge::JudgeRallyEvent(const FBallEvent& E)
{
    const int32 H = LastHitter, O = 1 - LastHitter;
    switch (E.Type)
    {
    case EBallEventType::Bounce:
        if (E.Side == SideOf(H))                                    // ITF Rule 24(c)：自コートに落ちた
            return End(O, ETennisPointReason::NetError, TEXT("24c"));
        if (BouncesSinceHit == 0)
        {
            BouncesSinceHit = 1;
            if (Court.IsInside(E.Pos2D(), ECourtArea::SinglesHalf, E.Side, E.ContactPatch))
            { bFirstBounceGood = true; return ERuleVerdict::Continue; }   // Rule 12
            return End(O, ClassifyOut(E), TEXT("24c"));
        }
        return End(H, bReceiverTouched ? ETennisPointReason::DoubleBounce
                                       : ETennisPointReason::Winner, TEXT("24b"));
    case EBallEventType::FixtureHit:                                // Rule 13
        return bFirstBounceGood ? End(H, ETennisPointReason::FixtureAfterGoodBounce, TEXT("13"))
                                : End(O, ETennisPointReason::FixtureBeforeBounce, TEXT("24d"));
    case EBallEventType::BodyHit:                                   // Rule 24(i)
        return End(1 - E.SubjectId, ETennisPointReason::BodyHit, TEXT("24i"));
    // NetTouch・PostHit・NetCross は Continue（Rule 25 a・c・d）。越えずに落ちた場合は Bounce で判定される
    default:
        return ERuleVerdict::Continue;
    }
}
```

### 3.3 境界の扱い

| 状況 | 扱い |
|---|---|
| ラインの上 | `IsInside` は `ContactPatch(v_n)` を加えて判定する（Rule 12） |
| ネットを越えたかどうか | `NetCross` イベントの有無。ネットの面の真上で止まる球（まれ）は、次に落ちた側で決める |
| ボールが静止しない（転がり続ける） | 2 回目のバウンドの後は、`Rest` を待たずに終わる。サーブ中に転がり続ける場合は、`Rest` で Fault |
| ポイントの時間切れ（60 s） | `Replay`（Rule 26 のやり直し）。ログを残す（不具合の検出用） |

## 4. スコア（`FTennisScoreRules`）

### 4.1 データ

```cpp
USTRUCT(BlueprintType) struct FTennisScore
{
    GENERATED_BODY()
    UPROPERTY() int32 Points[2]     = {0, 0};   // 標準ゲームのポイント、またはタイブレークの点
    UPROPERTY() int32 Games[2]      = {0, 0};   // 現在のセット
    UPROPERTY() int32 Sets[2]       = {0, 0};
    UPROPERTY() TArray<FIntPoint> SetHistory;   // 終わったセットのゲーム数（タイブレークの点は TBHistory）
    UPROPERTY() TArray<FIntPoint> TBHistory;
    UPROPERTY() ETieBreakKind TieBreak = ETieBreakKind::None;  // None / Set7 / Set10 / Short5 / Match7 / Match10
    UPROPERTY() int32 Server = 0;               // 現在のゲーム（タイブレークでは最初）のサーバー
    UPROPERTY() int32 TieBreakFirstServer = 0;
    UPROPERTY() int32 GamesSinceBallChange = 0;
    UPROPERTY() int32 PlayerOnSideMinusX = 0;   // X < 0 の側にいる選手（エンドチェンジで入れ替える）
    UPROPERTY() bool  bPendingChangeAfterFirstGame = false; // 偶数ゲームでセットが終わったとき
};
```

### 4.2 主な関数

| 関数 | 内容 |
|---|---|
| `AwardPoint(Score, Winner, Rules)` | [ACT-06](../03_UML/05_Activity_Diagrams.md)。結果の差分（`FScoreDelta`：GameWon・SetWon・MatchWon・TieBreakStarted・ChangeEnds・ServerChanged・BallChange）を返す |
| `GetServeInfo(Score, Rules)` | 次のポイントのサーバーとサイド。タイブレークは [ルール仕様 §5.2](../01_Specification/05_Rules_Specification.md) の式。No-Ad の決勝ポイントはレシーバーの選択を待つ |
| `ShouldChangeEnds(Before, After, Rules)` | 奇数ゲームの後、セットの終わり（偶数なら保留）、タイブレーク中（6 点ごと、代替は 1-4-4） |
| `GetSituation(Score, Rules)` | GamePoint・BreakPoint・SetPoint・MatchPoint・DecidingPoint のビット集合（サーバー視点・レシーバー視点） |
| `GetCallText(Score, Lang)` | 主審のコール（§5） |

### 4.3 決勝ポイント（No-Ad）のサイドの選択

- レシーバーが人間なら、UI で「デュースサイド／アドサイド」を選ぶ（3 秒で既定のデュースサイド）
- AI は、自分のリターンの得意な側（フォアで受けられる側）を選ぶ

## 5. 主審のコール（`UTennisUmpireComponent`）

| 出来事 | コール（英語の音声） | 字幕（日本語） |
|---|---|---|
| ポイント（標準ゲーム） | "Fifteen–Love"、"Thirty all"、"Deuce"、"Advantage {Name}" | 「15-0」「30-30」「デュース」「アドバンテージ {名前}」 |
| ゲーム | "Game, {Name}"／"Game, {Name}. {Name} leads {n} games to {m}" | 「ゲーム {名前}。{n}-{m}」 |
| セット | "Game and first set, {Name}, {n} games to {m}" | 「ゲーム、第 1 セット {名前}」 |
| タイブレーク | "{n}–{m}, {Name}"（リードしている側の名前） | 「{n}-{m} {名前}」 |
| マッチ | "Game, set and match, {Name}" | 「ゲームセット、ウォンバイ {名前}」 |
| フォルト | "Fault"（電子ラインコールの声） | 「フォルト」 |
| レット | "Let, first service"／"Let, second service" | 「レット」 |
| アウト | "Out"（電子ラインコールの声） | 「アウト」 |
| フットフォールト | "Foot fault" | 「フットフォールト」 |
| 時間の違反 | "Time violation, warning, {Name}" | 「タイムバイオレーション」 |
| 準備 | "Quiet please"（観客が騒いでいるとき） | — |

- **選手の名前**の音声は、名前ごとに収録する（架空の選手 24 人分）。オンラインで名前が不明な場合は "Player 1"／"Player 2"
- 数のコールは、標準ゲームでは定型の組み合わせ（Love・Fifteen・Thirty・Forty の 4×4＝16 通り＋Deuce・Advantage）を 1 本ずつ収録し、つなぎ合わせない（自然さのため）。タイブレークは数（0〜20）をつなぐ

## 6. 統計（`UTennisMatchStatsComponent`）

### 6.1 記録する項目（選手ごと）

| 分類 | 項目 |
|---|---|
| サーブ | エース、ダブルフォルト、第 1 サーブの数と入った数、第 1／第 2 サーブでの得点、最高・平均の速さ（第 1／第 2）、サーブのコースの分布（ワイド・ボディ・T） |
| リターン | リターンの得点、ブレークポイントの機会と成功 |
| ラリー | ウィナー（フォア・バック・ボレー・スマッシュ）、凡ミス、強制されたミス、ネットでの得点と機会 |
| ショット | 球種ごとの数、平均の球速と回転、ネットの上の余裕の平均 |
| 移動 | 走行距離（合計・1 ポイントあたり）、最高速度 |
| ラリー | ラリーの長さの分布（0〜4・5〜8・9 以上） |

### 6.2 凡ミスと強制されたミスの分類

ミス（`NetError`・`OutLong`・`OutWide`）をした選手について、**直前に受けた球の難しさ** D を計算する。

```
D = 0.35 · clamp((v_in − 20) / 20, 0, 1)            … 球速
  + 0.20 · clamp((rpm_in − 1500) / 2500, 0, 1)       … 回転
  + 0.25 · RunSpeedRatio_at_impact                    … 走りながら
  + 0.10 · StretchRatio                               … 届かない
  + 0.10 · (打点の高さが帯の外なら 1)
D ≥ 0.45 なら強制されたミス、それ未満なら凡ミス
```

閾値 0.45 は、AI 対 AI の自動対戦で、プロの統計の典型的な比率（凡ミスと強制されたミスがおおむね同じ程度）に近づくように調整する（設計推定。T-STAT-002）。

## 7. ボール交換と時間の規則

| 項目 | 実装 |
|---|---|
| ボール交換 | `GamesSinceBallChange` を数える。規則（[ルール仕様 §6](../01_Specification/05_Rules_Specification.md)）で交換するとき、`OnBallChange` を発行し、ボールの摩耗の値を 0 に戻す（FR-BALL-011） |
| ショットクロック | `BetweenPoints` の入りで 25 s。サーバーがトスを上げたら止まる。0 になったら：実時間モードは時間の違反、スキップ可モードは何もしない |
| 休憩 | `ChangeEnds`・`SetBreak` の時間。プレイヤーはボタンで飛ばせる（オンラインは両者が押したら） |

## 8. コイントスと AI の選択

| AI の状況 | 選択 |
|---|---|
| サーブの能力値が高い（≥ 75）またはビッグサーバー | サーブ |
| リターンが得意（スタイルがカウンターパンチャー） | レシーブ |
| 屋外で風・日差しが強い | 風上／太陽のないエンドを選び、サーブ／レシーブは相手に選ばせる |
| その他 | レシーブ（一般的な傾向。設計推定） |

## 9. 試合の途中保存（`FMatchSnapshot`）

| 項目 | 内容 |
|---|---|
| 保存する時 | ポイント間、エンドチェンジ、セットブレイク、アプリの中断（ポイント中ならポイントの開始時の状態） |
| 中身 | `FMatchSetup`、`FTennisScore`、統計、スタミナ、乱数の状態（`MatchSeed`、`RallySeq`）、ボール交換からのゲーム数、AI の相手モデル |
| 復元 | 段階を `BetweenPoints` から再開する |

## 10. テスト

| ID | 内容 |
|---|---|
| T-RULE-S01〜S16 | ルール判定仕様の表 S01〜S16 の各行（イベント列を作って判定を確認） |
| T-RULE-R01〜R19 | 同じく R01〜R19 |
| T-RULE-010 | ラインの判定（外縁から 1 mm 内側はイン、パッチ半径 + 1 mm 外側はアウト） |
| T-RULE-020 | 同じステップ内の複数イベント（白帯 → 自コート）で、最初の判定だけが有効 |
| T-SCR-001〜016 | [ルール仕様 §7](../01_Specification/05_Rules_Specification.md) の 16 例 |
| T-SCR-020 | 全プリセット（ツアー標準・グランドスラム・ショート・マッチタイブレーク・クイック）で、ランダムなポイントの列 10 万試合を流し、不変条件を確認する（スコアが負にならない、マッチが必ず終わる、セット数が上限を超えない、サーバーがゲームごとに交代する） |
| T-MATCH-001 | 状態図 ST-01 の全遷移 |
| T-MATCH-002 | 途中保存 → 復元でスコアと統計が一致する |
| T-STAT-001 | 統計の各項目が、作ったイベント列から正しく数えられる |
| T-STAT-002 | AI 対 AI で凡ミスと強制されたミスの比率が 0.6〜1.6 |
