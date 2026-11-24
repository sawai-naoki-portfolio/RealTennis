# UE5 テニスゲーム 技術設計書

> 対象：UE5（5.4 以降推奨）／C++ 主体＋Blueprint で調整
> 目的：1on1 テニス（シングルス）を、物理的にもっともらしく、かつ遊びやすく作るための具体的な実装方針
> 表記：物理計算は **メートル・秒・kg**、UE への受け渡し時に **×100（cm）** に変換する

---

## 0. 設計の基本方針

| 項目 | 方針 | 理由 |
|---|---|---|
| ボール物理 | **Chaos を使わず自前シミュレーション** | 剛体ソルバではマグヌス効果・スピン付きバウンドを制御しにくい。決定論的な軌道予測（AI・アニメ同期・ネット同期）に同一コードを使いたい |
| 打球の生成 | **「狙った着地点」から初速を逆算**（ショットソルバ） | 「ラケットに当たった結果を物理で計算」すると狙い通りに飛ばず、ゲームにならない |
| 当たり判定 | **ラケットとボールの物理衝突を使わない**。インパクトフレームで距離判定 | フレームレート依存のすり抜けを避け、打感を設計で保証する |
| アニメ | Montage＋**Motion Warping**＋再生速度補正で「ボールが来る瞬間」にインパクトを合わせる | 見た目と結果の一致 |
| 時間 | ボールは**固定タイムステップ（240Hz）**、描画は補間 | 可変 FPS でも軌道が変わらない |
| データ | ショット種別・コート面・難易度は **DataAsset / DataTable** | チューニングをコードから切り離す |

---

## 1. モジュール構成とクラス一覧

```
Source/Tennis/
├─ Core/
│   ├─ TennisTypes.h            // FBallState, FBallLaunch, ETennisShotType など
│   ├─ TennisCourt.h/.cpp       // ATennisCourt：コート寸法・座標変換・ライン判定
│   └─ TennisSettings.h         // UDeveloperSettings：定数（重力・空気密度など）
├─ Ball/
│   ├─ TennisBallPhysics.h/.cpp // 純粋関数：加速度・積分・バウンド・ネット衝突
│   ├─ TennisBallSimComponent   // 固定ステップ駆動・イベント発火
│   ├─ TennisBallPredictor      // 未来軌道のサンプル列を生成
│   └─ ATennisBall              // 見た目（Mesh, Niagara, Audio）
├─ Shot/
│   ├─ TennisShotSolver.h/.cpp  // 着地点→初速の逆算
│   ├─ UShotTypeData            // DataAsset：ショット種別パラメータ
│   └─ TennisShotQuality        // タイミング・体勢からエラー量を算出
├─ Player/
│   ├─ ATennisCharacter
│   ├─ UTennisMovementComponent // UCharacterMovementComponent 派生
│   ├─ UTennisStrokeComponent   // ストローク選択・Montage 再生・インパクト判定
│   └─ UTennisReachComponent    // 到達可能な打点の探索
├─ Rules/
│   ├─ ATennisGameMode / ATennisGameState
│   └─ FTennisScore             // 得点ロジック（純粋データ＋関数）
├─ AI/
│   ├─ ATennisAIController
│   └─ StateTree タスク群         // Recover / Intercept / ChooseShot / Serve
└─ Net/
    └─ TennisNetTypes.h         // FBallLaunchEvent など複製用構造体
```

ボールのシミュレーションは **`TennisBallPhysics` の純粋関数群**として実装し、以下の 4 箇所から同じコードを呼ぶ：

1. 本番シミュレーション（`UTennisBallSimComponent`）
2. 軌道予測（AI・プレイヤーの打点探索・着地点マーカー）
3. ショットソルバ（逆算時の試行シミュレーション）
4. ネットワーク（クライアントが打球イベントから軌道を再生）

---

## 2. 座標系とコート

### 2.1 コート局所座標

`ATennisCourt` のトランスフォームを原点にして、**ネット中央の地面 = (0,0,0)** とする。

- X：コートの長手方向（+X がプレイヤー A 側からみて相手側）
- Y：横方向
- Z：上

ボールの物理はすべて**コート局所座標（m）**で計算する。描画時に `CourtTransform.TransformPosition(PosMeters * 100.f)` で変換する。

### 2.2 公式寸法（ITF 規格）

| 名称 | 値 |
|---|---|
| コート全長 | 23.77 m（半長 11.885 m） |
| シングルス幅 | 8.23 m（半幅 4.115 m） |
| ダブルス幅 | 10.97 m（半幅 5.485 m） |
| サービスライン（ネットから） | 6.40 m |
| ネット高さ（中央） | 0.914 m |
| ネット高さ（ポスト） | 1.07 m |
| シングルススティック位置 | シングルスサイドラインの 0.914 m 外側 |
| ライン幅 | 5 cm（ベースラインは最大 10 cm） |

```cpp
// TennisCourt.h
USTRUCT(BlueprintType)
struct FCourtDimensions
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) float HalfLength      = 11.885f;
    UPROPERTY(EditAnywhere) float HalfWidthSingles = 4.115f;
    UPROPERTY(EditAnywhere) float ServiceLineX    = 6.40f;
    UPROPERTY(EditAnywhere) float NetHeightCenter = 0.914f;
    UPROPERTY(EditAnywhere) float NetHeightPost   = 1.07f;
    UPROPERTY(EditAnywhere) float NetPostY        = 4.115f + 0.914f; // シングルススティック
};
```

### 2.3 ネット形状

ネットはセンターストラップで中央が下がり、**ポストから中央へほぼ直線**になる。区分線形で近似する。

```cpp
float NetHeightAt(float Y, const FCourtDimensions& D)
{
    const float T = FMath::Clamp(FMath::Abs(Y) / D.NetPostY, 0.f, 1.f);
    return FMath::Lerp(D.NetHeightCenter, D.NetHeightPost, T);
}
```

### 2.4 イン／アウト判定

ボールがラインに少しでも触れればイン。接地の瞬間のボール中心の投影点を `P` とし、接地面の半径 `ContactPatch`（約 1.5 cm。速いボールほど潰れて大きくなる）を足して判定する。

```cpp
bool IsInside(const FVector2D& P, const FBox2D& Area, float ContactPatch = 0.015f)
{
    return P.X >= Area.Min.X - ContactPatch && P.X <= Area.Max.X + ContactPatch
        && P.Y >= Area.Min.Y - ContactPatch && P.Y <= Area.Max.Y + ContactPatch;
}
```

`Area` は判定の種類ごとに用意する：シングルスコート（相手側半面）、サービスボックス（デュース／アドバンテージ側）。

---

## 3. ボール物理

### 3.1 状態

```cpp
USTRUCT()
struct FBallState
{
    GENERATED_BODY()
    FVector Pos   = FVector::ZeroVector; // m（コート局所）
    FVector Vel   = FVector::ZeroVector; // m/s
    FVector Spin  = FVector::ZeroVector; // 角速度 rad/s（ベクトル）
    double  Time  = 0.0;                 // シミュレーション時刻（サーバー時刻基準）
    int32   BounceCount = 0;
    bool    bNetTouched = false;
};

USTRUCT()
struct FBallParams
{
    GENERATED_BODY()
    float Mass    = 0.0577f;  // kg（規格 56.0–59.4 g）
    float Radius  = 0.0335f;  // m（直径 6.54–6.86 cm）
    float Cd      = 0.55f;    // 抗力係数
    float AirRho  = 1.21f;    // kg/m^3
    float InertiaAlpha = 0.55f; // I = α m r^2（薄肉中空球の実測値に近い）
    float SpinDecayTau = 5.0f;  // s：飛行中のスピン減衰の時定数（要調整）
    float Gravity = 9.81f;
};
```

