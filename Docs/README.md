# RealTennis 設計ドキュメント 索引

Unreal Engine 5.8 で制作する、物理ベースのリアルなテニスゲーム「RealTennis」（仮称）の設計ドキュメントです。

| 項目 | 内容 |
|---|---|
| 対象 | 企画・プログラム・アート・アニメーション・サウンド・QA の全員 |
| エンジン | Unreal Engine 5.8.4（C++ ＋ ブループリント） |
| プラットフォーム | Windows PC（主、v1.0）、iOS／Android（v1.1） |
| 基にした文書 | [UE5_Tennis_Technical_Design.md](UE5_Tennis_Technical_Design.md)（既存の技術設計書。変更しない。改訂点は [RES-05](00_Research/05_ExistingDesign_Review.md)） |
| 書式 | Markdown。図は Mermaid（GitHub と VS Code で表示できる）。CI が全リンクと全図を検査する |

---

## 1. フォルダ構成

| フォルダ | 内容 | ID の接頭辞 |
|---|---|---|
| `00_Research/` | 調査・分析（規則、物理、統計、技術評価、既存設計のレビュー） | RES |
| `01_Specification/` | 仕様書（ゲーム仕様、機能仕様、非機能要件、操作、ルール判定） | SPEC |
| `02_Architecture/` | 設計図（システム構成、モジュール、コーディング規約、Unreal MCP を使った開発） | ARCH |
| `03_UML/` | UML 図（ユースケース、クラス、シーケンス、状態、アクティビティ） | UML |
| `04_DetailedDesign/` | 詳細設計（サブシステムごと） | DD |
| `05_Data/` | データ設計（データモデル、パラメータ表、セーブと設定） | DATA |
| `06_UI_UX/` | 画面設計（画面遷移、HUD） | UI |
| `07_Art_Animation/` | アセットとアニメーション、制作パイプライン | ART |
| `08_QA/` | テスト計画、性能予算 | QA |
| `09_Project/` | ロードマップ・WBS、リスク | PM |
| [`Screenshots/`](Screenshots/README.md) | 対応ごとの画面の記録（日付別・対応内容別。PR にも載せる） | — |

## 2. ドキュメント一覧

### 00 調査・分析

| ID | 文書 | 内容 |
|---|---|---|
| RES-01 | [テニス規則（ITF 2026）](00_Research/01_Tennis_Rules_Research.md) | 2026 年版の規則の原文から、寸法・ボール・得点・サーブ・失点の条件を整理 |
| RES-02 | [ボール物理](00_Research/02_Ball_Physics_Research.md) | 空力・バウンド・CPR・環境。数値実験（[参照スクリプト](../Tools/Reference/ballsim_reference.py)） |
| RES-03 | [選手の動きと試合の統計](00_Research/03_Player_Match_Stats_Research.md) | 球速・回転・反応・移動・ラリーの長さ。リアリティ KPI |
| RES-04 | [UE 5.8 の技術評価](00_Research/04_UE58_Technology_Assessment.md) | プラグインの成熟度（ローカルで確認）と採用判断、MCP、モバイル |
| RES-05 | [既存設計のレビュー](00_Research/05_ExistingDesign_Review.md) | 既存の技術設計書の検算と 28 件の指摘、改訂方針 |
| RES-06 | [UE 5.8 のストロークとロコモーションの API](00_Research/06_UE58_Animation_Stroke_API.md) | Chooser・Motion Warping・モンタージュと Notify・PoseSearch の API（エンジンのソースで確認）と、DD-03 とのずれ |

### 01 仕様書

| ID | 文書 | 内容 |
|---|---|---|
| SPEC-01 | [ゲーム仕様書](01_Specification/01_Game_Specification.md) | コンセプト、柱、モード、選手、コート、試合形式、アシスト、スコープ |
| SPEC-02 | [機能仕様書](01_Specification/02_Functional_Specification.md) | ID 付きの機能要件と受け入れ基準 |
| SPEC-03 | [非機能要件](01_Specification/03_NonFunctional_Requirements.md) | 動作環境・性能・決定論・ネットワーク・品質・アクセシビリティ・国際化・法務 |
| SPEC-04 | [操作仕様書](01_Specification/04_Controls_Specification.md) | ゲームパッド・キーボード・タッチ、Enhanced Input、タイミングと狙い |
| SPEC-05 | [ルール判定仕様書](01_Specification/05_Rules_Specification.md) | サーブとラリーの判定表、スコアの規則（条番号付き） |

