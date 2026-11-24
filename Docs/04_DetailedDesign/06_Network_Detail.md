# DD-06：ネットワーク 詳細設計

| 項目 | 内容 |
|---|---|
| 文書 ID | DD-06 |
| 版 | 1.0（2026-11-04） |
| 対象のクラス | `ATennisGameMode`、`ATennisGameState`、`ATennisPlayerController`、`UTennisBallSimComponent`、`UTennisStrokeComponent`、`TennisNetTypes.h` |
| 要件 | FR-NET-001〜008、NFR-NET-001〜004、NFR-DET-003・007 |
| 根拠 | 既存設計 §9、[配置図](../02_Architecture/01_System_Architecture.md) §7 |

---

## 1. 構成

| 項目 | 内容 |
|---|---|
| 形 | リッスンサーバー（ホストが権威）。1 対 1 |
| 輸送 | UE の標準のレプリケーション（NetDriver）。EOS の P2P（NAT 越え・中継）を使う |
| ホストの選び方 | PC を優先。両方が PC（または両方がスマートフォン）の場合は、RTT の計測で回線の良い方 |
| 権威 | ルールの判定、打球の確定、スコア、試合の段階はサーバーだけ。クライアントは入力の意図を送る |

## 2. 複製するデータ

| データ | 場所 | 方式 | 頻度 |
|---|---|---|---|
| スコア | `ATennisGameState::Score` | `ReplicatedUsing=OnRep_Score` | ポイントごと |
| 試合の段階・タイマー | `ATennisGameState::Phase`、`PhaseEndServerTime` | Replicated | 変化時 |
| ラリー番号・試合のシード | `RallySeq`、`MatchSeed` | Replicated | ポイントごと |
| 打球イベント | `MulticastLaunch(FBallLaunchEvent)` | NetMulticast・Reliable | 打球ごと |
| バウンドの補正 | `MulticastBounce(FBallBounceNetEvent)` | NetMulticast・Reliable | バウンドごと |
| ストロークの開始 | `MulticastStrokeStarted(FStrokeStartNet)` | NetMulticast・Unreliable | 予約ごと |
| ポイントの結果 | `MulticastPointEnded(FPointResult)` | NetMulticast・Reliable | ポイントごと |
| 選手の移動 | CMC | 標準の複製（クライアント予測） | 毎ティック |
| スタミナ | `UTennisStaminaComponent::Stamina` | Replicated（COND_SkipOwner ではない） | ポイント間 |
| 統計 | `UTennisMatchStatsComponent` | ポイント終了時に Multicast（差分） | ポイントごと |

## 3. 打球イベントと量子化

```cpp
USTRUCT() struct FBallLaunchEvent
{
    GENERATED_BODY()
    UPROPERTY() int32  Seq = 0;
    UPROPERTY() double ServerTime = 0.0;          // 打った時刻（サーバーのシミュレーション時刻）
    UPROPERTY() FVector_NetQuantize100 Pos;       // m 単位の値をそのまま入れる → 0.01 m の精度
    UPROPERTY() FVector_NetQuantize100 Vel;       // 0.01 m/s の精度
    UPROPERTY() FVector_NetQuantize10  Spin;      // 0.1 rad/s の精度
    UPROPERTY() uint8  HitterId = 0;
    UPROPERTY() uint8  ShotType = 0;              // 演出用（音・観客）
    UPROPERTY() uint8  Quality = 0;               // 0〜255（演出用）
};
```

**量子化の誤差の見積もり**（RES-05 指摘 25）：

| 量 | 最大の誤差 | 1.0 s の飛行後の位置の誤差 |
|---|---|---|
| 位置 | 0.005 m | 0.005 m |
| 速度 | 0.005 m/s（各成分） | 約 0.005 m |
| スピン | 0.05 rad/s | マグヌスの変化はごく小さい（< 1 mm） |

→ **サーバーとクライアントの両方が、量子化した後の値から軌道を作る**（サーバーも自分の Launch の値を量子化してから使う）。これで、同じ CPU 系列ならビット単位で同じ軌道になる。量子化は誤差ではなく、共通の入力の丸めになる。

## 4. 打つ側の遅延補償

シーケンスは [SEQ-04](../03_UML/03_Sequence_Diagrams.md)。

### 4.1 クライアント

1. 予約・溜め・離すはローカルで処理し、アニメも先に再生する
2. インパクトで、ローカルの品質・誤差（シード = `MakeSeed(MatchSeed, RallySeq, ShotIndex)`）・ソルバから**予測の打球**を作り、すぐに再生する（Seq は仮番号）
3. `ServerRequestHit` を送る

```cpp
UFUNCTION(Server, Reliable, WithValidation)
void ServerRequestHit(int32 BallSeq, double ClientViewTime, FShotIntentNet Intent, double ReleaseTime, FVector_NetQuantize100 SweetSpot);
```

### 4.2 サーバー

| 段階 | 処理 |
|---|---|
| 検証 | `BallSeq` が現在の打球か。`ClientViewTime` が現在から 150 ms 以内の過去か。狙いが相手のコートの範囲内か。溜めが 0〜1 か。離した時刻が予約の後か |
| 巻き戻し | `GetStateAt(ClientViewTime)` で、その時刻のボールの状態を求める（関数を 1 回呼ぶだけ） |
| 判定 | スイートスポットの位置は、クライアントが送った値と、サーバー側の骨の位置の差が 0.3 m 以内なら、クライアントの値を使う（それ以上は不正として、サーバーの値を使う） |
| 打球 | 同じシードで品質・誤差・ソルバ → `Launch(状態, ClientViewTime)`（過去の時刻なので早送り） |
| 配信 | `MulticastLaunch`（確定した Seq） |

