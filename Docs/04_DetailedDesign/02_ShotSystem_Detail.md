# DD-02：ショット 詳細設計

| 項目 | 内容 |
|---|---|
| 文書 ID | DD-02 |
| 版 | 1.0（2026-11-04） |
| 対象のクラス | `FShotIntent`、`FTennisShotQuality`、`FTennisErrorModel`、`FShotRequest`、`FTennisShotSolver`、`UShotTypeData`、`FTennisShotTable` |
| 要件 | FR-SHOT-001〜012、FR-INP-003〜005 |
| 根拠 | 既存設計 §4〜5、[RES-03](../00_Research/03_Player_Match_Stats_Research.md) §1、[操作仕様](../01_Specification/04_Controls_Specification.md) §4〜5 |

---

## 1. 処理の流れ

```
FShotIntent（入力・AI）
   │ 球種・狙い・溜め・修飾・離した時刻
   ▼
UTennisStrokeComponent（打点・ストローク・アニメ）
   │ インパクトの時刻・ボールの状態・体勢
   ▼
FShotQualityInputs ──▶ FTennisShotQuality::Evaluate ──▶ 品質 Q・σ・狙いの流れ・ミスの原因
   ▼
FTennisErrorModel::ApplyError（シード付き）──▶ 誤差を加えた目標点
   ▼
FShotRequest（打点・目標点・球種・目標の速さ／制約・回転）
   ▼
FTennisShotSolver::Solve ──▶ 打ち出しの状態（FBallState）
   ▼
UTennisBallSimComponent::Launch
```

## 2. 球種の一覧（`UShotTypeData` の初期値）

速さは m/s、回転は rpm（正 = トップスピン、負 = バックスピン）。σ は深さ方向の基本値（m）。

| 球種 | 速さ（最小〜最大） | 回転 | 制約（未知数 s・θ に対する 2 つ目の条件） | 基本 σ | 球威の再利用 | 初期仰角 |
|---|---|---|---|---|---|---|
| Flat | 25〜46 | +600 | ネットの上の余裕：溜め 0 で 0.70 m → 溜め 1 で 0.25 m | 0.90 | 0.25 | 5° |
| Topspin | 22〜38 | +2,300 | 余裕：1.40 → 0.60 m | 0.60 | 0.20 | 9° |
| HeavyTopspin | 22〜36 | +3,200 | 余裕：1.80 → 0.90 m | 0.55 | 0.15 | 12° |
| Slice | 18〜31 | −1,500 | 余裕：0.60 → 0.25 m | 0.70 | 0.30 | 4° |
| Lob | 15〜26 | +1,500 | 最高点：溜め 0 で 9.0 m → 溜め 1 で 6.0 m | 1.20 | 0.05 | 30° |
| SliceLob | 13〜22 | −1,000 | 最高点：8.0 → 5.5 m | 1.30 | 0.05 | 28° |
| Drop | 8〜16 | −2,000 | 余裕：0.45 → 0.20 m | 0.50 | 0.00 | 8° |
| Volley | 12〜30 | −400 | 余裕：0.30 → 0.10 m | 0.60 | 0.40 | 2° |
| HalfVolley | 15〜28 | +800 | 余裕：0.60 → 0.30 m | 0.80 | 0.35 | 6° |
| Smash | 30〜50 | +500 | **速さを固定**（溜めで決める）→ θ だけを解く | 0.80 | 0.10 | −8° |
| ServeFlat | 45〜64 | 1,200（軸 +10°） | 速さを固定 → θ と方位を解く | 0.45 | — | −5° |
| ServeSlice | 40〜54 | 2,200（軸 +65°） | 同上 | 0.40 | — | −3° |
| ServeKick | 35〜46 | 3,200（軸 −20°） | 同上 | 0.35 | — | 0° |
| ServeUnderarm | 15〜22 | −1,500 | 同上 | 0.50 | — | 10° |

**溜めとネットの上の余裕**：溜め（Charge）を「攻撃性」として扱う。溜めるほど余裕が小さく（低く速く）なる。速さは、2 つの条件（飛距離・余裕）から自然に決まる。解いた速さが `MaxSpeed × 能力値の倍率` を超える場合は、余裕を大きくして（速さを抑えて）解き直す。

## 3. 回転

### 3.1 スピンベクトル

打球の進行方向 H（水平の単位ベクトル）、上 U = (0, 0, 1) に対して：

```
ω = rpm × (2π/60) × [ cos φ · (U × H) + sin φ · U ]
```

