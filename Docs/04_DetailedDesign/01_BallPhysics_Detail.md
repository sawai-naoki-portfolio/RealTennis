# DD-01：ボール物理 詳細設計

| 項目 | 内容 |
|---|---|
| 文書 ID | DD-01 |
| 版 | 1.1（2026-11-14：ステップの通し番号 `GetStepCount`：§3.3。1.0 は 2026-11-04） |
| 対象のクラス | `TennisBallPhysics`、`FTennisBallPredictor`、`UTennisBallSimComponent`、`ATennisBall`、`ATennisCourt` |
| 要件 | FR-CRT-001〜008、FR-BALL-001〜012、FR-ENV-001〜005、NFR-DET-001〜007 |
| 根拠 | [RES-02](../00_Research/02_Ball_Physics_Research.md)、既存設計 §2〜3 |

---

## 1. 座標系と単位

| 項目 | 定義 |
|---|---|
| 原点 | ネット中央の地面（`ATennisCourt` の Transform） |
| 軸 | +X：プレイヤー 0 の側 → プレイヤー 1 の側。+Y：プレイヤー 0 から見て左。+Z：上 |
| 単位 | m、s、kg、rad/s（double） |
| 側（Side） | X < 0 がプレイヤー 0 の側（`Side = −1`）、X > 0 がプレイヤー 1 の側（`Side = +1`）。エンドチェンジでは**選手の担当する側が入れ替わる**（座標系は動かさない） |
| ワールドへの変換 | `ATennisCourt::LocalMetersToWorld`／`WorldToLocalMeters`（`Court/TennisCourt.h`）だけで行う。m × 100 = cm で、アクタの拡大・縮小は無視する（コートは常に実寸）。ほかの場所で × 100 を書かない（ARCH-03 U-2） |

ITF 2026 の規格値（コート・ネット・ポスト・スティックの寸法）と物理の定数（重力・固定ステップ 1/240 s）は、`TennisSim/Public/TennisConstants.h` の `TennisConst` にだけ置く（ARCH-03 S-5）。値と条番号はそのヘッダーと [RES-01 §1](../00_Research/01_Tennis_Rules_Research.md) を参照する。

## 2. データ構造

型の定義（項目・単位・既定値）は `TennisSim/Public/TennisSimTypes.h`（`FBallState`・`FBallParams`・`FSurfaceParams`・`FAirParams`・`FMatchConditions`・`FBallSimContext`）が正。主要な項目の関係は [クラス図 §1](../03_UML/02_Class_Diagrams.md)。既定値は [DATA-02](../05_Data/02_Parameter_Tables.md) の初期値で、本番の値はデータ（`DA_Ball_*`・`DA_Surface_*`）から入れる。

面の 2 つの式（`FSurfaceParams`）：

```
e(v_n, T)  = clamp(RestitutionRef − RestitutionSpeedSlope·(v_n − 8.27) + TempCoeff·(T − 23), 0.55, 0.92)   … RestitutionAt
patch(v_n)  = ContactPatchBase + ContactPatchPerVn · max(v_n, 0)   … ContactPatch（m）
```

- `FBallState::Time` はシミュレーションの時刻（s）。オンラインではサーバーの時刻を基準にする（DD-06 §6）
- `FBallSimContext` は今、ボール・面・空気・気温だけを持つ。恒久施設と選手の体は、それを解く作業（W-1.4 の残り・§6）で加える

## 3. 積分

### 3.1 加速度

```
v_rel = v − w(t)                               … 風（屋内・屋根の閉では 0）
S     = r·|ω| / |v_rel|                        … スピンパラメータ
C_d   = Cd0 + CdSpin · S                       … CdSpin の初期値は 0（RES-02 §2.1）
C_L   = S / (1 + 2S)                           … = 1 / (2 + 1/S)。S = 0 でも連続なので、しきい値を置かない
a     = (0, 0, −g)
      − (½ρ C_d A / m) · |v_rel| · v_rel
      + (½ρ C_L A / m) · |v_rel|² · normalize(ω × v_rel)
```