### 4.3 クライアントの照合

- 確定のイベントと予測の打球を比べ、打ち出しの差が 1 cm・0.05 m/s 以内なら、そのまま（確定の値に置き換えるだけ）
- 差が大きい場合は、確定の軌道に置き換え、表示は 0.1 s かけて寄せる
- サーバーが「空振り」と判定した場合は、ボールを確定の状態（打たれていない）に戻す。アニメはそのまま（空振りに見える）

## 5. 受ける側の見え方

| 問題 | 対策 |
|---|---|
| 相手の打球のイベントが RTT/2 遅れて届く | 届いたときに `Now − ServerTime` だけ早送りして再生する。表示は 0.1 s かけて実位置へ寄せる（指数的） |
| 相手のスイングが遅れて見える | 相手の予約（`MulticastStrokeStarted`）が届いた時点でアニメを始める。インパクトのタイミングは、確定のイベントに合わせて再生速度を補正する |
| 自分の反応の時間が RTT/2 減る | RTT/2 の分だけ、相手の打球の着地点マーカー（アシスト）の表示を早める。AI と違って補正はしない（公平性：相手も同じ条件） |

## 6. 時刻の同期

- サーバーのシミュレーション時刻（`ServerSimTime`）を基準にする
- クライアントは 2 秒ごとに `ServerPing(ClientTime)` → `ClientPong(ClientTime, ServerSimTime)` で RTT とずれを測る。直近 8 回のうち RTT の小さい 4 回の中央値でずれを推定する
- ずれの変化は 1 秒あたり 5 ms までに抑えて、滑らかに補正する

## 7. バウンドの補正（クロスプレイを含む）

- サーバーは、バウンドのたびに `MulticastBounce(Seq, Time, Pos, Vel, Spin)` を送る
- クライアントは、自分の軌道の同じバウンドと比べ、**位置の差が 5 cm を超えたら**、サーバーの状態に置き換えて、その後を再計算する。表示は 0.1 s かけて補正する
- x64 と ARM64 の差（浮動小数点の丸めの違い）は、通常は数 mm 以下。白帯に当たるような分岐の近くでだけ大きく変わるので、その場合に補正が働く
- 補正の発生率と量を、`tennis.Debug.Net 1` と統計で記録する（NFR-DET-003・007）

## 8. 移動とストローク

| 項目 | 内容 |
|---|---|
| 移動 | CMC のクライアント予測。自動移動（打点への接近）も入力ベクトルとして送るので、予測が効く |
| ストローク中の移動 | ストロークのモンタージュはその場で、ルートモーションを使わない（DD-03 §6.2）。ストロークの間も、体は移動のモデル（CMC の予測と補正）のまま動く。`ServerStrokeStarted` では、ストローク・打点の時刻・自動移動の目標を渡す |
| ネットの手前で止まる | サーバーとクライアントの両方で同じ規則（FR-MOVE-006） |

## 9. 切断・再接続・中断

| 状況 | 処理 |
|---|---|
| ポイント中に切断 | ポイントはやり直し（Rule 26 の扱い）。60 秒の猶予 |
| 60 秒以内に再接続 | `FMatchSnapshot` を送り、ポイント間から再開する |
| 60 秒を超えた | 残った側の不戦勝 |
| スマートフォンのアプリの中断 | 切断と同じ扱い（FR-MATCH-007） |
| ポーズ | オンラインではない。ポイント間の時間とエンドチェンジの時間を使う |
| ホストの切断 | 試合は終了（ホストの移行はしない）。統計は記録しない |

## 10. マッチメイキング（EOS）

| 項目 | 内容 |
|---|---|
| ログイン | PC：Steam のアカウント（Connect の外部認証）、iOS：Sign in with Apple、Android：Google Play Games |
| ロビーの属性 | ゲームの版（互換性のある版だけ）、入力の種類（Touch／Pad_KB）、地域、レーティング（任意）、試合形式 |
| 優先順位 | 同じ入力の種類 → 地域 → レーティングの差（FR-NET-008） |
| 招待 | フレンド（各プラットフォームのフレンド ＋ EOS のフレンド） |

## 11. 帯域の見積もり

| 内容 | バイト | 頻度 | 平均 |
|---|---|---|---|
| 打球イベント | 約 60 | 1 s に 1 回 | 0.06 KB/s |
| バウンドの補正 | 約 50 | 1 s に 1 回 | 0.05 KB/s |
| 選手の移動（2 人） | 約 40 × 30 Hz | — | 約 2.4 KB/s |
| その他（スコア・段階・統計） | — | ポイントごと | < 0.1 KB/s |
| **合計** | | | **約 3 KB/s**（NFR-NET-002 の 10 KB/s 以内） |

## 12. テスト

| ID | 内容 | 合格条件 |
|---|---|---|
| T-NET-001 | 量子化後の値から、サーバーとクライアントで同じ軌道 | ビット単位で一致（同じ CPU 系列） |
| T-NET-002 | RTT 150 ms（`Net PktLag=75`）で 1 セット | 打球の不一致の割合 < 2% |
| T-NET-003 | パケットロス 5%（`Net PktLoss=5`） | 試合が破綻しない |
| T-NET-004 | 不正な `ServerRequestHit`（範囲外の狙い、200 ms 前の時刻） | 拒否される |
| T-NET-005 | 切断 → 30 秒で再接続 | 同じスコアから再開 |
| T-NET-006 | PC 対スマートフォン（ARM64）で 1 セット | 補正の発生率 < 10%、平均の補正量 < 3 cm |
| T-NET-007 | 帯域 | 平均 10 KB/s 以下 |