### 3.2 働く力

| 力 | 式 | 備考 |
|---|---|---|
| 重力 | `a_g = (0,0,-g)` | |
| 空気抵抗 | `a_d = -k_d · |v| · v`、`k_d = ½ρ·C_d·A / m` | 規格球で `k_d ≈ 0.0203 /m`、終端速度 約 22 m/s |
| マグヌス | `a_m = (½ρ·C_L·A·|v|² / m) · normalize(ω × v)` | `C_L = 1 / (2 + v/(r·|ω|))`（スピンパラメータ `S = rω/v` による近似） |

**目安**：30 m/s・トップスピン 2400 rpm（約 250 rad/s）のとき `S ≈ 0.28`、`C_L ≈ 0.18`、マグヌス加速度は約 6 m/s²（重力の約 0.6 倍、下向き）。トップスピンのボールが「落ちる」のはこの力による。

### 3.3 スピンベクトルの作り方（UE は左手座標系だが問題にならない）

ω×v も、後述のバウンドで使う ω×r も、`FVector::CrossProduct` という**同じ代数**で計算する限り物理的に一貫する。左手座標系かどうかは結果に影響しない。ショット種別からスピンベクトルを作るヘルパーを1つだけ用意し、テストで向きを保証する。

```cpp
// 進行方向 VDir（水平）に対して：
//  トップスピン：ω = +RPM * (Up × VDir)  → ω×v は下向き（ボールが沈む）
//  バックスピン：ω = -RPM * (Up × VDir)  → ω×v は上向き（ボールが浮く）
//  サイドスピン：ω = ±RPM * Up           → 左右に曲がる
FVector MakeSpin(const FVector& VDir, float TopRPM, float SideRPM)
{
    const FVector Up(0, 0, 1);
    const FVector H = FVector(VDir.X, VDir.Y, 0).GetSafeNormal();
    const float RadPerRPM = 2.f * PI / 60.f;
    return FVector::CrossProduct(Up, H) * (TopRPM * RadPerRPM)
         + Up * (SideRPM * RadPerRPM);
}
```

> 検証（自動テスト）：`TopRPM>0` で水平に打ち出したとき、`Accel().Z < -g` になること。

### 3.4 加速度関数と積分（RK4・固定ステップ）

```cpp
// TennisBallPhysics.cpp
static FVector ComputeAccel(const FVector& V, const FVector& W, const FBallParams& P)
{
    const float A  = PI * P.Radius * P.Radius;
    const float Kd = 0.5f * P.AirRho * P.Cd * A / P.Mass;
    FVector Acc(0, 0, -P.Gravity);

    const float Speed = V.Size();
    if (Speed < 1e-3f) return Acc;

    Acc += -Kd * Speed * V;                                   // 空気抵抗

    const float WMag = W.Size();
    if (WMag > 1e-2f)
    {
        const float S  = P.Radius * WMag / Speed;
        const float Cl = 1.f / (2.f + 1.f / S);
        const float Km = 0.5f * P.AirRho * Cl * A / P.Mass;
        const FVector Dir = FVector::CrossProduct(W, V).GetSafeNormal();
        Acc += Km * Speed * Speed * Dir;                      // マグヌス
    }
    return Acc;
}

void IntegrateRK4(FBallState& S, float Dt, const FBallParams& P)
{
    const FVector W = S.Spin; // 1ステップ中はスピン一定とみなす
    const FVector p0 = S.Pos, v0 = S.Vel;

    const FVector a1 = ComputeAccel(v0,                W, P);
    const FVector v1 = v0 + a1 * (Dt * 0.5f);
    const FVector a2 = ComputeAccel(v1,                W, P);
    const FVector v2 = v0 + a2 * (Dt * 0.5f);
    const FVector a3 = ComputeAccel(v2,                W, P);
    const FVector v3 = v0 + a3 * Dt;
    const FVector a4 = ComputeAccel(v3,                W, P);

    S.Pos = p0 + (Dt / 6.f) * (v0 + 2.f * v1 + 2.f * v2 + v3);
    S.Vel = v0 + (Dt / 6.f) * (a1 + 2.f * a2 + 2.f * a3 + a4);
    S.Spin *= FMath::Exp(-Dt / P.SpinDecayTau);
    S.Time += Dt;
}
```

**固定ステップの駆動**（`UTennisBallSimComponent::TickComponent`）

```cpp
static constexpr float SimDt = 1.f / 240.f;

void UTennisBallSimComponent::TickComponent(float DeltaTime, ...)
{
    Accumulator += FMath::Min(DeltaTime, 0.1f); // スパイク対策
    while (Accumulator >= SimDt)
    {
        PrevState = State;
        StepOnce(State, SimDt);   // 積分＋衝突＋イベント発火
        Accumulator -= SimDt;
    }
    const float Alpha = Accumulator / SimDt;
    const FVector RenderPos = FMath::Lerp(PrevState.Pos, State.Pos, Alpha);
    BallActor->SetActorLocation(Court->LocalMetersToWorld(RenderPos));
}
```

- TickGroup は `TG_PrePhysics`。キャラクターより先に更新し、AI・ストローク判定が最新のボールを参照できるようにする。
- 240Hz・RK4 なら 1 打（約 1.5 秒）は約 360 ステップで済み、コストは無視できる。

### 3.5 地面バウンド（摩擦とスピンの相互作用）

実際のボールは、**滑りながら跳ねる**か、**途中で転がり状態に移る**かのどちらかになる。これを撃力（インパルス）で解く。

1. 法線方向：`J_n = -(1+e) · m · v_n`（`v_n < 0`）
2. 接触点の速度：`v_c = v + ω × r_c`、`r_c = -r·n`。接線成分を `v_ct` とする
3. 転がり状態にするのに必要な接線方向の撃力：`J_roll = -v_ct · m·α/(1+α)`
4. 摩擦の上限：`|J_t| ≤ μ · J_n`。超えた場合は滑りながら跳ねる
5. 速度とスピンを更新：`v += (J_n·n + J_t)/m`、`ω += (r_c × J_t)/I`

```cpp
void ResolveGroundBounce(FBallState& S, const FBallParams& P, const FSurfaceParams& Surf)
{
    const FVector N(0, 0, 1);
    const float Vn = FVector::DotProduct(S.Vel, N);
    if (Vn >= 0.f) return;

    const float I  = P.InertiaAlpha * P.Mass * P.Radius * P.Radius;
    const FVector Rc = -P.Radius * N;

    // 法線方向
    const float E  = Surf.RestitutionAt(-Vn); // 衝突速度依存にしてもよい
    const float Jn = -(1.f + E) * P.Mass * Vn;

    // 接線方向（接触点の滑り速度）
    const FVector Vc  = S.Vel + FVector::CrossProduct(S.Spin, Rc);
    const FVector Vct = Vc - FVector::DotProduct(Vc, N) * N;

    const float  a     = P.InertiaAlpha;
    FVector      Jt    = -Vct * (P.Mass * a / (1.f + a)); // 転がりに必要な撃力
    const float  JtMax = Surf.Friction * Jn;
    if (Jt.Size() > JtMax) Jt = Jt.GetSafeNormal() * JtMax; // 滑りながら跳ねる

    S.Vel  += (Jn * N + Jt) / P.Mass;
    S.Spin += FVector::CrossProduct(Rc, Jt) / I;
    S.BounceCount++;
}
```

**接地の瞬間を正確に求める**：1 ステップ内で `Pos.Z - r` が負になったら、線形補間で接地時刻 `t*` を求める。`t*` まで進めてバウンドを解き、残り時間を再積分する。こうするとバウンド地点がフレームレートに依存しない。