- `|v_rel|` が 1e-6 m/s 未満なら空気の力を 0 とする（0 で割らない）。`ω × v_rel` が 0（回転の軸が進む向きと平行）ならマグヌスは 0
- 実装は `TennisBallPhysics::ComputeAccel` と、空中の 1 ステップ（RK4・§3.2 の減衰・時刻）の `TennisBallPhysics::IntegrateFlight`（`Ball/TennisBallPhysics.h`、W-1.2）。ステップの中で ω は一定とする（参照シミュレーションと同じ）。`Step()` は `IntegrateFlight` のあとに衝突を解く（W-1.3〜）
- 負や NaN の Δt は呼び出し側の誤りとして、状態を変えずに `LogTennisBall` に警告を出す

### 3.2 スピンの減衰

1 ステップの終わりに `ω ×= exp(−k_ω · |v| · Δt)`（k_ω の初期値 0.005 /m。RES-02 §2.3）。

### 3.3 固定ステップの駆動（`UTennisBallSimComponent::TickComponent`）

```cpp
void UTennisBallSimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    // フレームの時間はステップの数に直すだけに使う（ARCH-03 D-3）。倍率は tennis.TimeScale（UTennisDebugSettings）
    const int32 Steps = Clock.Advance(DeltaTime, GetDefault<UTennisDebugSettings>()->GetTimeScale());
    StepCount += Steps;      // ステップの通し番号：飛んでいない間も数える（ストロークの時計：DD-03 §6.1）
    if (!bInFlight)
    {
        return;              // 状態と時刻は、飛んでいる間だけ進める
    }
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        StepOnce();          // PrevState = State → TennisBallPhysics::Step(State, SimDt, Context, Events) → 配信
    }
    UpdateOwnerLocation();   // Lerp(PrevState.Pos, State.Pos, Clock.Alpha()) をワールドへ（見た目だけの補間）
    DrawDebug();             // tennis.Debug.Trajectory
}
```

**ステップの通し番号**（W-3.5）：`GetStepCount()`（int64）は、プレイの開始からこのコンポーネントが数えたステップの数。時計（`Clock`）と通し番号は、ボールが飛んでいない間も進め、ボールの状態と時刻（`State.Time`）は飛んでいる間だけ進める。ストロークは、この番号の差だけ進む（[DD-03 §6.1](03_Player_Movement_Stroke_Detail.md)）。ボールが飛んでいる間は、番号の差 × `SimDt` が、そのまま `State.Time` の進みになる（打ち出し `Launch`（§3.4）が時刻を置き直したフレームを除く。打ち出しは番号を変えない）。T-BALL-024

**実装（W-1.6）**：フレームの時間をステップの数に直す部分は、`TennisSim` の `FFixedStepClock`（`FixedStepClock.h`）に分けた（純粋な部品で、T-BALL-020 を単体テストにする）。0.1 s での切り詰め・時間の倍率・負や NaN の値の扱い（0 とする）もここにある。

**実装（W-1.6）**：`Tennis/Ball/TennisBallSimComponent`・`TennisBall`。

- コートは `Court` のプロパティで指定する。空ならプレイの開始時にレベルの中の `ATennisCourt` を探し、なければ警告を出す（ボールは画面で動かない）
- イベントは 1 つずつ `OnBallEvent` で配信し、`LogTennisBall` に 1 行ずつ残す（`[Bounce] t=… pos=…`）。打ち出しも `[Launch]` の 1 行を残す
- 見た目の回転（ω を積分してメッシュを回す。見た目だけ）は、まだ扱わない
- デバッグの描画（W-1.8）：`tennis.Debug.Trajectory`（ARCH-03 §5.2）が 1 なら予測の軌道（黄。60 Hz に間引く）とイベントの印（バウンドは赤、ネットの通過は緑、ネットとポストは橙）、2 なら本番の軌道（水色）も重ねる。描画は `Ball/TennisBallDebugDraw` に分け、出荷版では描かない。Visual Logger には打ち出しとイベントの位置を `LogTennisBall` で残す（不具合をシードと Visual Logger で再現する：開発ルール集（非公開） §5）
- 確かめるための打ち出しは、テストマップの打球ツール（`ATennisShotTester`、FR-DBG-003）で行う（M1 の確認用の `DebugLaunchVelocity` は、M2 で打球ツールに置き換えて消した）
- メッシュのアセットはレベル・ブループリントで設定する（C++ にアセットのパスを書かない）

### 3.4 過去の時刻からの打ち出し（`Launch(LaunchState)`）

