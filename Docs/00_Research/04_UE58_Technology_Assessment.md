# 調査報告 04：UE 5.8 の技術評価と採用判断

| 項目 | 内容 |
|---|---|
| 文書 ID | RES-04 |
| 調査日 | 2026-11-04 |
| 調査方法 | (1) インストール済みエンジン `C:\Program Files\Epic Games\UE_5.8` の `Build.version` と各 `.uplugin` の成熟度フラグを直接読んだ。(2) Unreal MCP（localhost:8000/mcp）でエディタのツールセット一覧を取得した。(3) 5.8 のリリース情報を Web で確認した |
| 関連 | [システム構成](../02_Architecture/01_System_Architecture.md)、[モジュール設計](../02_Architecture/02_Module_Package_Design.md) |

---

## 1. 開発環境（実測）

| 項目 | 値 |
|---|---|
| エンジン | **UE 5.8.4**（Changelist 58902052、Release-5.8 ブランチ、Promoted Build） |
| IDE | Visual Studio Community 2026（18.6） |
| プロジェクト | `Tennis.uproject`（EngineAssociation 5.8）。有効なプラグインは `Tennis.uproject` の `Plugins` が正（2026-11-19：`ModelContextProtocol`・`AllToolsets`・`MetaHumanCharacter`（W-3.10）・`HairStrands`（W-3.10）・`Chooser`（W-3.4）・`PoseSearch`（W-3.6）） |
| ソース | **`Source/` フォルダがない（ブループリントのみのプロジェクト）** → C++ モジュールを追加する必要がある（[モジュール設計](../02_Architecture/02_Module_Package_Design.md) §5） |
| Content | 空（`Collections`、`Developers/<個人名>` のみ） |
| Blender | 5.2.3 LTS、MCP for Blender アドオン v1.8（プロトコル 15、最新）。アセットライブラリと 3D 生成はすべてオフ |

## 2. UE 5.8 の主な変更（Web）

- 5.8 は 2026 年 6 月に正式リリース（Epic のフォーラム）
- キャラクター作成とアニメーションのツールを強化し、外部アプリへの依存を減らした
- **Game Animation Sample が 5.8 向けに更新された**。複数の Skeletal Mesh をまたいで Pose Search を行うツールと、Mover Pawn の Blend Stack を新しい Chooser（改良された Pose Match Column 付き）で作り直したものが入っている
- **Lumen Lite**（より軽いグローバルイルミネーション）が選べる（PC Gamer）
- LLM 向けの標準プロトコル（MCP）のプラグインが入った（このプロジェクトで使っている `ModelContextProtocol`）
- Mover は 5.8 でも Experimental のまま（フォーラム。ローカルの `.uplugin` でも確認）

## 3. プラグインの成熟度（ローカルの `.uplugin` から）