### 02 設計図（アーキテクチャ）

| ID | 文書 | 内容 |
|---|---|---|
| ARCH-01 | [システム構成](02_Architecture/01_System_Architecture.md) | レイヤー、ランタイムの構成、データの流れ、Tick の順序、イベント、配置図 |
| ARCH-02 | [モジュール・パッケージ設計](02_Architecture/02_Module_Package_Design.md) | 4 モジュール、Build.cs、フォルダ構成、C++ の追加手順、命名 |
| ARCH-03 | [コーディング規約](02_Architecture/03_Coding_Standards.md) | 単位・決定論・ログ・CVar・テストの書き方 |
| ARCH-04 | [Unreal MCP を使った開発の設計](02_Architecture/04_Unreal_MCP_Workflow.md) | 確認したツールの能力と不足、MCP で確かめられる設計、専用のツールセット、開発のループ |

### 03 UML 図

| ID | 文書 | 内容 |
|---|---|---|
| UML-01 | [ユースケース図](03_UML/01_UseCase_Diagrams.md) | アクターとユースケース |
| UML-02 | [クラス図](03_UML/02_Class_Diagrams.md) | サブシステムごとのクラスと関係 |
| UML-03 | [シーケンス図](03_UML/03_Sequence_Diagrams.md) | 打球、サーブ、ポイントの終了、オンラインの打球と受ける側の補正、AI、リプレイ、アプリの中断、試合の開始 |
| UML-04 | [状態図](03_UML/04_State_Diagrams.md) | 試合、ポイント、サーブ、選手、ショットの入力、AI、カメラ、ボール、オンラインのセッション |
| UML-05 | [アクティビティ図](03_UML/05_Activity_Diagrams.md) | ボールの 1 ステップ、ソルバ、打点の探索、インパクト、判定、スコア、スワイプの判別 |

### 04 詳細設計

| ID | 文書 | 内容 |
|---|---|---|
| DD-01 | [ボール物理](04_DetailedDesign/01_BallPhysics_Detail.md) | 座標、積分、バウンド v2、ネット、体との衝突、予測、イベント、環境 |
| DD-02 | [ショット](04_DetailedDesign/02_ShotSystem_Detail.md) | 球種、意図から要求まで、ソルバ v2、品質と誤差、能力値 |
| DD-03 | [移動とストローク](04_DetailedDesign/03_Player_Movement_Stroke_Detail.md) | 移動、スプリットステップ、打点の探索、ストロークの選択、同期 |
| DD-04 | [ルールと試合の進行](04_DetailedDesign/04_Rules_Match_Detail.md) | ルールエンジン、試合の進行、統計、審判 |
| DD-05 | [AI](04_DetailedDesign/05_AI_Detail.md) | StateTree、知覚、ショットの選択、スタイル、難易度 |
| DD-06 | [ネットワーク](04_DetailedDesign/06_Network_Detail.md) | 打球イベントの同期、遅延補償、補正、クロスプレイ |
| DD-07 | [カメラと演出](04_DetailedDesign/07_Camera_Presentation_Detail.md) | 中継カメラ、リプレイ、電子ラインコールの再現、観客、審判 |
| DD-08 | [サウンド](04_DetailedDesign/08_Audio_Detail.md) | MetaSounds、ミックス、観客、主審の音声 |

### 05 データ設計

| ID | 文書 | 内容 |
|---|---|---|
| DATA-01 | [データモデル](05_Data/01_Data_Model.md) | ER 図、DataAsset・DataTable・Chooser のスキーマ |
| DATA-02 | [パラメータ表](05_Data/02_Parameter_Tables.md) | 初期値と出典（実測・二次・設計推定）、検証のテスト ID |
| DATA-03 | [セーブデータと設定](05_Data/03_SaveData_Settings.md) | SaveGame の構造、設定項目、バージョン移行 |

### 06 画面設計

| ID | 文書 | 内容 |
|---|---|---|
| UI-01 | [画面遷移](06_UI_UX/01_Screen_Flow.md) | 画面の一覧と遷移図 |
| UI-02 | [HUD ワイヤーフレーム](06_UI_UX/02_HUD_Wireframes.md) | PC とタッチの HUD、スコアボード、メーター、ミスの理由 |

