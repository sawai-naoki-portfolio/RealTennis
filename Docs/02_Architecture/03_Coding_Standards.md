# コーディング規約

| 項目 | 内容 |
|---|---|
| 文書 ID | ARCH-03 |
| 版 | 1.0（2026-11-04） |
| 前提 | Epic の C++ コーディング規約（UE 5.8）に従う。ここには、このプロジェクトで追加・変更する規則だけを書く |
| 関連 | [モジュール設計](02_Module_Package_Design.md)、[非機能要件](../01_Specification/03_NonFunctional_Requirements.md) |

---

## 1. 単位と座標

| 規則 | 内容 |
|---|---|
| U-1 | **物理（`TennisSim`）は SI 単位**：m、s、kg、rad/s。変数名に単位を付けない代わりに、構造体のコメントに単位を書く |
| U-2 | UE のワールドとの変換は `ATennisCourt::LocalMetersToWorld()`／`WorldToLocalMeters()` だけで行う。ほかの場所で `* 100` や `/ 100` を書かない |
| U-3 | 回転は rad/s で持つ。rpm は表示とデータの入力にだけ使い、変換は `TennisUnits::RpmToRadPerSec()`（`TennisSim/Public/TennisUnits.h`）を使う |
| U-4 | 表示用の速さは km/h と mph（設定による）。変換と書式は 1 か所にまとめる（`TennisUnits` に、速さを表示する HUD の作業 W-4.4 で足す） |
| U-5 | コート局所座標：原点はネット中央の地面、+X はプレイヤー 0 の側から見て相手の側、+Z は上。プレイヤー 1 の視点の計算は `Side` を掛けて反転する（`Side = +1 / −1`） |
| U-6 | 角度は rad で計算し、データ（DataAsset）の入力は度で行う（`UPROPERTY` に `Units="deg"` を付ける） |

## 2. 決定論

| 規則 | 内容 |
|---|---|
| D-1 | ボール物理・ソルバ・誤差・ルールは **double** で計算する（`FVector` は UE5 で double。`FVector3f` を使わない） |
| D-2 | 乱数は `FRandomStream` だけ。シードは `HashCombine(MatchSeed, RallySeq, ShotIndex)` で作る。`FMath::Rand()`、`FMath::FRand()`、`std::rand` を **TennisSim と Tennis のゲームロジックで使わない**（演出は可） |
| D-3 | ゲームの結果に関わる計算で、フレームの `DeltaTime` を使わない。ボールは固定ステップ（`TennisConst::SimDt = 1/240`。`TennisConstants.h`）、スタミナなどもステップ数で数える |
| D-4 | `TMap`／`TSet` の反復順に依存する処理を書かない（結果に影響する場合は、キーで並べ替えてから処理する） |
| D-5 | 浮動小数点の比較は、許容誤差つきの関数（`FMath::IsNearlyEqual` など）を使う。ただし**分岐の境界**（ネットの高さ、ラインの判定）では、許容誤差を入れずに常に同じ式で比較する（予測と本番で結果を一致させるため） |
| D-6 | `TennisSim` の関数は、グローバルな状態を持たない（`static` の可変変数を使わない）。必要な情報は引数（`FBallSimContext` など）で渡す |
| D-7 | 予測・ソルバ・本番・ネットワークの再生・リプレイは、同じ `TennisBallPhysics::Step()` を呼ぶ。**簡略版の物理を作らない**（例外は AI のショット表の作成だけで、それも `Step()` で作る） |

## 3. 構造

| 規則 | 内容 |
|---|---|
| S-1 | `TennisSim` は `Engine` モジュールに依存しない。`UObject` を継承しない（`USTRUCT` は可） |
| S-2 | Actor と Component は薄く保つ。ロジックは `TennisSim` の関数か、テストできる小さなクラスに置く |
| S-3 | 演出（音・エフェクト・カメラ・UI・観客）は `UTennisEventSubsystem` のイベントを購読する。演出からゲームの状態を変えない |
| S-4 | AI とプレイヤーで打球の経路を分けない（`FShotIntent` に統一する） |
| S-5 | 調整の値を直書きしない。DataAsset・DataTable・`UTennisSimSettings` に置く。物理定数（g など）と、ITF の規格値（コートの寸法など）は `TennisSim/Public/TennisConstants.h` に名前付きで置く |
| S-6 | ブループリントから呼ぶ関数は `BlueprintCallable`／`BlueprintPure` にし、カテゴリを `Tennis|<分類>` にする |
| S-7 | ネットワークの RPC は、名前に `Server`／`Client`／`Multicast` を付ける。引数は検証する（`WithValidation`） |