### 3.6 コート面のパラメータ（初期値・要調整）

| 面 | 反発係数 e | 摩擦 μ | 体感 |
|---|---|---|---|
| ハード | 0.75 | 0.60 | 標準 |
| クレー | 0.72 | 0.80 | 高く弾み、遅い。スピンが強く効く |
| 芝 | 0.70 | 0.45 | 低く滑る。速い |
| 室内カーペット | 0.73 | 0.50 | やや速い |

> 参考：ITF のボール規格では、254 cm から落とすと 135〜147 cm 跳ね返る。これは e ≈ 0.73〜0.76 に相当し、ドロップテストの自動テストでこの範囲に入ることを確認する。

### 3.7 ネット衝突

ネット面は `X = 0` の鉛直平面で、高さは `NetHeightAt(Y)`。1 ステップで X の符号が変わったら、交差点 `(0, Y, Z)` を求める。

| 条件 | 処理 |
|---|---|
| `Z + r < h(Y)` | **ネット本体に当たる**：X 方向の速度を `-0.1` 倍、Y・Z を `0.3` 倍に落とし、手前に落ちる |
| `h(Y) - r ≤ Z ≤ h(Y) + r` | **白帯（コード）に接触**：コードを半径 5 mm の円柱とみなし、球と線分の衝突として反発 0.3・摩擦ありで解く。越えればネットイン、戻ればネット |
| `Z - r > h(Y)` | 通過。`NetClearance = Z - r - h(Y)` を記録（ショット品質の評価やリプレイ表示に使う） |

見た目：ネットはマテリアルのワールドポジションオフセット（WPO）で揺らす。衝突点と強さをマテリアルパラメータで渡し、減衰する波として揺らす。Chaos Cloth より安く、十分に見える。

### 3.8 軌道予測（Predictor）

```cpp
struct FTrajectorySample { float T; FVector Pos; FVector Vel; int32 Bounce; };

// 現在状態から MaxTime 秒先まで、固定ステップで同じ物理を回してサンプル列を返す
void PredictTrajectory(const FBallState& Start, float MaxTime, int32 MaxBounces,
                       TArray<FTrajectorySample>& Out, float SampleDt = 1.f/60.f);
```

- 打球のたびに 1 回だけ計算してキャッシュする。イベント駆動なので毎フレーム再計算しない
- 用途：着地点マーカー、AI の移動目標、打点の探索、ストロークアニメの同期

---

## 4. ショットソルバ（着地点 → 初速）

### 4.1 考え方

プレイヤーの入力は「どこに、どの球種で、どれくらいの強さで打つか」。これを**ボールの初期状態**（位置・速度・スピン）に変換する。

未知数は **速さ s** と **仰角 θ**（方位角は別ループで補正）。条件は球種ごとに 2 つ課す。

| 球種 | 条件1 | 条件2 | スピン（rpm） |
|---|---|---|---|
| フラット | 着地距離 = D | ネット通過高さ = h_net + 0.3 m | Top 300 |
| トップスピン | 着地距離 = D | ネット通過高さ = h_net + 1.0 m | Top 2500 |
| スライス | 着地距離 = D | ネット通過高さ = h_net + 0.4 m | Back 1500 |
| ロブ | 着地距離 = D | 最高到達点 = 6〜9 m | Top 1500 |
| ドロップ | 着地距離 = D（ネット際） | ネット通過高さ = h_net + 0.25 m | Back 2000 |
| サーブ | 着地点 = 目標 | 速さ s を固定（溜めで決める）→ θ だけを 1 次元で解く | 種類別 |
| スマッシュ | 着地点 = 目標 | 速さ固定・下向きの θ を解く | Top 500 |

### 4.2 「最初のバウンドまで」の試行シミュレーション

```cpp
struct FFlightResult
{
    bool    bLanded = false;
    bool    bHitNet = false;
    FVector LandPos;          // 最初の接地点
    float   NetCrossZ = 0.f;  // X=0 を通過したときのボール下端の高さ
    float   ApexZ = 0.f;
    float   FlightTime = 0.f;
};
FFlightResult SimulateToFirstBounce(const FBallState& Launch, const FBallParams& P,
                                    const FCourtDimensions& D, float MaxTime = 4.f);
```

### 4.3 ニュートン法（2変数）

```cpp
bool FTennisShotSolver::Solve(const FShotRequest& Req, FBallState& OutLaunch)
{
    // 方位：打点→目標 の水平方向
    FVector2D Dir2 = FVector2D(Req.Target - Req.Origin).GetSafeNormal();
    const float D  = FVector2D::Distance(FVector2D(Req.Origin), FVector2D(Req.Target));

    // 初期値：空気抵抗なしの放物線から推定
    float s = Req.Type->InitialSpeedGuess;           // 例 25 m/s
    float th = FMath::DegreesToRadians(Req.Type->InitialElevationDeg); // 例 8°

    for (int AzIter = 0; AzIter < 3; ++AzIter)        // 方位補正ループ（サイドスピン・風）
    {
        for (int It = 0; It < 10; ++It)
        {
            auto Eval = [&](float S, float Th, float& R0, float& R1)
            {
                FBallState L = MakeLaunch(Req, Dir2, S, Th);
                FFlightResult F = SimulateToFirstBounce(L, Params, Court);
                const float Range = FVector2D::Distance(FVector2D(Req.Origin), FVector2D(F.LandPos));
                R0 = Range - D;
                R1 = Req.Type->ConstraintValue(F) - Req.Type->ConstraintTarget(Req); // ネット高さ or 頂点
            };
            float r0, r1; Eval(s, th, r0, r1);
            if (FMath::Abs(r0) < 0.05f && FMath::Abs(r1) < 0.05f) break;

            // 差分ヤコビアン
            const float ds = 0.05f, dth = 0.002f;
            float a0, a1, b0, b1;
            Eval(s + ds, th, a0, a1);
            Eval(s, th + dth, b0, b1);
            const float J00 = (a0 - r0)/ds, J01 = (b0 - r0)/dth;
            const float J10 = (a1 - r1)/ds, J11 = (b1 - r1)/dth;
            const float Det = J00*J11 - J01*J10;
            if (FMath::Abs(Det) < 1e-6f) break;

            float dS  = (-r0*J11 + r1*J01) / Det;
            float dTh = (-J00*r1 + J10*r0) / Det;
            // ダンピングとクランプ
            s  = FMath::Clamp(s  + 0.8f*dS,  Req.Type->MinSpeed, Req.Type->MaxSpeed);
            th = FMath::Clamp(th + 0.8f*dTh, FMath::DegreesToRadians(-20.f), FMath::DegreesToRadians(60.f));
        }
        // 横ずれ分だけ方位を回す
        FBallState L = MakeLaunch(Req, Dir2, s, th);
        FFlightResult F = SimulateToFirstBounce(L, Params, Court);
        const FVector2D Err = FVector2D(Req.Target) - FVector2D(F.LandPos);
        const float Lateral = FVector2D::CrossProduct(Dir2, Err);
        Dir2 = Dir2.GetRotated(FMath::RadiansToDegrees(FMath::Atan2(Lateral, D)));
    }
    OutLaunch = MakeLaunch(Req, Dir2, s, th);
    return ValidateNoNet(OutLaunch);
}
```

**解けない場合の扱い**（体勢が悪い、打点が低すぎる、など）：

1. ネット通過高さの条件を段階的に緩める
2. それでも駄目なら速さの上限内で最も近い解を採用する → ネットやアウトになる。物理的な結果としてミスが起きる

計算コストは 1 打あたり約 30〜60 回の試行シミュレーション。1 回 400 ステップとしても数万ステップで、1 ms 未満に収まる。