インパクトの時刻はフレームの境目と一致しない。打ち出しの時刻 `LaunchState.Time` が今のシミュレーションの時刻 `State.Time` より前のとき：

1. 状態を `LaunchState` にする
2. 打ち出す前の時刻に追いつくまで、`TennisBallPhysics::AdvanceTo` で進める（固定ステップで進め、最後の端数は端数のステップで進める）
3. 早送りの間に起きたイベント（まれ）も、時刻つきで配信する
4. 予測軌道を、打ち出しの状態から作り直す

打ち出しが 2 フレーム（`LateLaunchWarning` = 33 ms）より古い場合は、`LogTennisBall` に警告を出す（ネットワークの遅延補償では最大 150 ms まで許す）。

### 3.5 任意の時刻の状態（`GetStateAt(T)`）

- 予測軌道（§7）の 240 Hz の状態列のうち、`T` の直前の状態から、端数 `T − t_k` だけ `Step()` で進めて返す（補間ではなく、同じ物理で進める。D-7）
- インパクトの判定・ネットワークの巻き戻し・リプレイが使う

## 4. 地面のバウンド（v2）

### 4.1 アルゴリズム

[ACT-01](../03_UML/05_Activity_Diagrams.md) とコードの例（[コーディング規約 §7](../02_Architecture/03_Coding_Standards.md)）を参照。v1（既存設計）からの変更点は次の 3 つ。

| 変更 | 内容 |
|---|---|
| e(v_n, T) | 衝突速度と気温による反発（§2 の `RestitutionAt`） |
| e_t | グリップしたときの接線方向の反発。転がりに必要な撃力を `(1 + e_t)` 倍する（上限は μ·J_n のまま） |
| 球の Type | `e_court = e_surface × (e_ball / e_ball(Type 2))`。Type 1 は少し高く、高地球は低く弾む |

実装は `TennisBallPhysics::ResolveGroundBounce`（W-1.3）。基準の `e_ball(Type 2)` は `FBallParams::Type2RestitutionHard`（0.775）。

`TennisBallPhysics::Step`（[ACT-01](../03_UML/05_Activity_Diagrams.md)、W-1.3）での接地の扱い：

- ステップの終わりにボールの中心が `R` より下で、下へ動いていれば接地。接地の時刻を線形補間で求め、そこまで戻して進め直し、中心の高さを `R` にそろえてからバウンドを解く。残りの時間は続けて進める
- 法線方向の速さが **0.1 m/s 未満**の接地は弾ませず、地面に沿わせる（`Z = R`、`v_z = 0`。跳ね上がりは 0.5 mm 未満）。1 ステップの接地は**最大 4 回**で、超えたら同じく地面に沿わせる（無限のループを防ぐ）。転がりの抵抗と `Rest` のイベントは、まだ扱わない
- `Bounce` のイベントの属性は、今は時刻・位置・衝突の後の速度と回転・`NormalSpeed`・`ContactPatch`。側と `LineDistance`（§4.3）は、ラインの判定を作る作業（M5）で加える

### 4.2 面のパラメータの決め方（キャリブレーション）

```
入力：目標 CPR、目標 e（面の性格で決める：クレー 0.84、ハード 0.80、芝 0.72 …）
1. μ = 1 − (CPR − 150·(0.81 − e)) / 100
2. μ_max = 1.237 / (1 + e)   … CPR 試験の条件での転がりの上限（α = 0.55）
3. μ ≤ μ_max なら e_t = 0。μ > μ_max なら、Friction を大きめ（0.9）にして e_t = μ/μ_max − 1
4. 仮想 CPR 試験（`TennisCourtCalibration::VirtualCprTest`）で確認し、±2 に入るまで e_t を微調整する
```

初期値は [パラメータ表 §2](../05_Data/02_Parameter_Tables.md)。

仮想 CPR 試験とドロップテストの計算は `TennisSim` の `Calibration/CourtCalibration.h`（`VirtualCprTest`・`CprCategory`・`DropTestReboundCm`）。置いた `DA_Surface_*`・`DA_Ball_*` がすべて目標に入ることは、自動テスト `Tennis.Game.Data`（T-BALL-001〜004、W-1.9）で確かめる。エディタの校正のツールは作らず、このテストで代えた（[ARCH-04 §4](../02_Architecture/04_Unreal_MCP_Workflow.md) の `RunCourtPaceCalibration`）。