- φ = 0：純粋なトップスピン（rpm > 0）／バックスピン（rpm < 0）
- φ > 0：打者から見て左に曲がる成分（U 軸まわり）
- **右利きのスライスサーブ**は φ = +65°（左へ曲がる）。左利きは φ の符号を反転する
- キックサーブは φ = −20°（右利き。トップスピンが主で、少し右へ曲がる）
- サーブの軸は、横の入力 x（−1〜1。打者から見て右が正。画面の向きからの変換は入力の層が受け持つ）で φ − 15°·x に傾けられる（[操作仕様](../01_Specification/04_Controls_Specification.md) §5）。右へ倒すほど右へ曲がる向きで、利き手によらない（左利きのスライスは右へ倒すと曲がりが強まる）。範囲の外は −1〜1 に収め、数でない値は 0 として警告を出す
- 利き手の反転と傾きは `TennisShotSpin::AxisDeg`（`TennisSim/Public/Shot/ShotSpin.h`）で計算する

### 3.2 回転数

```
rpm = BaseRpm × SpinSkill × (1 + 0.10·Control − 0.15·Power) × (0.8 + 0.2·Q)
SpinSkill = lerp(0.85, 1.20, 回転量の能力値 / 100)
```

## 4. 目標の速さ（速さを固定する球種：サーブ・スマッシュ）

```
s = lerp(MinSpeed, MaxSpeed × SpeedSkill, Charge^0.8) × (1 + 0.10·Power − 0.10·Control) − PosturePenalty
SpeedSkill     = lerp(0.90, 1.10, 球威の能力値 / 100)
PosturePenalty = 6 · StretchRatio + 3 · RunSpeedRatio      [m/s]
```

ラリーの球種（制約が余裕・最高点のもの）の速さは、ソルバの結果で決まる。相手の球威の再利用は、ネットの上の余裕を下げる方向に効かせる：

```
余裕の目標 = lerp(余裕0, 余裕1, Charge') 、 Charge' = clamp(Charge + PaceReuse · max(0, v_in − 20) / 20, 0, 1)
```

### 4.1 球種のデータからリクエストへ（`TennisShotRequestMapping::Make`）

球種の値 `FShotTypeParams`（`UShotTypeData` が持つ。[DATA-01 §3.1](../05_Data/01_Data_Model.md)）と、打点・狙い・時刻・溜め・利き手・横の入力から `FShotRequest` を作る。

| 項目 | 今の写し方（M2） | 後で加える補正 |
|---|---|---|
| 溜め | 0〜1 に収める。数でない値は 0 として警告 | — |
| 条件の目標 | lerp(ConstraintAtCharge0, ConstraintAtCharge1, Charge) | 球威の再利用の Charge'（M4） |
| 速さ（FixedSpeed） | lerp(MinSpeed, MaxSpeed, Charge^0.8) | SpeedSkill・Power／Control・PosturePenalty（M4） |
| 速さの範囲・初期の仰角 | そのまま | SpeedSkill（M4） |
| 回転数 | BaseRpm | §3.2 の SpinSkill・Power／Control・品質（M4） |
| 回転の軸 | `TennisShotSpin::AxisDeg`（利き手・横の入力。§3.1） | — |

## 5. 品質（`FTennisShotQuality::Evaluate`）

### 5.1 入力（`FShotQualityInputs`）

| 入力 | 意味 | 範囲 |
|---|---|---|
| TimingError | Δt = 離した時刻 − 理想の時刻。押したままインパクトが来た（`FShotIntent::bAutoReleased`）なら Δt = W_late（Late の端。操作仕様 §4.1） | s |
| ImpactDistance | スイートスポットとボールの距離 | 0〜0.25 m |
| ContactHeight | 打点の高さ | m |
| RunSpeedRatio | インパクト時の移動速度 / 最高速度 | 0〜1 |
| StretchRatio | 届かなかった距離 / 0.6 m（ストレッチのとき） | 0〜1 |
| IncomingSpeed、IncomingSpin | 来た球の速さ・回転 | m/s、rpm |
| Stamina | 0〜1 | |
| PressureLevel | 局面の重さ（ブレークポイント 0.5、セットポイント 0.7、マッチポイント 1.0） | 0〜1 |

### 5.2 式