### 4.4 速さの決め方

```
s_target = lerp(Type.MinSpeed, Type.MaxSpeed, Charge)
         + IncomingSpeed * Type.PaceReuseFactor   // 相手の球威を利用（0.1〜0.3）
         - PosturePenalty                          // 走りながら・届かない打点
```

条件 1・2 を満たす解が s_target から離れすぎる場合は、ニュートン法の初期値に s_target を与え、仰角側で調整させる。

---

## 5. 入力とショット品質

### 5.1 Enhanced Input

| InputAction | 型 | 用途 |
|---|---|---|
| IA_Move | Axis2D | 移動 |
| IA_Aim | Axis2D | 狙い（移動と同じスティックでも可。押下中の入力を狙いとして解釈） |
| IA_Flat / IA_Topspin / IA_Slice / IA_Lob | Button（Pressed / Released） | 球種の選択と溜め |
| IA_Drop | 修飾キー＋スライス | ドロップ |

InputMappingContext は「ラリー中」「サーブ中」「メニュー」で切り替える（`AddMappingContext` の優先度で上書き）。

### 5.2 溜めとタイミング

1. **ボタンを押す** → ストロークの予約。球種が決まり、溜め（Charge 0→1）が始まる
2. **離す**か、**インパクト時刻**に達する → 打球を確定
3. タイミング誤差 `Δt = 離した時刻 - 理想時刻（インパクトの 0.1〜0.2 秒前）`

```
Quality = 1 - clamp(|Δt| / Window, 0, 1)      // Window = 0.15 s
```

| 判定 | |Δt| | 表示 |
|---|---|---|
| Perfect | ≤ 0.04 s | 打点エフェクト＋SE |
| Good | ≤ 0.09 s | |
| Early / Late | それ以上 | 狙いが流れる（Early はクロス方向、Late は逆クロス方向） |

### 5.3 エラーモデル（狙いのブレ）

狙いの点 `Target` に、楕円状のガウス分布の誤差を加えてからソルバに渡す。**誤差は必ず着地点に対して与える**（初速にノイズを入れると、ネットとアウトの比率が制御できない）。

```
σ_depth = Type.BaseSigma * (1 + 2·(1-Quality)) * PostureFactor * PaceFactor
σ_side  = σ_depth * 0.6
PostureFactor = 1 + 0.8 * (走行速度/最高速度) + 1.5 * StretchRatio
PaceFactor    = 1 + 0.02 * max(0, IncomingSpeed - 20)
```

- 乱数は `FRandomStream`。シードは `RallySeq` と `ShotIndex` から作る。サーバーとクライアントで同じ誤差を再現するため（§9）
- 球種ごとの `BaseSigma`：フラット 0.9 m、トップスピン 0.6 m、スライス 0.7 m、ロブ 1.2 m、ドロップ 0.5 m
- ライン際を狙うほど期待値が下がるので、狙いを内側に自動補正する「セーフティマージン」をオプションで用意する（初心者向けアシスト）

---

## 6. キャラクター移動とストローク

### 6.1 移動

`UCharacterMovementComponent` 派生で十分。テニスでは即応性が最優先なので、移動はルートモーションに頼らずコードで制御する。

| パラメータ | 目安 |
|---|---|
| 最高速度（スプリント） | 6.0 m/s（600 cm/s） |
| 通常速度 | 4.0 m/s |
| 加速度 | 25 m/s² |
| 減速（ブレーキ） | 35 m/s² |
| 方向転換時の減速 | `GroundFriction` を高めにし、`bUseSeparateBrakingFriction` を有効にする |

- **スプリットステップ**：相手のインパクト時に 0.15 秒だけ小ジャンプのモーションを入れ、直後 0.3 秒の加速度を 1.3 倍にする。リズムを作ると同時に、反応の速さを報酬にする
- **移動アシスト**（設定で ON/OFF）：予測した理想打点に向かう速度を、スティック入力に `AssistWeight` で合成する

### 6.2 到達可能な打点の探索（UTennisReachComponent）

予測軌道のサンプル `(t_i, p_i)` から、打点として使えるものを選ぶ。

1. 高さ帯でフィルタする：`0.25 m ≤ Z ≤ 1.7 m`（通常ストローク）、`Z > 2.0 m`（スマッシュ候補）、バウンド前（ボレー）
2. 到達時間：距離 `d`、最高速度 `v`、加速度 `a` のとき
   - `d ≤ v²/2a` なら `t_req = √(2d/a)`
   - それ以外は `t_req = v/a + (d - v²/2a)/v`
3. `t_req + ReactionDelay + StrokeWindup ≤ t_i` を満たす候補にスコアを付ける
   - 打ちやすい高さ（腰〜胸：0.8〜1.2 m）を優遇
   - 体の横 0.9〜1.1 m（ラケットの長さ＋腕）にボールが来る位置を目標にする
   - 前で取る（早い t）ほど攻撃的、後ろで取るほど安全
4. 間に合わないときは**ストレッチ／ダイブ**のストロークを選ぶ（品質に大きなペナルティ）

### 6.3 ストロークの選択テーブル（DataTable）

キーは「サイド × 高さ帯 × 到達 × 球種」。

| 列 | 例 |
|---|---|
| Side | Forehand / Backhand（ボールがキャラの右か左か） |
| HeightBand | Low（<0.6） / Mid（0.6–1.2） / High（1.2–1.8） / Overhead |
| Reach | Normal / Stretch / Dive |
| ShotFamily | Drive / Slice / Lob / Volley / Smash |
| Montage | AM_FH_Mid_Drive など |
| ImpactTime | Montage 内のインパクト時刻（秒） |
| SweetSpotOffset | インパクト時の「ルート → ラケットのスイートスポット」の相対トランスフォーム |
| WindupTime | 動き出しからインパクトまでの時間 |

`ImpactTime` と `SweetSpotOffset` は手入力しない。**Editor Utility** で Montage をサンプリングし、AnimNotify「Impact」の時刻でラケットのソケット `racket_sweetspot` の位置をルート基準で記録して、自動生成する。

### 6.4 アニメの同期（いちばん重要）

インパクトの瞬間に、**ラケットのスイートスポットとボールが同じ位置・同じ時刻**にあるようにする。

1. **時刻合わせ**：ボールが打点に来る時刻 `T_hit` と現在時刻 `T_now` から
   `PlayRate = ImpactTime / (T_hit - T_now)` を求め、`[0.8, 1.25]` にクランプする。範囲外なら、開始位置（`StartingPosition`）をずらして吸収する
2. **位置合わせ（Motion Warping）**：
   - ワープ目標 = `BallPos(T_hit) - Rotate(SweetSpotOffset)`
   - Montage の構え〜インパクト区間に `MotionWarping` の NotifyState を置き、Root Motion Modifier に **Skew Warp** を使う
   - 向き（Yaw）は打球方向に合わせて最大 ±35° までワープさせる
3. **上半身の微調整**：Control Rig（または Two Bone IK）で、インパクト直前の腕を数 cm 補正する。ワープしきれなかった残差をここで吸収する
4. **インパクト判定**：AnimNotify「Impact」の時点で
   `Dist = |BallPos - SweetSpotWorld|` を計算する
   - `Dist ≤ 0.25 m` なら打球成功。品質に `1 - Dist/0.25` を掛ける
   - それ以上は空振り
   - このとき初めてソルバの結果をボールに適用する（`BallSim->Launch(State)`）

### 6.5 ロコモーション