## 4. 命名

[モジュール設計 §8](02_Module_Package_Design.md) に加えて、次の規則を使う。

| 対象 | 規則 | 例 |
|---|---|---|
| ルールの判定関数 | 条番号をコメントに書く | `// ITF Rule 24(i): ball touches player` |
| イベントの構造体 | `F<主語><出来事>Event`。同じ主語の出来事で中身がほぼ同じなら、種類（`Type`）を持つ 1 つの `F<主語>Event` にしてよい | `FPointEndedEvent`、`FBallEvent`（`EBallEventType`：Bounce・NetCross・NetTouch・PostHit） |
| デリゲート | `FOn<出来事>` | `FOnBallBounce` |
| ブール | `b` で始める | `bNetTouchedThisFlight` |
| 時刻 | `<名前>Time`（秒、シミュレーション時刻） | `ImpactTime`、`LaunchTime` |
| 回数 | `Num<名前>` | `NumBounces` |

## 5. ログと可視化

### 5.1 ログのカテゴリ

| カテゴリ | 内容 | 既定の詳細度 |
|---|---|---|
| `LogTennisBall` | 打ち出し・バウンド・ネット | Log |
| `LogTennisShot` | ソルバの結果・品質・誤差 | Log |
| `LogTennisStroke` | 打点・ストロークの選択・インパクト | Log |
| `LogTennisMove` | 選手の移動（コートが見つからないなど）・範囲 | Log |
| `LogTennisInput` | 操作（球種の予約・離す・取り消し、Context の切り替え、操作のアセットが見つからない） | Log |
| `LogTennisCamera` | カメラ（モードの始まり・目標にそろえた、追う選手・コートがない） | Log |
| `LogTennisRules` | 判定（条番号付き）・スコア | Log |
| `LogTennisMatch` | 試合の段階の遷移 | Log |
| `LogTennisAI` | AI の判断 | Warning（`tennis.Debug.AI 1` で Verbose） |
| `LogTennisNet` | 打球イベントの送受信・補正 | Log |
| `LogTennisPlatform` | アプリの中断・入力機器の切り替え | Log |

### 5.2 CVar

| CVar | 型 | 内容 |
|---|---|---|
| `tennis.Debug.Trajectory` | int | 1：予測軌道とバウンド点、2：本番の軌道との差も描く |
| `tennis.Debug.Solver` | int | ソルバの試行の軌道と、収束の回数 |
| `tennis.Debug.Reach` | int | 打点の候補と到達できるか |
| `tennis.Debug.Impact` | int | インパクトの時のスイートスポットとボールの距離 |
| `tennis.Debug.Rules` | int | 判定の結果と条番号を画面に表示 |
| `tennis.Debug.AI` | int | AI の状態・ショットの候補の点数 |
| `tennis.Debug.Net` | int | ネットワークの補正の量 |
| `tennis.TimeScale` | float | シミュレーションの速さ（固定ステップは維持） |
| `tennis.Sim.ForceSeed` | int | 乱数のシードを固定（再現用） |
| `tennis.Cheat.PerfectTiming` | int | タイミングを常に Perfect（開発ビルドのみ） |

- **デバッグの切り替え（上の表の `tennis.Debug.*`・`tennis.TimeScale` など）は、`UTennisDebugSettings`（Project Settings → Game → Tennis Debug）のプロパティと、それを直接読み書きするコンソールコマンドとして作る**。CVar は使わない（`meta=(ConsoleVariable)` は自動では同期せず、`RegisterConsoleVariableRef` は値のコピーを持つため。ARCH-04 §3.1）。Unreal MCP には CVar を設定するツールがないため、エージェントは `ConfigSettingsToolset` でこのセクションを変えて切り替える（[ARCH-04 §3.1](04_Unreal_MCP_Workflow.md)）
- 新しい切り替えは、`UTennisDebugSettings` のプロパティ・名前の公開定数・コマンドの登録・テストを同じ PR で追加する