### 4.3 接地パッチとイン／アウト

- 判定は、接地の瞬間のボール中心の (X, Y) と、`ContactPatch(v_n)` を使う（[ルール仕様](../01_Specification/05_Rules_Specification.md) の Rule 12）
- `LineDistance`（`FBallEvent` の `Bounce` に加える属性）：最も近いラインの外縁までの符号付き距離（m、内側が正）。電子ラインコールの再現と「際どい判定」の判定に使う（|LineDistance| < 0.05 m で際どい）

### 4.4 見た目

| 項目 | 内容 |
|---|---|
| つぶれ | 0.05 s で 1.0 → (1 − 0.03·v_n) → 1.0（下限 0.7）。法線方向に縮め、横に少し広げる |
| ボールマーク | クレー：楕円のデカール（長さ = パッチ × 2 + 滑った距離）。60 秒、またはポイントが変わるまで残す。ハード・芝：短い跡（2 秒） |
| 土煙 | クレーのみ。v_n と滑りの速さで量を変える |

## 5. ネット・ポスト・恒久施設

### 5.1 ネットの形

| 区間（\|y\|） | 存在するもの | 高さ |
|---|---|---|
| 0 〜 5.029 | ネット本体（網）＋白帯 | `h(y) = 0.914 + (1.07 − 0.914) × |y| / 5.029` |
| 5.029 〜 6.399 | ネットコード（白帯）だけ。網との判定はしない（Rule 25 d） | 1.07 |
| 5.029 | シングルススティック（円柱、半径 0.0375 m、高さ 1.07 m） | — |
| 6.399 | ネットポスト（円柱、半径 0.075 m、高さ 1.095 m） | — |
| 6.399 より外 | 何もない（ポストの外を回る球は有効。Rule 25 c） | — |

### 5.2 白帯（コード）との衝突

- 白帯は X = 0、高さ h(y) の位置にある水平の円柱（半径 `r_c = 0.006 m`）として扱う
- 球の中心と円柱の軸の距離が `R + r_c` を下回ったら衝突。接触の法線方向に、反発 0.30・摩擦 0.4 の撃力で解く
- 越えたか戻ったかは、その後の軌道で自然に決まる（ネットインもネットも物理の結果）
- `NetTouch` イベントの `NetPart = Cord`
- **実装（W-1.4）**：`Private/Ball/NetCordContact`。ステップの間の動きを直線で近似し、白帯の軸（XZ 面）からの距離が `R + r_c` になる時刻を 2 次方程式で求める（速い球のすり抜けを防ぐ）。ポストの外（|Y| > 6.399）に白帯はない。接線方向の撃力は止まるところまで、回転は変えない。解いた後は、白帯にちょうど触れる位置に置き直す

### 5.3 ネット本体

- `Z + R < h(Y)` で X の符号が変わったら、ネット本体に当たったとする
- 速度：`v_x ×= −0.1`、`v_y ×= 0.3`、`v_z ×= 0.3`。スピン ×0.2。位置は X = −sign(v_x_old)·R に戻す
- `NetTouch` イベントの `NetPart = Body`

**実装（W-1.4）**：`Court/CourtGeometry.h`（`NetHeightAt`・`HasNetAt`・`HasNetBodyAt`）と、`Step` から呼ぶ `Private/Ball/NetPlaneContact`。

- ボールの中心が X = 0 を越えた時刻を線形補間で求め、そこまで戻して解く（地面との衝突と、時刻の早い順）
- ポストの外（|Y| > 6.399）を越える球には何もしない（Rule 25 c）
- 本体に当たったとするのは、本体がある位置（|Y| ≤ 5.029）で `Z + R < h(Y)` のとき。白帯に触れる球は、面に届く前に白帯の判定（§5.2）が解く
- それ以外は通過として `NetCross` を出す。`NetClearance` はボールの底とネットの上端の差（スティックとポストの間で白帯の下を通る球は負）

### 5.4 ポスト・スティック・恒久施設