| プラグイン | 場所 | Beta | Experimental | 既定で有効 | 判定 |
|---|---|---|---|---|---|
| EnhancedInput | EnhancedInput | – | – | ○ | 正式版 |
| PoseSearch（Motion Matching） | Animation | – | – | – | 正式版 |
| Chooser | Chooser | – | – | – | 正式版 |
| MotionWarping | Animation | **○** | – | – | **Beta** |
| AnimationWarping（Orientation/Stride） | Animation | – | – | – | 正式版 |
| ControlRig / IKRig | Animation | – | – | ○ | 正式版 |
| MotionTrajectory | Experimental/Animation | – | **○** | – | **Experimental** |
| StateTree / GameplayStateTree | Runtime | – | – | – | 正式版 |
| SmartObjects | Runtime | – | – | – | 正式版（不要） |
| Mover / ChaosMover | Experimental | – | **○** | – | **Experimental** |
| NetworkPrediction | Runtime | **○** | – | – | Beta |
| Iris | Experimental | **○** | – | – | Beta |
| CommonUI | Runtime | – | – | – | 正式版 |
| GameplayAbilities | Runtime | – | – | – | 正式版 |
| GameFeatures / ModularGameplay | Runtime | **○** | – | – | Beta |
| Metasound | Runtime | – | – | ○ | 正式版 |
| AudioModulation | Runtime | – | – | – | 正式版 |
| Harmonix | Runtime | – | **○** | – | Experimental |
| Niagara | FX | – | – | ○ | 正式版 |
| ChaosCloth | ChaosCloth | – | – | ○ | 正式版 |
| GameplayCameras | Cameras | – | **○** | ○ | **Experimental（既定で有効）** |
| PCG | PCG | – | – | ○ | 正式版 |
| Interchange | Interchange | – | – | ○ | 正式版 |
| Dataflow | Dataflow | – | – | ○ | 正式版 |
| HairStrands | Runtime | – | – | – | 正式版 |
| MetaHuman（Animator） | MetaHuman | – | – | – | 正式版 |
| MetaHumanCharacter | MetaHuman | **○** | – | – | Beta |
| AnimToTexture | Experimental | – | **○** | – | Experimental |
| LearningAgents | Experimental | – | **○** | – | Experimental |
| NNERuntimeORT | NNE | **○** | – | – | Beta |
| CQTest | Tests | – | – | – | 正式版 |
| Gauntlet | Experimental | **○** | – | – | Beta |
| SemanticSearch | Experimental | – | **○** | – | Experimental |
| ModelContextProtocol / AllToolsets | Experimental | – | **○** | – | Experimental（エディタ専用・開発用） |
| LiveLink | Animation | – | – | – | 正式版 |
| OnlineSubsystemEOS | Online | – | – | – | 正式版（対応プラットフォームの指定あり） |
| OnlineServicesEOS / EOSShared | Online | – | – | – | 正式版 |
| OnlineSubsystemSteam | Online | – | – | – | 正式版（Windows など） |
| OnlineSubsystemIOS / OnlineSubsystemApple | Online/IOS ほか | – | – | – | 正式版 |
| OnlineSubsystemGooglePlay | Online/Android | – | – | – | 正式版 |
| GooglePAD（Play Asset Delivery） | Runtime | – | – | – | 正式版（Android ほか） |
| MobilePatchingUtils | Runtime | – | – | – | 正式版 |
| AndroidPermission | Runtime | **○** | – | – | Beta |

エンジンの `Engine/Platforms` には **Android** と **IOS** のフォルダがあり、モバイルのプラットフォームの対応はインストールされている（iOS のビルドには Mac が必要）。

## 4. 採用判断

### 4.1 判定基準

1. **ゲームの中核（毎フレーム動き、出荷物に入る）には、正式版だけを使う**
2. Beta は、代わりの手段がある場合に限って使う（代替策を設計書に書く）
3. Experimental は、出荷物に含めない。エディタ専用のツールとしてなら使ってよい

### 4.2 機能ごとの判断