- **Motion Matching**（PoseSearch プラグイン、5.4 以降）が特に向いている。サイドステップ・クロスステップ・後退・急停止を、データベースに入れるだけで自然につながる
- ストローク中は Montage が上書きする。ストローク後のリカバリーは「センターへ戻る」移動入力で Motion Matching に戻す
- ラケットは `hand_r` のソケットに固定する。両手バックハンドは左手を IK でグリップに吸着させる

---

## 7. ルールと試合進行

### 7.1 ラリーの状態遷移

```
PreServe ─(トス)→ Serving ─(打球)→ InPlay_ServeFlight
   │                                   ├─ サービスボックス外 or ネット → Fault
   │                                   ├─ ネット接触後ボックス内 → Let（打ち直し）
   │                                   └─ ボックス内 → InPlay
InPlay ─(相手コートでバウンド)→ InPlay（バウンド 1 回目）
       ─(2 回目のバウンド)→ PointEnd（最後に打った側の得点）
       ─(ノーバウンドでアウト)→ PointEnd（打った側の失点）
       ─(自コートに直接落下 / ネット)→ PointEnd
Fault ×2 → PointEnd（ダブルフォルト）
PointEnd → スコア更新 → ChangeEnds? → PreServe
```

ボールのイベント（`OnBounce(Pos, Side)`、`OnNetTouch`、`OnHit(PlayerId)`）を購読し、**ルール判定はサーバー（GameMode）だけ**で行う。

### 7.2 スコア（純粋データ＋関数で書き、単体テストする）

```cpp
USTRUCT(BlueprintType)
struct FTennisScore
{
    GENERATED_BODY()
    UPROPERTY() int32 Points[2] = {0,0};   // ゲーム内のポイント（0,1,2,3,...）
    UPROPERTY() int32 Games[2]  = {0,0};
    UPROPERTY() TArray<FIntPoint> SetHistory;
    UPROPERTY() int32 Sets[2]   = {0,0};
    UPROPERTY() bool  bTieBreak = false;
    UPROPERTY() int32 Server    = 0;
    UPROPERTY() int32 TieBreakFirstServer = 0;
};

struct FMatchRules
{
    int32 SetsToWin   = 2;     // 3 セットマッチ
    int32 GamesPerSet = 6;
    bool  bNoAd       = false; // ノーアドバンテージ
    int32 TieBreakAt  = 6;     // 6-6 でタイブレーク
    int32 TieBreakTo  = 7;
    bool  bFinalSetSuperTB = false; // 最終セットを 10 ポイントのタイブレークにする
};
```

| ルール | 実装メモ |
|---|---|
| ゲーム | 4 ポイント以上かつ 2 ポイント差（ノーアドは 3-3 で次のポイントを取った方） |
| 表示 | 0/15/30/40/Deuce/Ad。`Points` から表示を作る |
| セット | 6 ゲーム以上かつ 2 ゲーム差。6-6 でタイブレーク |
| タイブレーク | 7 ポイント以上かつ 2 ポイント差。サーブは最初 1 本、以降 2 本ずつ交代 |
| サーブ交代 | ゲームごと |
| エンドチェンジ | 奇数ゲーム終了時。タイブレーク中は 6 ポイントごと |
| サーブ位置 | ゲーム内の合計ポイントが偶数ならデュースサイド、奇数ならアドサイド |

### 7.3 サーブの仕組み

1. PreServe：ベースラインの後ろで、左右の位置だけを動かせる
2. ボタン押下でトスする（トスのボールも同じ物理で上げる。頂点は 2.8〜3.0 m）
3. 押している間は溜め。**打点の高さ**（トスの頂点に近いほど良い）と溜め量で速さと品質が決まる
   - 速さ：`lerp(35, 55, Charge) m/s`（第 2 サーブは上限 45、スピン多め）
   - 打点が頂点から外れるほど `σ` が増える
4. 狙いはスティックでサービスボックス内のマーカーを動かす（ワイド／ボディ／センター）

---

## 8. AI

### 8.1 構成（StateTree）

```
Root
├─ Serve           : 球種と狙いを確率で選択 → 溜め時間を決めて実行
├─ Return/Rally
│   ├─ Recover     : 相手の打球可能範囲の角の二等分線上に戻る
│   ├─ Intercept   : 予測軌道から打点を選び、移動する（反応遅延あり）
│   └─ Strike      : 球種と狙いを決めてストロークを予約する
└─ PointEnd        : リアクションのアニメ（喜ぶ／悔しがる）
```

### 8.2 判断の流れ

1. **反応遅延**：相手のインパクトから `ReactionTime` が経つまで、予測軌道を「見ない」
2. **予測の不完全さ**：AI が使う予測着地点に `PredictionNoise`（m）を加える。時間が経つほど小さくなる
3. **ポジショニング**：自分のショット後、相手が打つ地点から、自コートの両サイドライン端を結ぶ角の二等分線上の、ベースラインから 0.5〜1 m 後ろへ戻る
4. **ショット選択（ユーティリティ）**：

| 候補 | 加点要素 | 減点要素 |
|---|---|---|
| クロスに深く | 安全（ネットの低い部分を通り、コートが長い） | — |
| ストレート | 相手がクロス寄りにいる | ネットが高い、ミスのリスク |
| 角度のあるショート | 相手がベースラインの後方 | 打点が低いとミス |
| ロブ | 相手がネット前 | 自分の体勢が悪いと浅くなる |
| ドロップ | 相手が深い位置、自分がコート内 | 球威の強い球には使えない |
| アプローチして前へ | 浅い球が来た | — |

スコアに `RiskAppetite` を掛け、最大値（または softmax で確率的に）を選ぶ。

### 8.3 難易度パラメータ（DataAsset）

| パラメータ | Easy | Normal | Hard | Pro |
|---|---|---|---|---|
| ReactionTime（s） | 0.32 | 0.24 | 0.17 | 0.12 |
| 最高速度の倍率 | 0.85 | 0.95 | 1.0 | 1.05 |
| PredictionNoise 初期値（m） | 1.2 | 0.8 | 0.4 | 0.2 |
| タイミング誤差 σ（s） | 0.08 | 0.05 | 0.03 | 0.02 |
| 狙いのライン内マージン（m） | 1.5 | 1.0 | 0.6 | 0.4 |
| RiskAppetite | 0.3 | 0.5 | 0.7 | 0.8 |
| 溜めの平均 | 0.4 | 0.6 | 0.75 | 0.85 |

AI も**プレイヤーと同じエラーモデルとソルバ**を通して打つ。AI だけ正確な初速を直接作るような「ずる」をしないことで、ミスが自然に見える。

**ラバーバンド**（任意）：点差に応じて ReactionTime を ±10% 調整する。

---

## 9. オンライン対戦（サーバー権威）

### 9.1 基本方針：打球イベントだけを同期する

ボールの位置を毎フレーム複製しない。軌道は「打った瞬間の状態」から決定論的に再現できるので、**打球イベント**だけを送る。

```cpp
USTRUCT()
struct FBallLaunchEvent
{
    GENERATED_BODY()
    UPROPERTY() int32  Seq;          // 打球ごとに単調増加
    UPROPERTY() double ServerTime;   // 打った時刻（サーバー時計）
    UPROPERTY() FVector_NetQuantize100 Pos;   // 量子化で帯域を削減（m 単位の値をそのまま格納）
    UPROPERTY() FVector_NetQuantize100 Vel;
    UPROPERTY() FVector_NetQuantize10  Spin;
    UPROPERTY() uint8  HitterId;
};
```

- GameState が `NetMulticast, Reliable` で `MulticastLaunch(FBallLaunchEvent)` を送る
- クライアントは `GetServerWorldTimeSeconds() - ServerTime` だけ早送りして再生する
- **バウンドイベント**（`Seq`、接地位置、時刻）もサーバーから送る。クライアントは誤差が 5 cm を超えたら 0.1 秒かけて補正する。浮動小数点の演算結果はプラットフォーム間で完全には一致しないので、その保険になる