- ポストとスティック：鉛直の円柱。反発 0.5、摩擦 0.2
  - **実装（W-1.4）**：`Private/Ball/NetUprightContact`。白帯と同じく、ステップの間の動きを直線で近似して触れ始める時刻を求める（円柱との共通の計算は `Private/Ball/ContactMath`）。上端より上を通る球は当たらない（上面は扱わない）。`PostHit` のイベントの `NetPart` は `SinglesStick` か `Post`。回転は変えず、`bNetTouched` は立てない（ネットではなくポスト。ルールはイベントで判定する）。ポストの高さ 1.095 m は `TennisConst::PostHeight`
- 恒久施設（`UTennisFixtureComponent`）：箱または平面。反発 0.4。`FixtureHit` イベントの後は、ボールをデッドにする（ルールの判定は Rule 13 による）
- 観客席・フェンスはコートの外側にだけある。天井（屋内）は高さ 12〜18 m

## 6. 選手の体との衝突

| 項目 | 内容 |
|---|---|
| 形 | 選手ごとに 2 つのカプセル：胴体（骨盤 → 首、半径 0.17 m）、脚（骨盤 → 足首の中点、半径 0.14 m）。腕と頭は胴体のカプセルに含める |
| 更新 | キャラクターの Tick の後に、骨の位置から計算して `UTennisBallSimComponent::SetBodies()` に渡す。シミュレーション時刻のずれ（最大 1 フレーム）は無視する |
| 除外 | ラケットは除外する（打球はインパクトの判定で行う）。打った直後 0.15 s は、打った本人の体を判定しない（ラケットの振り抜きでの誤判定を防ぐ） |
| 結果 | `BodyHit` イベント。ボールの速度は 0.2 倍にして落とす（見た目） |

## 7. 予測（`FTennisBallPredictor`）

| 項目 | 値 |
|---|---|
| 呼ぶ時 | Launch のたびに 1 回。ネットワークでバウンドの補正が入ったとき |
| 打ち切り | 4.0 s、またはバウンド 2 回、または静止 |
| 保存 | 240 Hz の全状態（最大 960 個、約 80 KB）＋イベントの一覧。60 Hz の間引きの配列（AI・UI 用） |
| 体の衝突 | 予測では判定しない（選手が動くため）。本番だけで判定する |
| 費用 | 約 0.3 ms（推奨環境） |

**実装（W-1.5）**：`Ball/TennisBallPredictor.h`。

- `FTrajectory` は 240 Hz の全状態（`FBallState`。`States[0]` が打ち出し）とイベントの一覧を持つ。60 Hz の間引きの配列（AI・UI 用）は、それを使う作業（M3・M6）で加える（YAGNI）。そのため今の `FPredictOptions` は `MaxTime`・`MaxBounces` だけ
- 打ち切りは時間とバウンドの数。静止での打ち切りは、`Rest` のイベントを作る作業で加える
- 不正な打ち切りの値（0 以下・NaN）は、既定値にして警告を出す
- `FTennisBallPredictor::GetStateAt`（§3.5）：範囲の外（打ち出しより前・最後の状態より後）と NaN は `false`
- `TennisBallPhysics::AdvanceTo`（§3.4 の早送り）：固定ステップで進め、最後の端数を端数のステップで進め、時刻をちょうど目標にする。過去へ戻す・10 s より先・NaN は `false`（状態は変えない）。`Launch` の本体と、フレームの時間の積み立て（T-BALL-020・022）は、W-1.6 の `UTennisBallSimComponent` で使う

## 8. イベントの配信

| イベント | 発行の条件 | 主な属性 |
|---|---|---|
| `Launch` | 打球・サーブ | Seq、打者、打ち出しの状態、球種 |
| `Bounce` | 地面との衝突 | 位置、v_n、側、面、LineDistance、ContactPatch |
| `NetCross` | ネットの面を越えた | 位置、ネットの上の余裕（m） |
| `NetTouch` | ネットの本体・白帯 | 部位、越えたか（後で確定） |
| `PostHit` | ポスト・スティック | 種類 |
| `FixtureHit` | 恒久施設 | 種類 |
| `BodyHit` | 選手の体 | 選手 |
| `Rest` | 速さ < 0.3 m/s が 0.5 s 続いた | — |

- 1 ステップの中のイベントは時刻の順に並べて配信する
- 配信の順番：ルールの判定（`UTennisMatchFlowComponent`）→ 統計 → 演出。演出が先に反応してルールと食い違うことを防ぐ

## 9. トス