| 機能 | 採用 | 不採用／保留 | 理由 |
|---|---|---|---|
| キャラクター移動 | **CharacterMovementComponent（CMC）派生** | Mover（Experimental） | CMC は正式版で、クライアント予測が成熟している。テニスは移動が単純（地上のみ、ジャンプはスマッシュのみ）なので CMC で十分。5.9 以降で Mover が正式版になったら再評価する |
| ロコモーションのアニメ | **PoseSearch（Motion Matching）＋ AnimationWarping** | — | 正式版。サイドステップ・クロスオーバー・後退・急停止をデータベースでつなげられる。Game Animation Sample（5.8）の構成を参考にする |
| ストロークの選択 | **Chooser Table** | DataTable の自前検索 | 正式版。「サイド × 高さ × 到達 × 球種」の多条件の選択を、エディタで見える表で管理できる。既存設計の DT_Strokes を Chooser に置き換え、メタデータ（インパクト時刻など）は別の DataAsset にする |
| 打点の位置合わせ | **移動のモデル（`TennisMovement` の自動移動）＋その場のモンタージュ** | MotionWarping（Beta） | root motion のモンタージュの間は移動のモデルが効かないので、Motion Warping は使わない（[RES-06](06_UE58_Animation_Stroke_API.md) G-1、DD-03 §6.2。2026-11-13 に変更）。プラグインそのものは、PoseSearch の依存として有効になる（G-6） |
| 腕・ラケットの微調整 | **ControlRig（Full Body IK）、IKRig** | — | 正式版 |
| 軌跡の予測（Motion Matching 用） | GASP 5.8 版と同じ方式 | MotionTrajectory（Experimental）に直接依存しない | 5.8 の Game Animation Sample で使われている軌跡生成の仕組みを確認してから決める（[リスク表](../09_Project/02_Risk_Register.md) R-07） |
| AI | **StateTree ＋ GameplayStateTree** | Behavior Tree | 正式版。状態遷移が明確なテニスの AI（サーブ／リカバリー／迎撃／打球）に合う |
| ネットワーク | **標準のレプリケーション** | Iris（Beta）、NetworkPrediction（Beta） | 1v1 で帯域が小さい。ボールは打球イベントだけを同期する設計なので、標準で足りる |
| UI | **CommonUI ＋ UMG** | — | 正式版。ゲームパッドでの画面操作と入力ルーティングが標準で揃っている |
| サウンド | **MetaSounds ＋ AudioModulation** | Harmonix（Experimental） | 正式版。打球音を速度・品質で変えるのに MetaSounds のパラメータを使う。観客のダッキングは Modulation |
| エフェクト | **Niagara** | — | 正式版 |
| カメラ | **APlayerCameraManager 派生＋自前のカメラディレクター** | GameplayCameras（Experimental） | GameplayCameras は既定で有効だが Experimental。中継カメラは要件が単純なので自前で作る |
| 能力値・スタミナ | **自前のコンポーネント** | GameplayAbilities | GAS は正式版だが、テニスでは効果の種類が少なく過剰。必要になったら導入する |
| 選手のモデル | **MetaHumanCharacter（Beta）でエディタ内で作成** → 静的なアセットとして使う | — | 作成ツールとしてだけ使う（実行時の依存はない）。体は Manny 互換の骨格に IK Retargeter で合わせる |
| 髪 | **ヘアカード**（ゲームプレイ用）、ストランドはリプレイのアップだけ | ストランドを常時使う | 性能。MetaHuman のカードは Groom のアセットの LOD で、`GroomComponent` で描く（2026-11-18 に確かめた。DD-03 §7） |
| 服 | **ChaosCloth**（シャツの裾・スカート、LOD0 のみ） | — | 正式版 |
| ネットの揺れ | **マテリアルの WPO** | ChaosCloth | 既存設計どおり。安く、十分に見える |
| 観客 | **Instanced Static Mesh ＋ PCG での配置** | AnimToTexture（Experimental） | 5.8 の MetaHuman の群衆機能は要調査（R-08） |
| GI | **Lumen**（品質設定「高」）／**Lumen Lite**（「中」以下） | — | スタジアムの照明。性能予算は [NFR](../01_Specification/03_NonFunctional_Requirements.md) |
| テスト | **Automation Spec ＋ CQTest**、AI 対 AI の長時間試験に **Gauntlet（Beta）** | — | Gauntlet は開発ツールなので Beta でも可 |
| AI の学習 | — | LearningAgents（Experimental） | 研究テーマとして保留（AI のスタイル調整に将来使う可能性） |
| データ | **PrimaryDataAsset、DataTable、DeveloperSettings、GameplayTags** | — | すべてエンジンの中核機能 |
| オンライン | **Online Subsystem EOS**（ロビー・招待・P2P・プレイヤーデータ）＋各プラットフォームの ID（Steam／Apple／Google） | Steam だけ | PC とスマートフォンのクロスプレイ（2026-11-04 の方針変更）。Steam はスマートフォンに対応しない |
| タッチ入力 | **UMG のタッチウィジェット＋ `InjectInputForAction`** | 旧式の Touch Interface | Enhanced Input の同じ Action に流せる（[操作仕様](../01_Specification/04_Controls_Specification.md) §8.4） |
| モバイルの配信 | **GooglePAD**（Android の大きなアセット）、iOS は On-Demand Resources を検討 | — | ストアのアプリの容量の上限への対策。M10 で確定する |

### 4.3 `.uproject` で有効にするプラグイン

```json
"Plugins": [
  { "Name": "ModelContextProtocol", "Enabled": true },
  { "Name": "AllToolsets", "Enabled": true },
  { "Name": "PoseSearch", "Enabled": true },
  { "Name": "Chooser", "Enabled": true },
  { "Name": "AnimationWarping", "Enabled": true },
  { "Name": "StateTree", "Enabled": true },
  { "Name": "GameplayStateTree", "Enabled": true },
  { "Name": "CommonUI", "Enabled": true },
  { "Name": "AudioModulation", "Enabled": true },
  { "Name": "Gauntlet", "Enabled": true },
  { "Name": "MetaHumanCharacter", "Enabled": true },
  { "Name": "OnlineSubsystemEOS", "Enabled": true },
  { "Name": "EOSShared", "Enabled": true },
  { "Name": "OnlineSubsystemSteam", "Enabled": true, "PlatformAllowList": [ "Win64" ] },
  { "Name": "OnlineSubsystemIOS", "Enabled": true, "PlatformAllowList": [ "IOS" ] },
  { "Name": "OnlineSubsystemGooglePlay", "Enabled": true, "PlatformAllowList": [ "Android" ] },
  { "Name": "GooglePAD", "Enabled": true, "PlatformAllowList": [ "Android" ] }
]
```