### 5.2.1 ログの形式

- 1 行 = 1 つの出来事。`[種類] key=value key=value …` の形にする（例：`[Verdict] point=37 winner=1 reason=BodyHit rule=24i`）
- キーの名前はテストと統計の名前に合わせる。Unreal MCP の `LogsToolset.GetLogEntries(category, pattern)` で検索して確かめられるようにするため（[ARCH-04 §3.2](04_Unreal_MCP_Workflow.md)）

### 5.3 Visual Logger

- 打球（打ち出しの状態・目標・誤差）、バウンド、判定、AI の判断を `UE_VLOG` で記録する
- カテゴリはログと同じ名前にする
- AI 対 AI の自動対戦（FR-DBG-006）では、Visual Logger の記録をファイルに保存し、不具合の再現に使う

## 6. テスト

| 規則 | 内容 |
|---|---|
| T-1 | `TennisSim` の公開関数には、必ず単体テストを付ける（CQTest） |
| T-2 | テスト名は `Tennis.<モジュール>.<分類>.<ケース>`。ルールのテストは、[ルール仕様](../01_Specification/05_Rules_Specification.md) の行の ID を含める（例：`Tennis.Sim.Rules.R09_BodyHitBeforeBounce`） |
| T-3 | 物理のテストの期待値は、[参照スクリプト](../../Tools/Reference/ballsim_reference.py) の値を使う。期待値を変えたら、参照スクリプトと調査報告も更新する |
| T-4 | テストはエンジンのワールドを使わずに書けるものを優先する。ワールドが必要なテストは `Tennis.Game.*` に分け、`L_TestCourt` のマップを使う |
| T-5 | ランダムを使うテストは、シードを固定する |
| T-6 | 不具合を直したら、その不具合を再現するテストを先に追加する |

## 7. コードの例

```cpp
// TennisSim/Private/Ball/TennisBallPhysics.cpp（抜粋：規則 U-1, D-1, D-5, D-6 の例）
void TennisBallPhysics::ResolveGroundBounce(FBallState& S, const FBallParams& Ball,
                                            const FSurfaceParams& Surf, double TempC)
{
    const FVector N(0.0, 0.0, 1.0);
    const double Vn = FVector::DotProduct(S.Vel, N);              // m/s（負なら接近中）
    if (Vn >= 0.0) { return; }

    const double E  = Surf.RestitutionAt(-Vn, TempC);             // e(v_n, T)：RES-02 §3.4
    const double Jn = -(1.0 + E) * Ball.Mass * Vn;                // N·s
    const FVector Rc = -Ball.Radius * N;
    const FVector Vc  = S.Vel + FVector::CrossProduct(S.Spin, Rc);
    const FVector Vct = Vc - FVector::DotProduct(Vc, N) * N;      // 接触点の滑り速度

    const double A = Ball.InertiaAlpha;
    FVector Jt = -Vct * (Ball.Mass * A / (1.0 + A)) * (1.0 + Surf.TangentialRestitution);
    const double JtMax = Surf.Friction * Jn;
    const bool bSliding = Jt.Size() > JtMax;                      // 境界は常に同じ式で比較する（D-5）
    if (bSliding) { Jt = Jt.GetSafeNormal() * JtMax; }

    S.Vel  += (Jn * N + Jt) / Ball.Mass;
    S.Spin += FVector::CrossProduct(Rc, Jt) / (A * Ball.Mass * Ball.Radius * Ball.Radius);
    S.NumBounces++;
}
```

## 8. レビューの観点（PR のチェックリスト）

- [ ] 単位は SI か。cm との変換はコートの関数だけか（U-1、U-2）
- [ ] ゲームの結果に関わる計算に `DeltaTime` や `FMath::Rand` を使っていないか（D-2、D-3）
- [ ] 予測とソルバと本番で、同じ物理の関数を呼んでいるか（D-7）
- [ ] 調整の値がデータに出ているか（S-5）
- [ ] 演出がゲームの状態を変えていないか（S-3）
- [ ] ルールの分岐に条番号のコメントがあるか
- [ ] 単体テストがあるか。期待値の出典はあるか（T-1、T-3）
- [ ] 新しい CVar・ログのカテゴリを、この文書に追記したか