- サーブのトスも同じ物理で投げる（スピン 0〜2 rad/s の小さい値）
- 初速は、頂点の高さ `h_apex` と、打点の前方・横の目標位置から逆算する：`v_z = √(2g·(h_apex − h_release))`（抗力の補正は 1 回の反復で行う）
- ぶれ：前後と左右に `N(0, σ_toss)`（σ_toss = 0.05〜0.15 m。サーブの精度の能力値による）

## 10. 環境

| 項目 | 式・値 |
|---|---|
| 空気密度 | `ρ = p / (287.05 × (T + 273.15))`。気圧は標高から ISA の式：`p = 101325 × (1 − 2.25577e−5 × h)^5.25588` |
| 風 | `w(t) = w_mean + G·(0.6 sin(2πt/T1 + φ1) + 0.4 sin(2πt/T2 + φ2))`、T1 = 13 s、T2 = 7 s、φ はポイントのシードから。G は突風の強さ（0〜0.5 × |w_mean|） |
| ソルバの知る風 | `w_mean` だけ（FR-ENV-004） |
| 屋根の閉・屋内 | 風 0。気温は 21 ℃ 固定 |

**実装（W-1.7）**：`FMatchConditions`（`TennisSimTypes.h`）。`ComputeAir()` が ρ と平均の風を返し、`EffectiveTempC()` が実際の気温（屋根を閉じれば 21 ℃）を返す。入力は検証する：標高は −500〜4,000 m、気温は −10〜50 ℃ に丸め、NaN は既定値（海面・20 ℃・風 0）にする。既定の環境の ρ（1.2041）は、`FAirParams` の既定値（DATA-02 §1 の 1.204）と一致する。突風は `FAirParams::WindAt(t)`：G は平均の風に対する割合（`GustFactor`、0〜0.5 に丸める）で、風の向きは変えない。位相 φ1・φ2 は `ComputeAir(PointSeed)` の `FRandomStream` から決める。`ComputeAccel` は時刻を受け取り、RK4 の各段の時刻の風を使う。ソルバが知る風は `WindMean` だけ（FR-ENV-004。ソルバの作業で使う）。

## 11. 性能の目安

| 処理 | 回数 | 費用（推奨環境） |
|---|---|---|
| `Step()` | 240 回/s | 1 回あたり約 0.2 µs |
| 予測 | 打球ごと | 約 0.3 ms |
| ソルバの試行 | 打球ごとに 30〜60 回 | 計 0.5〜1.0 ms |

## 12. テスト

| ID | 内容 |
|---|---|
| T-BALL-001〜012 | [RES-02 §6](../00_Research/02_Ball_Physics_Research.md) の検証項目 |
| T-BALL-020 | 30／60／144 fps のフレーム時間の列で、同じ打ち出しの着地点が 1 mm 以内で一致する |
| T-BALL-021 | 予測と本番の状態が、同じステップ数で完全に一致する |
| T-BALL-022 | `Launch(過去の時刻)` の早送りの結果が、最初からその時刻に打ち出したものと一致する |
| T-BALL-023 | `GetStateAt(T)` が、T まで本番で進めた状態と一致する |
| T-BALL-024 | ステップの通し番号：飛んでいない間も数え、状態と時刻は変えない。飛んでいる間は、フレームごとの番号の差 × `SimDt` だけ時刻が進む（フレームの分け方・時間の倍率によらない） |
| T-BALL-030 | ネット本体：高さ 0.5 m で X 方向に打つ → 自コートに落ちる |
| T-BALL-031 | 白帯：越える／戻るの両方が、初期条件を少し変えると起きる（同じ条件では常に同じ） |
| T-BALL-032 | ポストの外を回る球：ネットの判定がなく、相手コートに落ちる |
| T-BALL-033 | スティックとポストの隙間：コードの下を通る球はネットに触れない |
| T-BALL-034 | シングルススティックへの直撃で反射する |
| T-BALL-035 | 1 ステップ内の 2 回の衝突（白帯 → 地面）を時刻の順に処理する |
| T-BALL-040 | 標高・気温から計算した ρ が §10 の式と一致する |
| T-BALL-041 | 風 3 m/s の横風でラリーの球が 0.2〜0.8 m 流れる |
| T-BALL-050 | 体のカプセルに当たると BodyHit になり、打った本人の 0.15 s 以内は除外される |