```
q_t = タイミングの係数（操作仕様 §4.2 の表）
q_c = 1 − 0.7 · (ImpactDistance / 0.25)^1.5
q_h = 打点の高さの係数：0.8〜1.2 m で 1.0。0.3 m と 1.7 m で 0.75 まで線形に下がる（ボレー・スマッシュは別の帯）
q_s = 0.92 + 0.08 · Stamina
Q   = clamp(q_t · q_c · q_h · q_s, 0.05, 1.0)
```

### 5.3 誤差の大きさ

```
σ_d = BaseSigma · SkillSigma · (1 + 2·(1 − Q)) · PostureF · PaceF · PressureF · ModSigma
σ_s = 0.6 · σ_d

SkillSigma = lerp(1.30, 0.70, その球種の能力値 / 100)
PostureF   = 1 + 0.8·RunSpeedRatio + 1.5·StretchRatio
PaceF      = 1 + 0.02·max(0, v_in − 20) + 0.00008·max(0, rpm_in − 2000)
PressureF  = 1 + 0.25 · PressureLevel · (1 − 安定性 / 100)
ModSigma   = 強打 ×1.30、コントロール ×0.80（トリガーの量で補間）
```

### 5.4 狙いの流れ（Early／Late）

```
AimPull = sign · 8.0 · max(0, |Δt| − W_good)     [m、横方向]
sign：Early = 引っ張る方向、Late = 流す方向
      右利きのフォアハンド：引っ張る = 打者から見て左（クロス）
      右利きのバックハンド：引っ張る = 右。左利きは反転
```

### 5.5 ミスの原因（`EMissCause`）

ポイントの後に表示する。σ の各要素の対数の寄与を比べ、最も大きいもの（と、閾値を超えたもの）を原因とする。

| 原因 | 判定の条件 | 表示 |
|---|---|---|
| Timing | ln(1 + 2(1 − q_t)) が最大、かつ \|Δt\| > W_good | 「振り遅れ」／「早すぎ」 |
| Contact | q_c < 0.6 | 「打点がずれた」 |
| Height | q_h < 0.85 | 「打点が低い／高い」 |
| Posture | ln(PostureF) が最大 | 「体勢が崩れた」 |
| Pace | ln(PaceF) が最大 | 「球威に押された」 |
| Risk | 狙いの点とラインの距離 < 1.0 σ_d | 「狙いすぎ」 |
| Pressure | ln(PressureF) > 0.1 | 「プレッシャー」（補助的に表示） |
| Solver | ソルバが Nearest で返した（物理的に無理な打球） | 「無理な体勢から打った」 |

## 6. 誤差の適用（`FTennisErrorModel::ApplyError`）

```cpp
FVector2D FTennisErrorModel::ApplyError(const FVector2D& Target, const FShotQualityResult& Q,
                                        FRandomStream& Rng, const FVector2D& Dir)
{
    const FVector2D Perp(-Dir.Y, Dir.X);
    auto Gauss = [&Rng]() {                              // Box–Muller（FRandomStream だけを使う）
        const double U1 = FMath::Max(Rng.GetFraction(), 1e-12);
        const double U2 = Rng.GetFraction();
        return FMath::Sqrt(-2.0 * FMath::Loge(U1)) * FMath::Cos(2.0 * UE_DOUBLE_PI * U2);
    };
    const double Ed = FMath::Clamp(Gauss(), -3.0, 3.0) * Q.SigmaDepth;
    const double Es = FMath::Clamp(Gauss(), -3.0, 3.0) * Q.SigmaSide;
    return Target + Dir * Ed + Perp * (Es + Q.AimPull);
}

int32 FTennisErrorModel::MakeSeed(int32 MatchSeed, int32 RallySeq, int32 ShotIndex)
{
    return (int32)HashCombine(HashCombine(GetTypeHash(MatchSeed), GetTypeHash(RallySeq)), GetTypeHash(ShotIndex));
}
```

**安全マージン**（アシスト）：誤差を加える**前**に、目標点を最も近いラインから `margin` m 内側へ移す。

## 7. ショットソルバ v2（`FTennisShotSolver`）

### 7.1 解き方の種類

| 種類 | 未知数 | 条件 | 対象 |
|---|---|---|---|
| 2 変数（余裕） | 速さ s、仰角 θ | 飛距離 = D、ネットの上の余裕 = c | Flat、Topspin、HeavyTopspin、Slice、Drop、Volley、HalfVolley |
| 2 変数（最高点） | s、θ | 飛距離 = D、最高点 = h | Lob、SliceLob |
| 1 変数 | θ | 飛距離 = D（s は固定） | Smash、Serve* |
| 方位の補正 | 方位 | 横のずれ = 0 | 全種（外側のループ、最大 3 回） |