### 07 アート・アニメーション

| ID | 文書 | 内容 |
|---|---|---|
| ART-01 | [アセット一覧](07_Art_Animation/01_Asset_List.md) | ID・仕様・予算・優先度・ライセンス台帳 |
| ART-02 | [アニメーション一覧](07_Art_Animation/02_Animation_List.md) | ストロークの行列、ロコモーション、Notify の規則 |
| ART-03 | [制作パイプライン](07_Art_Animation/03_Pipeline_Blender_UE.md) | Blender（MCP）→ UE、MetaHuman、モーションキャプチャ |

### 08 QA

| ID | 文書 | 内容 |
|---|---|---|
| QA-01 | [テスト計画](08_QA/01_Test_Plan.md) | テストの種類、自動テストの一覧、プレイテスト、KPI の計測 |
| QA-02 | [性能予算](08_QA/02_Performance_Budget.md) | フレーム・メモリ・描画の予算と計測の方法 |

### 09 プロジェクト

| ID | 文書 | 内容 |
|---|---|---|
| PM-01 | [ロードマップ・WBS](09_Project/01_Roadmap_WBS.md) | マイルストーン M0〜M10、WBS、ガントチャート |
| PM-02 | [リスク表](09_Project/02_Risk_Register.md) | リスク、影響、対策、担当 |

## 3. 役割ごとの読む順番

| 役割 | 順番 |
|---|---|
| 全員（最初に） | SPEC-01 → ARCH-01 → PM-01 |
| プログラマー（ゲームプレイ） | RES-05 → SPEC-02 → ARCH-02 → ARCH-03 → ARCH-04 → UML-02・03・04 → DD-01〜05 → DATA-01 → QA-01 |
| エージェント（Claude Code など） | 開発ルール集（非公開）（§0 の地図、§9 のエージェントの運用：エージェント設定（非公開））→ ARCH-04 → 作業に関係する詳細設計 → QA-01 |
| プログラマー（ネットワーク・プラットフォーム） | SPEC-03 → DD-06 → ARCH-01 §7 → SPEC-04 §8 |
| ゲームデザイナー | SPEC-01 → SPEC-04 → SPEC-05 → RES-03 → DATA-02 → DD-05 |
| アーティスト・アニメーター | ART-01 → ART-02 → ART-03 → DD-03 §4〜6 |
| サウンド | DD-08 → ARCH-01 §6（イベント一覧） |
| UI | UI-01 → UI-02 → SPEC-04 |
| QA | SPEC-02 → SPEC-05 → QA-01 → QA-02 → RES-03 §4 |

## 4. 共通の約束

| 項目 | 約束 |
|---|---|
| ID | 要件 `FR-<領域>-<番号>`、非機能 `NFR-<分類>-<番号>`、テスト `T-<領域>-<番号>`、アセット `A-<分類>-<番号>`、リスク `R-<番号>` |
| 優先度 | M（v1.0 に必須）／S（入れたい）／C（余裕があれば）／M¹（v1.1 のスマートフォン版で必須） |
| 数値の出典 | **[実測]**（論文・公式統計）／**[二次]**（解説記事など）／**[設計推定]**（後で検証する） |
| 単位 | 物理は m・s・kg・rad/s。UE に渡すときだけ cm（×100）。表示は km/h と rpm |
| ルール | ITF *Rules of Tennis* 2026 年版の条番号で参照する（例：Rule 24(i)） |
| 優先順位 | 既存の技術設計書と食い違う場合は、このフォルダの新しい文書を優先する |

## 5. 変更履歴

| 日付 | 版 | 内容 |
|---|---|---|
| 2026-11-04 | 1.0 | 初版。調査分析（5）・仕様書（5）・設計図（3）・UML（5）・詳細設計（8）・データ（3）・画面（2）・アート（3）・QA（2）・計画（2） |
| 2026-11-04 | 1.0 | 方針の追加：スマートフォン（iOS／Android）に対応する（v1.1）。性能の要件は当面定めない |
| 2026-11-04 | 1.1 | Unreal MCP を使った開発の設計（ARCH-04）を追加。MCP のツールの能力を `describe_toolset` で確認し、ART-03 の取り込みの記述（MCP の標準のツールには取り込みがない）と RES-04 の CVar の記述を修正。開発ルール集（非公開） に MCP の使い方を追加 |
