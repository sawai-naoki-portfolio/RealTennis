# DATA-03：セーブデータと設定

| 項目 | 内容 |
|---|---|
| 文書 ID | DATA-03 |
| 版 | 1.0（2026-11-04） |
| 対象のクラス | `UTennisSaveSubsystem`、`UTennisProfileSave`、`UTennisUserSettings`、`FMatchSnapshot` |
| 要件 | FR-SAVE-001〜005、FR-MATCH-006〜007、FR-INP-006、FR-UI-010、NFR-ACC-001〜010 |

---

## 1. 保存するもの

| 種類 | 保存先 | クラス | 同期 |
|---|---|---|---|
| 設定（映像・音・操作・アシスト・言語・アクセシビリティ） | `Saved/Config/<Platform>/GameUserSettings.ini` | `UTennisUserSettings`（`UGameUserSettings` 派生） | 端末ごと（同期しない） |
| キーの割り当て | Enhanced Input のユーザー設定（`UEnhancedInputUserSettings`） | — | 端末ごと |
| タッチの HUD の配置 | プロファイル | `UTennisProfileSave::TouchLayout` | 端末ごと |
| 進行（統計・トーナメント・解放） | `Saved/SaveGames/Profile_0.sav` | `UTennisProfileSave` | クロスセーブ（EOS のプレイヤーデータ、S 要件） |
| 中断した試合 | 同上（`Suspended`） | `FMatchSnapshot` | 端末ごと |
| リプレイ | `Saved/SaveGames/Replays/*.trpl` | `FPointRecording` の列 | 端末ごと |
| AI のショット表のキャッシュ | `Saved/ShotTables/*.bin` | `FTennisShotTable` | 端末ごと（消してもよい） |

## 2. `UTennisProfileSave` の構造

```cpp
UCLASS() class UTennisProfileSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 SaveVersion = 1;              // §4 の移行に使う
    UPROPERTY() FGuid ProfileId;
    UPROPERTY() FDateTime LastSaved;

    UPROPERTY() TMap<FName, FPlayerCareerStats> CareerByPlayer;   // 選手ごとの通算
    UPROPERTY() FModeStats Exhibition, Practice, Tournament, Online;
    UPROPERTY() TArray<FTournamentProgress> Tournaments;          // 進行中・完了
    UPROPERTY() TSet<FName> Unlocked;                             // 会場・選手・服の解放

    UPROPERTY() TArray<FSavedReplayRef> SavedReplays;             // リプレイのファイルへの参照
    UPROPERTY() FTouchLayout TouchLayout;                         // タッチの HUD
    UPROPERTY() TOptional<FMatchSnapshot> Suspended;              // 中断した試合（1 つだけ）

    void Migrate(int32 FromVersion);
};
```

| 構造体 | 主なフィールド |
|---|---|
| `FPlayerCareerStats` | Matches、Wins、SetsWon/Lost、GamesWon/Lost、Aces、DoubleFaults、Winners、UnforcedErrors、FirstServeIn/Total、FastestServeKmh、LongestRally、DistanceRunM、SurfaceWins（面ごと） |
| `FModeStats` | Played、Won、PlayTimeSec |
| `FTournamentProgress` | TournamentId、Round、Bracket（対戦の結果）、Seed、bCompleted |
| `FTouchLayout` | Scheme（Buttons/Swipe）、ボタンごとの位置（画面比 0〜1）・大きさ・透明度、bMirrored、SwipeSensitivity（3 段階） |
| `FMatchSnapshot` | [DD-04 §9](../04_DetailedDesign/04_Rules_Match_Detail.md) |

## 3. 保存のタイミング

| 時 | 内容 |
|---|---|
| 試合の終了 | 統計・進行を保存する |
| ポイント間・エンドチェンジ・セットブレイク | `Suspended` を上書き（オフラインのみ。60 秒に 1 回まで） |
| アプリの中断（スマートフォン） | 直ちに `Suspended` を保存（ポイント中ならポイントの開始時の状態） |
| 設定の変更 | 「適用」を押したとき |
| 失敗したとき | 前の版を `Profile_0.bak` に残す。読み込みで壊れていたら `.bak` を使う |

保存は `UGameplayStatics::AsyncSaveGameToSlot` で非同期に行う。

## 4. 版と移行

| SaveVersion | 変更 | 移行の処理 |
|---|---|---|
| 1 | 初版 | — |
| 2（予定） | ダブルスの統計を追加 | 既存の統計をシングルスとして扱う |

- 読み込み時に `SaveVersion` が古ければ `Migrate()` で順に上げる。新しすぎる（将来の版）場合は読み込まず、警告を出す
- 自動テスト：版 1 のサンプルのセーブを `TennisTests/Data/` に置き、最新の版で読めることを確認する（T-SAVE-001）

## 5. 設定項目（`UTennisUserSettings`）

### 5.1 映像

| 項目 | 選択肢 | 既定（PC） | 既定（スマートフォン） |
|---|---|---|---|
| 解像度・表示モード | — | デスクトップの解像度・ボーダーレス | 端末の解像度 |
| 品質のプリセット | Mobile／低／中／高／最高 | 自動の判定 | Device Profile |
| フレームレートの上限 | 30／60／120／144／無制限 | 60 | 60（端末が対応していれば） |
| アンチエイリアス | TSR／TAA／なし | TSR | TAA |
| GI | Lumen／Lumen Lite／なし | Lumen（高以上） | なし |
| 画面の揺れ | 0〜100% | 30% | 30% |
| フラッシュを減らす | オン／オフ | オフ | オフ |

### 5.2 音

全体、効果音、観客、音声（主審）、音楽（各 0〜100%）。字幕（オン／オフ、大きさ 3 段階）。

### 5.3 操作

| 項目 | 選択肢 |
|---|---|
| アシスト | シミュレーション／スタンダード／カジュアル／タッチ向け |
| アシストの個別設定 | タイミングの窓、移動のアシスト、狙いのマージン、着地点マーカー、タイミングのメーター、ミスの理由、自動のスプリットステップ（プリセットから変えると「カスタム」） |
| 狙いの方式 | スティック／マウス（PC） |
| 押している間の移動 | 自動で打点へ（既定）／移動を続ける（シミュレーションのみ） |
| 振動 | 0〜100% |
| タッチの方式 | ボタン／スワイプ |
| キーの割り当て | すべての Action |
| 片手操作のプリセット | オン／オフ |

### 5.4 ゲーム

| 項目 | 選択肢 |
|---|---|
| 言語 | 日本語／英語 |
| 速さの単位 | km/h／mph |
| 距離の単位 | m／ft |
| カメラ | 中継（高）／中継（低）／選手の後方。サーブの寄り（オン／オフ） |
| 演出 | リプレイ（毎回／ハイライトのみ／なし）、入場（オン／オフ）、ショットクロックの表示 |
| 色覚のパレット | 標準／1 型／2 型／3 型 |

## 6. テスト

| ID | 内容 |
|---|---|
| T-SAVE-001 | 版 1 のセーブを最新の版で読める |
| T-SAVE-002 | 保存の途中で強制終了しても、`.bak` から復元できる |
| T-SAVE-003 | 設定を変えて再起動しても残っている |
| T-SAVE-004 | `Suspended` から再開して、スコア・統計・スタミナ・乱数の状態が一致する |
| T-SAVE-005 | クロスセーブ（EOS）：PC で進めた統計がスマートフォンに反映される |