- **ネットの上の余裕**：ネットの面を越える瞬間の `Z − R − h(Y)`
- **ボレーで打点がネットに近い**（|X| < 3 m）場合、ネットの上の余裕の条件の代わりに、「打ち出しの仰角 ≥ ネットの上端を見込む角度 + 2°」を使う

### 7.2 初期値（ウォームスタート）

1. AI のショット表（§9）に近い条件があれば、その (s, θ)
2. なければ、抗力とマグヌスを入れた放物線の近似（§7.3 の「初期値」）
3. 同じラリーの前回の解（同じ球種・近い打点）があれば、それを優先する

### 7.3 反復の規則

| 項目 | 値 |
|---|---|
| ニュートン法の最大反復 | 10 回 × 方位 3 回 |
| 収束の条件 | 飛距離の残差 < 0.05 m、条件の残差 < 0.05 m |
| 差分 | ds = 0.05 m/s、dθ = 0.002 rad の片側差分。範囲の端では内側へ取る（外へ取ると範囲に収められて微分が 0 になり、仰角の上限に張り付いた解から戻れない） |
| 打ち切り | 速さが範囲の端に 3 回続けて張り付いたら、その条件では解けないとみなして次のフォールバックへ移る（時間を使い切らない。2026-11-04 に実装で加えた） |
| 歩幅 | 全歩（ダンピングなし）から始め、後退で縮める（2026-11-04 に実装で変えた。0.8 の固定では収束が線形になり、反復が増えた） |
| 後退（バックトラック） | 全歩で試し、残差の合計が悪くなれば歩幅を半分にして試し直す（最大 3 回。最後まで悪ければ、いちばん小さい歩で進む）。2 変数の条件には高い弧と低い弧の 2 つの解があり、大きく動くと別の解へ跳ぶため（2026-11-04 に実装で加えた） |
| 初期値 | 放物線の近似から解析的に求める。抗力は、水平の速さが距離に対して指数で減る（v = v0·e^{−kx}）とみなし、マグヌスの縦の成分は重力に含める（g'）。すると軌道は u(x) = (e^{kx} − 1)/k についての放物線になり、余裕の条件は tanθ と g'/(2 s² cos²θ) の連立一次方程式で厳密に解ける（Apex は、リクエストの仰角で飛距離に届く s）。k と g' は、見積もった (s, θ) の飛行の中ほどの速度で `ComputeAccel` から測り、3 回測り直す。抗力なしの近似では、トップスピンの球がネットにかかる初期値になり、最初の歩でロブの解へ跳んでいた（2026-11-04 に実装で加えた） |
| 範囲 | s：[MinSpeed × 0.8, MaxSpeed × SpeedSkill × 1.1]、θ：[−25°, 60°] |
| 1 回の試行 | `SimulateToFirstBounce`（最大 4 s。体の判定なし。ネットの判定あり） |

### 7.4 解けない場合（`EShotFallback`）

| 段階 | 処理 | 例 |
|---|---|---|
| None | 収束した | 通常 |
| RelaxConstraint | 余裕（または最高点）の目標を半分にして解き直す | 低い打点からの深い球 |
| SpeedClamped | 速さの上限に当たった。余裕を大きくして解き直す | 溜めの強いフラット |
| Nearest | 速さの範囲の中で、飛距離の残差が最小の解を採る（ネットやアウトになりうる） | ネットの近くの低い打点からフラットで深く |

### 7.5 結果

`FShotSolveResult`（`TennisSim/Public/Shot/ShotTypes.h`。項目は [クラス図 §2](../03_UML/02_Class_Diagrams.md)）：

- `bSolved` と `Fallback`：収束したか、どの段階（§7.4）で解いたか。`Nearest` のときは `bSolved = false`
- `Launch`：打ち出しの状態（Pos = 打点、Vel、Spin、Time = インパクトの時刻）
- 予測：最初のバウンドの位置、ネットの上の余裕、飛行時間。本番と同じ `Step` で飛ばした値と一致する（D-7）
- 残差：着地点と目標の距離、条件の残差。反復の数（方位の補正を通して）

## 8. 能力値の写像