### 9.2 打つ側の遅延補償

1. クライアントはストロークのアニメと打球を**ローカルで先行実行**する（予測）
2. `ServerRequestHit(Seq, ClientViewTime, ShotType, Charge, Aim, ReleaseTiming)` を送る
3. サーバーは `ClientViewTime` 時点のボール位置を軌道から求める（巻き戻しは関数を 1 回呼ぶだけ）。そのうえでインパクト判定を行う。巻き戻しの上限は 150 ms
4. エラーの乱数シードは `Seq` から作るので、クライアントの予測と同じ打球になる
5. サーバーの結果が予測と違う場合（判定 NG など）は、サーバーのイベントで上書きする

### 9.3 受ける側の見え方

受ける側の画面では、ボールは常に「推定サーバー時刻」の位置に描かれる。相手の打球イベントは RTT/2 遅れて届くので、**届いた瞬間にボールが RTT/2 分進んだ位置へ飛ぶ**。対策：

- 相手のスイングは、相手の入力（Hit リクエスト）がサーバーに届いた時点で `Multicast` し、受ける側でアニメを先に再生する
- 打球直後の 0.1 秒間だけ、表示位置を実位置へ指数的に寄せる（見た目のワープを隠す）

### 9.4 そのほか

- キャラクターの移動は標準の CMC 複製（クライアント予測）で十分
- スコアは `FTennisScore` を `ReplicatedUsing=OnRep_Score` で複製する
- 帯域は 1 打あたり 100 バイト未満で、1v1 なら問題にならない

---

## 10. カメラ・演出・サウンド

### 10.1 カメラ

- **中継カメラ**：自分のベースラインの後方 8〜10 m、高さ 6〜8 m、FOV 50〜60。左右はボールとプレイヤーの中点を、強めのダンピングで追う
- エンドチェンジで反転する（ローカルプレイヤーは常に手前）
- リプレイ：ボールの軌道を記録し、ポイント終了後にスローで再生する。ラインの際どい判定では「ボールマーク＋拡大」を表示する（ホークアイ風）

### 10.2 エフェクト（Niagara）

- 軌跡：リボンエミッタ。速度に応じて透明度と長さを変え、30 m/s 以上で強調する
- インパクト：品質が Perfect のときだけフラッシュと粒子を出す
- バウンド：ボールを 0.05 秒だけ Z 方向に潰す（スケール 1.0→0.75→1.0）。クレーでは土煙を出し、デカールでボールマークを 10 秒残す

### 10.3 サウンド（MetaSounds）

| 音 | パラメータ |
|---|---|
| 打球音 | 球速（ピッチと音量）、品質（Perfect でアタックを強く）、球種（スライスはこもった音） |
| バウンド | コート面ごとのサンプル＋速度 |
| ネット | ネット本体／白帯の 2 種 |
| シューズの鳴き | 急減速（加速度のしきい値）でトリガー |
| 観客 | ラリーの長さで期待感のレイヤーを上げ、ポイント終了で拍手 |

---

## 11. データ駆動（DataAsset 一覧）

```cpp
UCLASS(BlueprintType)
class UShotTypeData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) ETennisShotType Type;
    UPROPERTY(EditAnywhere) float MinSpeed = 18.f, MaxSpeed = 38.f;   // m/s
    UPROPERTY(EditAnywhere) float TopRPM = 2500.f, SideRPM = 0.f;
    UPROPERTY(EditAnywhere) ESolverConstraint Constraint = ESolverConstraint::NetClearance;
    UPROPERTY(EditAnywhere) float ConstraintValue = 1.0f;            // m
    UPROPERTY(EditAnywhere) float BaseSigma = 0.6f;                  // m
    UPROPERTY(EditAnywhere) float PaceReuseFactor = 0.2f;
    UPROPERTY(EditAnywhere) float InitialSpeedGuess = 25.f;
    UPROPERTY(EditAnywhere) float InitialElevationDeg = 8.f;
};

UCLASS(BlueprintType)
class UCourtSurfaceData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) float Restitution = 0.75f;
    UPROPERTY(EditAnywhere) float Friction = 0.6f;
    UPROPERTY(EditAnywhere) TObjectPtr<UMetaSoundSource> BounceSound;
    UPROPERTY(EditAnywhere) TObjectPtr<UNiagaraSystem> BounceFX;
};
```

ほかに `UAIDifficultyData`、`DT_Strokes`（ストロークのテーブル）、`UMatchRulesData`。

---

## 12. デバッグと検証

### 12.1 可視化（コンソール変数）

| CVar | 内容 |
|---|---|
| `tennis.Debug.Trajectory 1` | 予測軌道を線で描き、バウンド点に球を描く |
| `tennis.Debug.Solver 1` | ソルバの試行軌道を薄く描く。収束回数を画面に出す |
| `tennis.Debug.Reach 1` | 打点候補と到達可能性を色分けする |
| `tennis.Debug.Impact 1` | インパクト時のスイートスポットとボールの距離 |
| `tennis.TimeScale 0.2` | スロー再生（固定ステップは維持） |

ラリーの記録には **Visual Logger** を使う。打球イベント・AI の判断・スコアの変化を、時系列で後から確認できる。

### 12.2 自動テスト（Automation Spec）

| テスト | 合格条件 |
|---|---|
| ドロップテスト（2.54 m から） | 跳ね返り 1.35〜1.47 m（ハード） |
| 抗力なし・スピンなし | 解析解の放物線と 1 mm 以内で一致 |
| トップスピンの向き | マグヌス力の Z 成分が負 |
| スピンを掛けたバウンド | トップスピンはバウンド後に水平速度が増すか、減りが小さい。バックスピンは減る |
| ソルバの収束 | ランダムな打点・目標 1000 件で 95% 以上が誤差 5 cm 以内 |
| ネット | 高さ 0.5 m で X 方向に打つ → 自コートに落ちる |
| スコア | デュース、タイブレーク、ノーアド、エンドチェンジ、サーブ交代の全パターン |
| 決定論 | 同じイベントから 2 回再生した軌道が完全に一致する |

---

## 13. 開発ロードマップ

| 段階 | 内容 | 完了条件 |
|---|---|---|
| M0 | コート・ネット・ボールの仮アセット（BlenderMCP＋スクリプト、§14） | UE で寸法どおりに表示される |
| M1 | コート＋ボール物理＋デバッグ描画 | 自動テストが全部通る。手動で投げて自然に見える |
| M2 | ショットソルバ＋クリックした地点に打つツール | 狙った点に 5 cm 以内で落ちる。球種で軌道が違って見える |
| M3 | キャラ移動＋ストロークの Montage 3 種（FH・BH・サーブ）＋同期 | 打点に向かって振ると、ラケットにボールが当たって見える |
| M4 | 入力・タイミング・エラーモデル | 気持ちよく打てる（ここで一番時間をかけて調整する） |
| M5 | ルール・スコア・試合の流れ | 1 セットを最後までプレイできる |
| M6 | AI（3 難易度） | Easy は初心者が勝てる。Hard は上級者がギリギリ勝てる |
| M7 | 演出（カメラ・Niagara・MetaSounds・リプレイ） | |
| M8 | オンライン対戦 | RTT 150 ms で違和感なく打ち合える |

**優先順位の考え方**：M1〜M4 で「打って気持ちいい」状態を作るのが最重要。ここが固まる前にルールや AI を作り込むと、後の調整がすべてやり直しになる。

---

## 14. アセット制作パイプライン（Blender ＋ BlenderMCP）