オンラインのプラグインは M8（オンライン対戦）で、モバイルのプラグインは M10（スマートフォン版）で有効にする。それまでは無効のままでよい。

**有効にする時期**：上の一覧は最終的な形。各プラグインは、それを使うマイルストーンで有効にする（YAGNI。例：PoseSearch・Chooser は M3、StateTree は M6）。M0 で有効なのは ModelContextProtocol・AllToolsets だけ。

**CQTest**：テストの基盤の CQTest は**エンジンの Developer モジュール**（`Engine/Source/Developer/CQTest`）で、`TennisTests.Build.cs` の依存に書くだけで使える。`CQTest` という名前のプラグインは CQTest 自身のテスト（`CQTestTests`）だけなので、有効にしない（2026-11-04 にエンジンのファイルで確認）。

EnhancedInput、Metasound、Niagara、ControlRig、IKRig、PCG、ChaosCloth は既定で有効なので書かない。GameplayCameras は既定で有効だが使わない（無効化しても依存関係の問題がなければ、M1 で無効にする）。

### 4.4 スマートフォン対応の技術方針（性能の要件は未定）

| 項目 | PC | スマートフォン | 方針 |
|---|---|---|---|
| レンダラー | Deferred、Lumen（または Lumen Lite）、VSM、TSR | モバイルのレンダラー（Mobile Forward／Deferred）。Lumen と VSM は前提にしない | 照明は「動的なスタジアムの照明＋事前計算の空」に分け、モバイルでは事前計算の比率を上げる |
| メッシュ | スタジアムに Nanite | Nanite を前提にしない | **すべてのメッシュに通常の LOD を用意する**（NFR-MOB-002） |
| 髪・服 | ヘアカード、ChaosCloth（LOD0） | ヘアカードの低い LOD、Cloth なし | — |
| キャラクター | MetaHuman 由来のメッシュ（LOD0〜3） | LOD2 以下 | — |
| 観客 | Instanced Static Mesh | 枚数を減らしたカード | — |
| 品質の切り替え | Scalability（低・中・高・最高） | Device Profile（端末ごと）＋ Scalability の「Mobile」の段 | — |
| 物理・ルール・AI | 同じ | 同じ | 固定ステップ（240 Hz）なので、描画のフレームレートの影響を受けない |
| 入力 | ゲームパッド、キーボードとマウス | タッチ（2 方式）、ゲームパッド | `FShotIntent` に統一する |

**検証の順番**：M4 で、PC のエディタでタッチの試作（マウスでタッチを模擬）を作り、操作の手応えを先に確かめる。M10 で実機のビルドと性能の調整を行う（[ロードマップ](../09_Project/01_Roadmap_WBS.md)）。

**Android File Server（AFS）**：M10 までは無効（`Config/DefaultEngine.ini` の `bEnablePlugin=False`・`bAllowNetworkConnection=False`・`SecurityToken=` は空）。エンジンのソース（`AndroidFileServerRuntimeSettings.cpp`）では、トークンが `[AUTO]` のときに新しい値を生成して **`DefaultEngine.ini` に書き込む**ため、有効にするときはトークンを追跡しない設定（`Saved/Config/<Platform>/Engine.ini` など、開発者ごと）に置く。置き方は M10 の開始時に確かめる。

### 4.5 C++ の注意点（UE 5.8）

| 項目 | 内容 |
|---|---|
| 大規模ワールド座標（LWC） | `FVector` は `double`。ボール物理は**すべて double で計算**し、`float` との混在を避ける。ネットワークに送るときだけ量子化する |
| 決定論 | 同じ実行ファイル・同じ CPU 系列なら、同じ入力から同じ結果が出るようにする（`FMath` の関数だけを使う、`-ffast-math` 相当の最適化に頼らない）。異なるプラットフォーム間（x64 と ARM64 を含む）の一致は保証しない（バウンドイベントで補正する） |
| Tick の順序 | ボールの Tick は `TG_PrePhysics`。キャラクターに `AddTickPrerequisiteComponent` を付けて、順序を明示する |
| 自動テスト | 純粋な関数（物理・ソルバ・スコア）は UObject に依存させず、CQTest で高速に回す |

---