| 能力値（0〜100） | 影響する量 | 0 のとき | 50 のとき | 100 のとき |
|---|---|---|---|---|
| フォア／バック／ボレー／スマッシュ／スライス／ドロップ／ロブ | その球種の SkillSigma | 1.30 | 1.00 | 0.70 |
| 球威（フォア／バック／サーブの球威） | SpeedSkill（速さの上限の倍率） | 0.90 | 1.00 | 1.10 |
| 回転量 | SpinSkill | 0.85 | 1.025 | 1.20 |
| サーブの精度 | サーブの SkillSigma と σ_toss | 1.30・0.15 m | 1.00・0.10 m | 0.70・0.05 m |
| 安定性 | PressureF の係数 | 1.25 倍まで | 1.125 倍まで | 1.0（影響なし） |

写像はすべて線形補間（`FMath::Lerp`）。曲線で調整したくなった場合は、`UCurveFloat` に置き換える。

## 9. AI のショット表（`FTennisShotTable`）

| 項目 | 内容 |
|---|---|
| 目的 | AI が多数の候補を評価するときに、ソルバを使わずに打ち出しの近似と、ネットの上の余裕・飛行時間を得る |
| 格子 | 球種 6（Flat・Topspin・HeavyTopspin・Slice・Lob・Drop）× 打点の高さ 9（0.3〜2.3 m）× 深さ 11（2〜12 m）× 溜め 3（0・0.5・1） = 1,782 点 |
| 作成 | 試合の読み込み中に、ワーカースレッドで `FTennisShotSolver` を使って作る（約 1〜2 s）。面・球・ρ（0.02 刻み）ごとにディスクにキャッシュする |
| 引き方 | 格子の 3 次元の線形補間 |
| 精度 | 打つときは必ずソルバで解き直すので、表の誤差は AI の判断にだけ影響する |

## 10. テスト

| ID | 内容 | 合格条件 |
|---|---|---|
| T-SHOT-001 | ランダムな打点・目標・球種の 1,000 件。標本の範囲：グラウンドストロークの球種（Flat・Topspin・HeavyTopspin・Slice、溜め 0〜1）、打点はベースラインの付近（X −12.5〜−9 m、高さ 0.6〜1.4 m）、目標はサービスラインより後ろ（X 6.40〜11.5 m）。サービスラインより手前の短い球は、その球種の速さの範囲では届かないので、Drop などの別の球種が受け持つ | 95% 以上が 5 cm 以内に着地 |
| T-SHOT-002 | 1 打の計算時間 | P99 < 1.0 ms |
| T-SHOT-003 | 球種ごとに軌道の形が違う（同じ打点と目標・溜め 0.5、置いた `DA_Shot_*` の値で）。ネットの上の余裕・最高点・バウンドの後の高さの順序は HeavyTopspin > Topspin > Flat > Slice（スライスは低く飛び、低く滑る）。打ち出しの速さは Flat が最も速い（2026-11-04 に、当初の「… > Slice > Flat」から直した。§2 の余裕の目標と、スライスが低く滑るという球の性質に合わせた） | 順序が成り立つ |
| T-SHOT-004 | ネットの 0.5 m 手前・高さ 0.2 m の打点からフラットで深く（当初の「ベースラインの打点 0.2 m から」は、仰角を上げれば余裕 0.25 m で届くので解けてしまう。2026-11-04 に実装で確かめて直した） | Fallback = Nearest、目標から 1 m 以上外れる（ネットかアウト。速さの下限で余裕を取ると遠くへ飛ぶ） |
| T-SHOT-005 | サーブ 3 種（フラット・スライス・キック）を解いた打ち出しで、右利きのフラットとスライスは左に、キックは右に曲がる。左利きは逆。スライスは横の入力で左へ倒すと、より左に曲がる | 横のずれ（打ち出しの向きに対する着地点の左右）の符号と大小 |
| T-SHOT-006 | 溜め 0 → 1 で、Topspin の速さが上がり、余裕が下がる | 単調 |
| T-SHOT-010 | 誤差の分布（1 万回） | 標本の標準偏差が設定の ±3% |
| T-SHOT-011 | 同じシードで同じ誤差 | 完全一致 |
| T-SHOT-012 | Early／Late の流れの向き（右／左利き × フォア／バック） | 4 パターン |
| T-SHOT-013 | 安全マージン 0.8 m | 目標がラインから 0.8 m 内側 |
| T-SHOT-014 | ミスの原因の判定（各原因を人工的に作る） | 期待どおりの原因 |
| T-SHOT-020 | ショット表の補間と、ソルバの解の差 | ネットの余裕の差の平均 < 0.1 m |