### 14.1 方針

BlenderMCP（ahujasid/blender-mcp）は、Blender のアドオンが開くソケットと MCP サーバーをつなぎ、AI から Blender を操作する仕組み。オブジェクトやマテリアルの操作、シーン情報の取得、ビューポートのスクリーンショット、Blender 内での Python 実行ができる。Poly Haven・Sketchfab・Hyper3D Rodin・Hunyuan3D との連携もある。

このプロジェクトでは次のルールで使う。

1. **寸法が決まっているもの（コート・ネット・ボール・ラケット）は、会話で形を作らせない**。`Tools/Blender/` に置いた bpy スクリプトを MCP の `execute_blender_code` で実行させる。スクリプトは Git 管理し、何度でも同じものが作れるようにする
2. MCP の役割は「スクリプトの実行」「スクリーンショットでの目視確認」「寸法検証」「一括エクスポート」。微調整はスクリプトの引数を変えて再実行する
3. **キャラクターのスキニングとアニメーションは MCP に任せない**（品質が出ない）。§14.6 の流れで作る
4. AI による 3D 生成（Hyper3D・Hunyuan3D）は、**背景の小物だけ**に使う（ベンチ、審判台、植栽、看板など）。必ずリトポか Decimate で軽くし、ライセンスを確認する
5. `execute_blender_code` は任意のコードを実行できる。実行前に .blend を保存し、MCP の作業は作業用ファイルで行う

### 14.2 アセット一覧と仕様

| アセット | 作り方 | ポリゴン目安 | UE 側 | 備考 |
|---|---|---|---|---|
| コート面＋ライン | bpy で生成 | 数百 | Static Mesh | ライン幅・位置は ITF 寸法（§2.2）。ラインは別マテリアル |
| ネット＋ポスト＋シングルススティック | bpy で生成 | 2〜4k | Static Mesh（Nanite なし） | ネットは平面＋網目テクスチャの Masked マテリアル。揺れは WPO（§3.7） |
| ボール | bpy で生成 | 500〜1k | Static Mesh | 半径 3.35 cm、原点は中心 |
| ラケット | bpy で土台を作り手で仕上げる | 3〜6k | Static Mesh＋ソケット | ストリングは Masked テクスチャ |
| スタジアム・観客席 | 手作業＋生成小物 | 制限なし | Nanite | 観客はカード（ビルボード）か Instanced Static Mesh |
| 選手 | 手作業でモデリング → Mannequin 互換リグ | 30〜60k | Skeletal Mesh | §14.6 |

### 14.3 Blender 側の共通設定

| 項目 | 設定 |
|---|---|
| 単位 | Metric、Unit Scale = 1.0（1 BU = 1 m） |
| 向き | **UE の前方（+X）に合わせて Blender でも +X を前**にモデリングする。軸の設定は最初に矢印メッシュでテストエクスポートして確認し、プロジェクトで 1 つに固定する |
| 原点 | ボール＝中心、ラケット＝グリップエンド、コート＝ネット中央の地面（§2.1 と一致させる） |
| 命名 | `SM_`（Static）、`SK_`（Skeletal）、`UCX_SM_xxx_00`（凸包コリジョン）、`SOCKET_xxx`（ソケット用 Empty） |
| トランスフォーム | エクスポート前に Apply（Rotation & Scale） |
| UV | ch0 = テクスチャ用。Lumen を使うなら Lightmap UV は不要 |
| マテリアル | Blender ではスロットと名前だけ付ける（`M_Court_Hard` など）。実際のシェーダーは UE で作る |

### 14.4 bpy スクリプト：コートの生成

```python
# Tools/Blender/build_court.py
import bpy

HALF_LEN   = 11.885
HALF_W_S   = 4.115
HALF_W_D   = 5.485
SERVICE_X  = 6.40
LINE_W     = 0.05
BASE_W     = 0.10
LINE_H     = 0.001          # 1 mm 浮かせて Z ファイティングを防ぐ
RUNOFF_X, RUNOFF_Y = 6.40, 3.66   # コート外の余白（大会規格の目安）

def get_mat(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)

def box(name, cx, cy, sx, sy, z=0.0, sz=0.002, mat=None, coll=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, z + sz / 2))
    o = bpy.context.active_object
    o.name = name
    o.scale = (sx, sy, sz)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if mat: o.data.materials.append(get_mat(mat))
    if coll:
        for c in o.users_collection: c.objects.unlink(o)
        coll.objects.link(o)
    return o

def build():
    coll = bpy.data.collections.get("COURT") or bpy.data.collections.new("COURT")
    if coll.name not in bpy.context.scene.collection.children:
        bpy.context.scene.collection.children.link(coll)
    for o in list(coll.objects): bpy.data.objects.remove(o, do_unlink=True)

    # 面（コート＋ランオフ）
    box("SM_Court_Surface", 0, 0, 2*(HALF_LEN+RUNOFF_X), 2*(HALF_W_D+RUNOFF_Y),
        z=-0.002, mat="M_Court_Out", coll=coll)
    box("SM_Court_Inner", 0, 0, 2*HALF_LEN, 2*HALF_W_D, z=-0.001, sz=0.001,
        mat="M_Court_In", coll=coll)

    L = dict(z=LINE_H, sz=0.001, mat="M_Court_Line", coll=coll)
    # ベースライン（外側の縁が HALF_LEN）
    for s in (-1, 1):
        box(f"Line_Base_{s}", s*(HALF_LEN-BASE_W/2), 0, BASE_W, 2*HALF_W_D, **L)
    # サイドライン（シングルス／ダブルス）
    for s in (-1, 1):
        box(f"Line_SideS_{s}", 0, s*(HALF_W_S-LINE_W/2), 2*HALF_LEN, LINE_W, **L)
        box(f"Line_SideD_{s}", 0, s*(HALF_W_D-LINE_W/2), 2*HALF_LEN, LINE_W, **L)
    # サービスライン（シングルス幅の間）
    for s in (-1, 1):
        box(f"Line_Service_{s}", s*(SERVICE_X-LINE_W/2), 0, LINE_W, 2*HALF_W_S, **L)
    # センターサービスライン
    box("Line_CenterService", 0, 0, 2*SERVICE_X, LINE_W, **L)
    # センターマーク（長さ 10 cm、コート内側へ）
    for s in (-1, 1):
        box(f"Line_CenterMark_{s}", s*(HALF_LEN-BASE_W-0.05), 0, 0.10, LINE_W, **L)

    # ラインを 1 メッシュに統合
    bpy.ops.object.select_all(action='DESELECT')
    lines = [o for o in coll.objects if o.name.startswith("Line_")]
    for o in lines: o.select_set(True)
    bpy.context.view_layer.objects.active = lines[0]
    bpy.ops.object.join()
    bpy.context.active_object.name = "SM_Court_Lines"

build()
```

MCP で実行させるときの指示例：

> `Tools/Blender/build_court.py` の内容を execute_blender_code で実行して、真上と斜めのビューポートのスクリーンショットを撮って。そのあと `validate_dimensions.py` を実行して結果を表示して。

### 14.5 そのほかのスクリプト

**ネット（`build_net.py`）**
- ポストはダブルスのサイドラインの 0.914 m 外側（y = ±6.399）、高さ 1.07 m。シングルススティックは y = ±5.029
- ネット本体は X=0 の平面。横 64 × 縦 12 分割程度にし、上辺の頂点を区分線形の高さ（中央 0.914 m）に合わせる
- **頂点カラー R** に揺れの重みを入れる：ポスト際と下辺で 0、上辺の中央で 1（`(1-|y|/y_post) * (z/h)`）。UE の WPO マテリアルはこの値で揺れ幅を掛ける
- 白帯（ネットバンド）は別メッシュにし、別マテリアルを割り当てる