## 5. MCP を使った開発の方針

### 5.1 Unreal MCP（エディタ内のツールセット。実際に取得した一覧から）

| 用途 | ツールセット |
|---|---|
| レベルの構築・アクタの配置 | `SceneTools`、`ActorTools`、`PrimitiveTools` |
| アセットの作成・検査 | `AssetTools`、`StaticMeshTools`、`SkeletalMeshTools`、`TextureTools` |
| データ駆動 | `DataAssetTools`、`DataTableTools`、`CurveTableTools`、`StringTableTools` |
| マテリアル | `MaterialTools`、`MaterialInstanceTools` |
| ブループリント | `BlueprintTools`、`ObjectTools` |
| アニメーション | `ControlRigTools`、`SequencerTools` 一式（リプレイ・カットシーン） |
| AI | `StateTreeTools`（検査） |
| UI | `UMGToolSet` |
| エフェクト | Niagara ツールセット一式 |
| 配置 | `PCGToolset` |
| テスト | **`AutomationTestToolset`**（テストの探索・実行・結果の取得） |
| 実行・デバッグ | `EditorAppToolset`（PIE の制御、CVar、ビューポート）、`LogsToolset` |
| 設定 | `ConfigSettingsToolset`、`PluginToolset` |
| 一括処理 | `ProgrammaticToolset`（複数のツール呼び出しをまとめる） |

**使い方**：C++ のビルド後に、MCP で「自動テスト実行 → Project Settings でデバッグ表示を切り替え → PIE 起動 → ビューポートの画像とログで確認」までを行う。設計書の受け入れ条件をその場で確かめる。

**確認した制約**（`describe_toolset` で確認）：CVar を**設定する**ツールとコンソールコマンドを実行するツールがない（`EditorAppToolset` は CVar の検索だけ）。`AssetTools` には取り込みがない（`StaticMeshTools.import_file` はあるが新規の取り込みだけ。再取り込みと保存はプロジェクト専用のツールで補う）。`StateTreeTools` は**検査だけ**。`DataAssetTools` は作成だけ（値は `ObjectTools`）。Chooser・PoseSearch のツールセットはない。C++ のビルドは MCP の外で行う。これらを前提にした開発の設計は [ARCH-04](../02_Architecture/04_Unreal_MCP_Workflow.md) にまとめた。

### 5.2 Blender MCP（MCP for Blender v1.8）

| ツール | 用途 |
|---|---|
| `execute_blender_code` | `Tools/Blender/*.py`（コート・ネット・ボール・ラケットの生成スクリプト）を実行する |
| `look` | ビューポートを画像で確認する |
| `get_scene_info` | 寸法・ポリゴン数・トポロジーの検査 |
| `search_assets` / `import_asset` | Poly Haven などから背景の小物を入手する（**現在はオフ**。必要なら Blender のアドオン設定でオンにする） |
| `generate_3d` | AI による 3D 生成（**現在はオフ**。背景の小物だけに使う。クレジットを消費する場合がある） |

> 既存設計 §14 は `ahujasid/blender-mcp` を前提にしているが、接続中のものは「MCP for Blender」。ツール名と機能が違う（`look`、`get_scene_info` の検査項目など）。[パイプライン](../07_Art_Animation/03_Pipeline_Blender_UE.md) で更新した。

---

## 6. 出典

- Epic Developer Community, *Unreal Engine 5.8 Released* — https://forums.unrealengine.com/t/unreal-engine-5-8-released/2729274
- Unreal Engine Tech Blog, *Download the latest Game Animation Sample project, now updated for UE 5.8* — https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8
- PC Gamer, *Unreal Engine 5.8 launches with … a Lumen Lite option …* — https://pcgamer.com/software/unreal-engine-5-8-launches-with-improved-terrain-and-vegetation-tools-a-lumen-lite-option-for-faster-global-illumination-and-for-the-times-we-now-live-in-an-open-standard-plugin-for-llm-systems/
- 80.lv, *Unreal Engine 5.8 is out* — https://80.lv/articles/unreal-engine-5-8-is-out-today-with-big-optimization-improvements-and-mesh-terrain/
- 80.lv, *Populate your UE5.8 worlds with MetaHuman crowds* — https://80.lv/articles/populate-your-ue5-8-worlds-with-metahuman-crowds
- Epic Developer Community, *Any roadmap updates on Mover?* — https://forums.unrealengine.com/t/any-roadmap-updates-on-mover/2833898