**ボール（`build_ball.py`）**
- UV Sphere（32 × 16）、半径 0.0335 m。フェルトの縫い目はテクスチャで表現する
- LOD1（16 × 8）も同じスクリプトで生成し、`SM_Ball_LOD1` としてエクスポートする

**ラケット（`build_racket.py`）**
- 全長 0.686 m（ITF の上限は 0.737 m）、フレーム幅 0.26 m 前後（上限は 0.317 m）
- フェイスは楕円カーブ → Bevel で厚み 2.2 cm 程度のフレームにする。スロート・グリップは手作業で仕上げてよい
- Empty を 2 つ置き、ラケットメッシュの子にする
  - `SOCKET_racket_sweetspot`：フェイスの中心より少し下（フェイス中心からグリップ方向に 3〜4 cm）
  - `SOCKET_grip`：手で握る位置（手首側からの取り付け用）
- FBX で取り込むと UE のソケットになる。§6.3・§6.4 の `racket_sweetspot` はこのソケットを使う

**寸法検証（`validate_dimensions.py`）**

```python
import bpy
EXPECT = {
    "SM_Court_Lines": (23.77, 10.97),     # X, Y の外形
    "SM_Ball":        (0.067, 0.067),
    "SM_Racket":      (None, None),       # 全長だけ別にチェック
}
TOL = 0.005
for name, (ex, ey) in EXPECT.items():
    o = bpy.data.objects.get(name)
    if not o: print(f"[NG] {name}: 見つからない"); continue
    dx, dy, dz = o.dimensions
    ok = (ex is None or abs(dx-ex) < TOL) and (ey is None or abs(dy-ey) < TOL)
    print(f"[{'OK' if ok else 'NG'}] {name}: {dx:.3f} x {dy:.3f} x {dz:.3f}")
r = bpy.data.objects.get("SM_Racket")
if r: print(f"[{'OK' if max(r.dimensions) <= 0.737 else 'NG'}] Racket length {max(r.dimensions):.3f} m")
```

**一括エクスポート（`export_all.py`）**

```python
import bpy, os
OUT = bpy.path.abspath("//../Export/")
os.makedirs(OUT, exist_ok=True)

def export(objs, filename):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
        for c in o.children: c.select_set(True)   # SOCKET_ / UCX_ も含める
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(OUT, filename),
        use_selection=True,
        apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_UNITS',   # 1 m → UE で 100 cm
        object_types={'MESH', 'EMPTY'},
        mesh_smooth_type='FACE',
        use_mesh_modifiers=True,
        add_leaf_bones=False,
        # 軸の設定は §14.3 のテストで決めた値に固定する
    )

for name in ["SM_Court_Surface", "SM_Court_Inner", "SM_Court_Lines",
             "SM_Net", "SM_Ball", "SM_Racket"]:
    o = bpy.data.objects.get(name)
    if o: export([o], f"{name}.fbx")
```

### 14.6 選手キャラクター（Skeletal Mesh）

テニスはアニメの量が多い（ストローク × 高さ × 到達 × 球種）。**骨格は UE5 Mannequin（Manny / Quinn）互換にそろえる**のがいちばん効率的。

1. UE から Manny の Skeletal Mesh を FBX でエクスポートし、Blender に読み込む
2. 自作の選手モデルを Manny の骨格に合わせてスキニングする（ウェイト調整は手作業。MCP は確認用のスクリーンショットや、ボーン名のチェックに使う）
3. ラケット用のボーンとして `hand_r` の子に `racket_attach` を追加する。追加しなくても、UE 側で `hand_r` にソケットを作れば足りる
4. アニメーションは、モーションキャプチャ（Rokoko、Move.ai など）またはマーケットプレイスのテニスモーションを使う。IK Retargeter で Manny 骨格へリターゲットする
5. Blender でストロークを手付けする場合は、インパクトのフレーム番号を JSON で書き出す。UE の Editor Utility（Python）がその JSON を読み、AnimNotify「Impact」を自動で打つ。§6.3 の `ImpactTime` と `SweetSpotOffset` の自動生成と同じツールにまとめる

```json
// Export/stroke_impacts.json（Blender 側スクリプトが出力）
{ "AM_FH_Mid_Drive": { "fps": 30, "impact_frame": 18 },
  "AM_BH_Mid_Slice": { "fps": 30, "impact_frame": 21 } }
```

### 14.7 UE への取り込み

| 項目 | 設定 |
|---|---|
| フォルダ | `Content/Tennis/Art/{Court,Net,Ball,Racket,Characters,Stadium}` |
| Static Mesh | Combine Meshes をオフ、Auto Generate Collision をオフ（UCX_ を使う） |
| コート面 | コリジョンは見た目用（ボールは自前物理なので不要）。キャラクターの接地用に単純な Box を 1 枚置く |
| ネット | Nanite オフ、Two Sided の Masked マテリアル、WPO は頂点カラー R × 衝突パラメータ |
| ボール | Nanite オフ。マテリアルは Default Lit＋フェルト用のノーマル |
| スタジアム | Nanite オン |
| 再取り込み | Blender から同じファイル名で出力 → UE の Reimport。可能なら Interchange の自動再取り込みを使う |

### 14.8 作業の流れ

```
build_*.py（Git 管理）
   │  BlenderMCP: execute_blender_code
   ▼
Blender シーン ──(MCP: スクリーンショット・validate_dimensions)──▶ 目視・寸法 OK?
   │ OK                                                     │ NG → スクリプトの引数を修正
   ▼
export_all.py ─▶ Export/*.fbx ─▶ UE Reimport ─▶ テストマップで確認
```

ロードマップ（§13）への組み込み：**M1 の前に「M0：コート・ネット・ボールの仮アセット」**を入れる（スクリプト生成なので半日程度）。ラケットと選手は M3 までに用意する。

---

## 付録 A：主要な数値のまとめ

| 項目 | 値 |
|---|---|
| ボール質量・半径 | 0.0577 kg・0.0335 m |
| 抗力係数 k_d | 約 0.0203 /m（C_d = 0.55） |
| 揚力係数 C_L | 1 / (2 + v/(rω)) |
| 慣性モーメント | 0.55·m·r² |
| シミュレーション周期 | 240 Hz・RK4 |
| ストロークの球速 | 20〜38 m/s（約 70〜140 km/h） |
| サーブ | 35〜55 m/s（約 125〜200 km/h） |
| トップスピン | 1500〜3000 rpm |
| スライス | 1000〜2000 rpm（バックスピン） |
| インパクト判定の距離 | 0.25 m 以内 |
| アニメの再生速度補正 | 0.8〜1.25 倍 |
| ネット | 中央 0.914 m・ポスト 1.07 m（区分線形） |

## 付録 B：ハマりどころ

1. **単位の混在**：UE は cm、物理は m。変換はコート座標の変換関数 1 か所だけで行う
2. **ボールの Tick 順序**：キャラより後に更新すると、インパクト判定が 1 フレームずれる。`TG_PrePhysics` にするか、`AddTickPrerequisiteComponent` で順序を固定する
3. **Montage のルートモーションと CMC**：ストローク中は `RootMotionMode = RootMotionFromMontagesOnly`。ワープ後に CMC の速度が残って滑らないよう、終了時に `Velocity` をリセットする
4. **予測とソルバで物理が食い違う**：試行シミュレーションに簡易版の物理を使うと、狙いがずれる。必ず同じ関数を呼ぶ
5. **難しさの原因が見えない**：ミスの原因（タイミング／体勢／球威）を UI で一言だけ出すと、プレイヤーが上達しやすい
