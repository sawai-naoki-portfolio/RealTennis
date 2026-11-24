# Blender のアセット生成スクリプト

寸法の決まったアセット（コート・ネット・ボール・ラケット・練習コートの小物・コートサイドの小物）を、Blender 5.2 の bpy で生成するスクリプトです。Blender MCP（MCP for Blender）の `execute_blender_code` から実行します。設計は [ART-03 制作パイプライン](../../Docs/07_Art_Animation/03_Pipeline_Blender_UE.md)、アセットの仕様は [ART-01 アセット一覧](../../Docs/07_Art_Animation/01_Asset_List.md) §2 にあります。

## 1. 構成

| パス | 内容 |
|---|---|
| `build_court.py` | コート：`SM_Court_Surface`（余白。ダブルスの区画を抜いた額縁）、`SM_Court_Inner`（ダブルスの区画）、`SM_Court_Lines`（ライン 11 本、1 mm 浮かせる）。コレクション `Tennis_Court` |
| `build_net.py` | ネット：`SM_Net`（横 64 × 縦 12、頂点カラー R＝揺れの重み）、`SM_NetBand`（白帯。上辺を包む U 字、同じ重み）、`SM_NetPost`・`SM_SinglesStick`（左右 2 本ずつを 1 メッシュ）。コレクション `Tennis_Net` |
| `build_ball.py` | ボール：`SM_Ball`（半径 3.35 cm、UV Sphere 32 × 16）、`SM_Ball_LOD1`（16 × 8）、`SM_Ball_Large`（Type 3 用、半径 3.575 cm）。3 つとも原点（球の中心）に重なる。コレクション `Tennis_Ball` |
| `build_racket.py` | ラケット：`SM_Racket`（全長 68.6 cm、打球面 32 × 25 cm の楕円、ストリング 16 × 19、三角形 2,088。スロット：グリップ・フレーム・ストリング）と、子のソケットの Empty `SOCKET_racket_sweetspot`・`SOCKET_grip`。原点と軸は §3。コレクション `Tennis_Racket` |
| `build_practice_court.py` | 練習コートの小物（A-STD-001）：`SM_Practice_Fence`（支柱 36 本・横桟・網の面）・`SM_Practice_Bench`（2 つ）・`SM_Practice_LightPole`（4 本）。コート局所座標。コレクション `Tennis_Practice` |
| `build_court_props.py` | コートサイドの小物（A-PRP-001〜006）：`SM_Prop_UmpireChair`（審判台）・`SM_Prop_BallMachine`（球出し機）・`SM_Prop_BallCan`（ボールの缶）・`SM_Prop_Cooler`（クーラーボックス）・`SM_Prop_ServeTarget`（サーブ練習の的）・`SM_Prop_Scoreboard`（電光掲示板）と、球出し機の子のソケットの Empty `SOCKET_muzzle`。箱と柱だけの仮の形（計 648 三角形）。どれも自分の局所座標なので、Blender では原点に重なる。コレクション `Tennis_Props` |
| `build_stadium_a.py` | センターコート A のスタジアムの仮の形（A-STD-010）：`SM_Stadium_A_Stands`（観客席。下の層・横の通路・上の層・手すり壁）・`SM_Stadium_A_Stands_LOD1`（段を斜面にした LOD1。LOD0 に重なる）・`SM_Stadium_A_Barrier`（看板の壁）・`SM_Stadium_A_Floor`（広げた余白と出入口の床）。コート局所座標。コレクション `Tennis_StadiumA` |
| `build_axis_test.py` | FBX の軸の確認用の矢印 `SM_AxisTest`（+X は矢じりつきの 1.0 m、+Y は 0.4 m、+Z は 0.2 m）。コレクション `Tennis_AxisTest` |
| `build_players.py` | 仮の選手 2 人（ART-01 A-CHR-004・005）：`SKM_Player_Male01`・`SKM_Player_Female01` と、その骨格（`*_Rig`）。入力は `Export/Base/` の Manny・Quinn（UE の `TennisTools.export_player_base_fbx`）。コレクション `Tennis_Players`。最後に、三角形の数と上限（60,000）を `[OK]`／`[NG]` で出す |
| `build_metahuman_wear.py` | MetaHuman の選手のテニスウェア（ART-01 A-CHR-040〜043）：`SKM_Wear_M01`・`SKM_Wear_F01` と、その骨格（`*_Rig`）。入力は `Export/Base/` の組み立てた体（UE の `TennisTools.export_player_base_fbx`）。体の殻として作り、体そのものは書き出さない。コレクション `Tennis_Wear`。三角形の数を出す。`export_all.py` で書き出し、UE の `import_exported_fbx` で取り込み、`apply_metahuman_wear` で選手に着せる（[ART-03 §6](../../Docs/07_Art_Animation/03_Pipeline_Blender_UE.md) の手順 5・6） |
| `build_stroke_anims.py` | 仮のストロークのアニメーション（フォア・バックと、低く打つ変種 `Dip`：DD-03 §5.3）：`Export/Base/SKM_Manny_Simple.fbx` の骨格に `stroke_motion` の動きを焼き、`Export/AS_*.fbx`（骨格とアニメーションだけ）と、Notify の時刻の `AS_*.json` を書き出す。コレクション `Tennis_StrokeAnims`（§5.4） |
| `export_all.py` | `Tennis_*` のコレクションの `SM_` のオブジェクトを 1 つずつ `Export/<名前>.fbx` に書き出す（§5）。選手の `SKM_` のメッシュは親の骨格ごと、UE → Blender → UE の往復で骨と参照姿勢が変わらない設定で書き出す（骨格のオブジェクトは、書き出しの間だけ UE の根の骨の名前 `root` にする） |
| `validate_dimensions.py` | 寸法の検査（許容 5 mm）。単位、各オブジェクトのバウンディングボックスとトランスフォーム、ラインの縁の座標（T-CRT-001）、面の数（分割の仕様）、ネットの上辺の高さ、揺れの重み、ソケットの親・位置・回転、ラケットの Appendix II の上限、三角形の数（ART-01 §1 の予算）。1 項目 1 行の `[OK]`／`[NG]` と、最後に全体の結果 |
| `tennis_blender/dims.py` | ITF 2026 の寸法の定数（m）と、ネット・ボールの分割数、ラケットの寸法（Appendix II の上限の中）と分割数・ソケットの位置。寸法はここにだけ書く |
| `tennis_blender/layout.py` | コートの面とラインの矩形の配置、矩形からメッシュの頂点と面を作る関数 |
| `tennis_blender/net_shape.py` | ネットの上辺の高さ（スティックの外は 1.07 m、内は中央の 0.914 m まで直線）、横の分割（スティックと中央に頂点）、揺れの重み、本体と白帯の頂点と面 |
| `tennis_blender/practice_court.py` | 練習コートの小物の形：フェンスの支柱の位置（角の間を等分）・横桟・網の面、ベンチ、照明柱と灯具 |
| `tennis_blender/court_props.py` | コートサイドの小物の形：審判台（脚・はしご・座面）、球出し機（本体・受け箱・車輪・発射口）、ボールの缶、クーラーボックス、サーブ練習の的、電光掲示板、ソケットの位置 |
| `tennis_blender/stadium_a.py` | スタジアム A の形：断面（列の段・通路・手すり壁）と、外周の長方形に沿った断面の位置（角は 45° の留め、縦の通路の帯）、断面を掃き出してスロットごとに分ける処理（開いた端のふた）、床、席の数の見積もり |
| `tennis_blender/primitives.py` | 形の部品：断面の八角形・正多角形、断面をつなぐ筒、縦の柱、軸にそろった箱、メッシュの結合とスロットの番号 |
| `tennis_blender/head_shape.py` | 仮の選手（ART-01 A-CHR-004・005）の頭と首：高さごとの断面（幅・前・後ろ）を単調な 3 次補間でつなぎ、顔（目のくぼみ・鼻・唇・あご・頬）を前の向きの変位で足す。唇の面、首から頭へのウェイト |
| `tennis_blender/head_parts.py` | 仮の選手の頭の部品：髪（生え際から頭頂への殻。短髪と、結んだ髪）・耳・目（白目と黒目）・眉・ポニーテールと結び目のゴム・ヘッドバンド。輪をつなぐ `loft`、`uv_sphere` |
| `tennis_blender/player_specs.py` | 仮の選手 2 人（`SKM_Player_Male01`・`SKM_Player_Female01`）の仕様：元の体（Manny・Quinn）、頭の倍率、髪型、服装（袖・襟ぐり・裾・スカート・丈・靴下）、マテリアル、三角形の上限。服の厚みなどの定数 |
| `tennis_blender/wear_specs.py` | MetaHuman の選手のテニスウェアの仕様：名前・元の体・服装（仮の選手と同じ。ヘッドバンドは後）と、殻の決まり（体の面より内へ入れない、平らにする回数、襟ぐりの後ろの半径、靴下の重なり、靴の凸包の大きさと三角形）。すねと足の頂点を靴と靴下に分ける関数 |
| `tennis_blender/player_regions.py` | 仮の選手の体の頂点の分け方：いちばん重いウェイトの骨と位置で、シャツ・ショートパンツ・靴・靴下・リストバンド・平らにする肌・消す部分（元の頭と、襟ぐりの中の首）に分ける。手の骨。MetaHuman の体の補助の骨（`*_twistCor_*`・`calf_knee_*`・足の指など）を、親をたどって Manny の骨にする（`manny_bone`） |
| `tennis_blender/mesh_ops.py` | メッシュの処理：隣り合い、mask を縮める（`erode`）、面の集まりの縁（`border_neighbors`・`boundary_loops`）、平らにする（`relax`、溝を埋めて元の面より下へ行かない `drape`。縁は縁に沿ってだけ）、近い頂点のウェイトを混ぜる（`blend_weights`）、縁の輪をなめらかにして筒にする（`loop_tube`） |
| `tennis_blender/skirt_shape.py` | 仮の選手のスカート：高さごとの体の外形に余裕を足し、下へ広げてひだを付けた輪（`skirt_rings`）と、腰から太ももへのウェイト |
| `tennis_blender/stroke_motion.py` | 仮のストロークの動き（フォア・バック。その場で振る）：キーの姿勢（腰・胸のひねり、スイートスポットの位置、ラケットの向き、もう一方の手首）、フレームごとの補間、2 本の骨の IK、ラケットの 3 軸、手のソケットの定義（Blender の骨の座標）、Notify の時刻。低く打つ変種 `Dip` は、元の動きから `dipped` で作る（構えはそのまま、スイートスポットを下げて腰を落とす） |
| `tennis_blender/racket_shape.py` | ラケットの形：ヘッド（打球面の楕円の縁から外へ）・スロート・グリップ・シャフト・ストリング、ソケットの位置 |
| `tennis_blender/expected.py` | 検査の期待値（dims から直接求める。生成の処理を通さない）と比較の関数 |
| `tennis_blender/materials.py` | マテリアルのスロットの名前と確認用の色（bpy に依存しない） |
| `tennis_blender/bpy_players.py` | 仮の選手の組み立て（bpy）：元の体の FBX を読み、ボクセルで 1 つの面に作り直してウェイトを写し（手は元の形）、服の殻（シャツ・ショートパンツ・靴・リストバンド）と襟のリブ、頭と首・髪・顔の部品、スカートを足して 1 つのメッシュにし、三角形を減らす。形と分け方は純粋なモジュールが決める |
| `tennis_blender/bpy_wear.py` | MetaHuman の選手のテニスウェアの組み立て（bpy）：組み立てた体の FBX を読み、UV の継ぎ目で分かれた頂点をつなぎ（分かれたままだと殻が継ぎ目で割れる）、服の殻（シャツ・ショートパンツ・靴下・リストバンド）・襟のリブ・スカート・靴（足の凸包を均等な面に作り直したもの）を 1 つのメッシュにする |
| `tennis_blender/bpy_strokes.py` | 仮のストロークの動きを骨に写す（bpy）：フレームごとに、腰 → 背骨 → 首 → 脚（足は地面に固定）→ 腕の順に骨の向きを決めて全フレームにキーを打ち、アクションを FBX に書き出す。姿勢は `stroke_motion` が決める |
| `tennis_blender/bpy_utils.py` | bpy の共通処理（コレクションの作り直し、確認用の色つきのマテリアル、頂点と面・bmesh からのメッシュの作成、スロットつきの部品からのメッシュ（箱の投影の UV・角度で硬くする辺）、ソケットの Empty、トランスフォームの判定、平面・箱の投影の UV、ログ） |
| `tests/` | 共通モジュールの単体テスト（標準の `unittest`。bpy は不要） |

`tennis_blender/` の中で bpy に依存してよいのは `bpy_utils.py`・`bpy_players.py`（仮の選手の組み立て）・`bpy_strokes.py`（仮のストロークを骨に焼く）だけです。ほかは純粋な Python にして、単体テストで確かめます。

## 2. Blender MCP からの実行

スクリプトの本文を `execute_blender_code` に貼らず、ファイルを `runpy` で実行します（リポジトリのファイルを正とするため）。`<リポジトリ>` は、リポジトリ（または worktree）の絶対パスに置き換えます。

```python
import sys, runpy
root = r"<リポジトリ>\Tools\Blender"
sys.path[:0] = [root]
for name in [m for m in sys.modules if m.split(".")[0] == "tennis_blender"]:
    del sys.modules[name]          # 編集した共通モジュールを読み直す
runpy.run_path(root + r"\build_court.py", run_name="__main__")
```

- 作業用のファイルで実行します。大事な `.blend` は先に保存しておきます（`execute_blender_code` は任意のコードを実行できるため）
- 実行したら `look`（真上と斜め）で形を、`get_scene_info`（`size`・`details`・`topology`）で寸法と面を確かめます
- 続けて同じ形で `validate_dimensions.py` を実行し、最後の行が `validate_dimensions：OK` であることを確かめます

## 3. 規約

| 項目 | 内容 |
|---|---|
| 単位 | Metric、Unit Scale 1.0（1 BU = 1 m） |
| 座標 | コート局所座標：原点＝ネット中央の地面、+X＝コートの長さ方向（UE の前方）、+Z＝上。ラケットだけは自分の局所座標：+X＝グリップ → 先端、Y＝フェイスの幅、Z＝フェイスの法線（XY・XZ の面について対称に作るので、FBX で Y が反転しても形は変わらない）。コートサイドの小物（`SM_Prop_*`）も自分の局所座標：+X＝正面（審判台は主審の向く方、球出し機は球の出る方、掲示板は画面の向く方）。XZ の面について対称に作る |
| 原点 | コート・ネット・練習コートの小物＝コートの原点（UE では同じ位置に置くだけでそろう）。ボール＝球の中心。ラケット＝グリップエンド（バットキャップの端面の中心。理由は [ART-03 §3](../../Docs/07_Art_Animation/03_Pipeline_Blender_UE.md)）。握る位置はソケット `SOCKET_grip` で表す。コートサイドの小物＝接地面の中心（UE では位置と向きをアクタで決める）。球出し機の球の出る点はソケット `SOCKET_muzzle` で表す |
| 命名 | `SM_`（Static Mesh）、`UCX_SM_xxx_00`（凸包のコリジョン）、`SOCKET_xxx`（ソケットの Empty）。メッシュのデータもオブジェクトと同じ名前にする |
| マテリアル | スロットの名前と確認用の色は `tennis_blender/materials.py` の 1 か所で決める（テストマップでラインやネットを見分けるため）。本番のシェーダーは UE で作る |
| UV・頂点カラー | UV ch0：平面のものはその面の座標（m）をそのまま入れる（UE のマテリアルの係数でタイリング）。ラケット・練習コートの小物は面ごとに、法線にいちばん近い軸を除いた 2 軸の座標（m。箱の投影）。ネットの頂点カラー `SwayWeight` はリニアの値（FBX は `colors_type='LINEAR'` で書き出す） |
| 何度実行しても同じ | 入口のスクリプトは、自分のコレクション（`Tennis_*`）の中身を消してから作り直す。ほかのコレクションには触れない |
| トランスフォーム | 頂点を直接その位置に作り、オブジェクトの Rotation・Scale は単位のままにする（エクスポートの前の Apply が要らない）。ソケットの Empty は親の座標での位置だけを持つ（回転・拡大は単位） |

## 4. 単体テスト

```powershell
py -I -m unittest discover -s Tools/Blender/tests -p "test_*.py" -v
```

CI も同じ形で `Tools/*/tests` を実行します。

## 5. FBX の書き出しと、UE での軸の確認

### 5.1 書き出し

1. `build_court.py`・`build_net.py`・`build_ball.py`・`build_racket.py`・`build_practice_court.py`・`build_court_props.py`・`build_stadium_a.py`・`build_axis_test.py` を実行し、`validate_dimensions.py` が OK であることを確かめる
2. `export_all.py` を実行する。書き出し先は既定でリポジトリの `Export/`（Git の対象外。スクリプトから再生成できる）。worktree から実行するときは、先に `os.environ["TENNIS_EXPORT_DIR"] = r"<本体のリポジトリ>\Export"` を設定する
3. 最後の行が `export_all：OK` であることを確かめる。トランスフォームが単位でないもの・隠れているものは書き出さずに `[NG]` を出す

| 設定 | 値 | 理由 |
|---|---|---|
| 単位 | `apply_unit_scale=True`、`apply_scale_options='FBX_SCALE_UNITS'` | 1 m が UE で 100 cm になる |
| 軸 | `axis_forward='X'`、`axis_up='Z'` | UE の +X 前・+Z 上に合わせる。UE で確認済み（§5.2。X と Z はそのまま、Y だけ反転する） |
| 頂点カラー | `colors_type='LINEAR'` | ファイルの値＝揺れの重み（0.5 → 128）。sRGB に変換しない |
| 面 | `mesh_smooth_type='FACE'` | スムーズの情報を面ごとに書き出す |
| 対象 | `object_types={'MESH', 'EMPTY'}` と子 | `UCX_`・`SOCKET_` を一緒に書き出す |
| 選手（`SKM_`）だけ | `object_types={'ARMATURE', 'MESH'}`、`apply_scale_options='FBX_SCALE_NONE'`、`primary_bone_axis='Y'`・`secondary_bone_axis='X'`、`armature_nodetype='NULL'`、`use_armature_deform_only=False` | UE から読んだ骨格（拡大 0.01）を cm に戻し、骨の向きを読み込みの逆の変換で元に戻す。骨格のオブジェクトは UE の根の骨 `root` として取り込まれる。UE → Blender → UE の往復で、骨 89 本の名前と順、参照姿勢（差 0.0002 cm・0.0004°）、境界、頂点の数が変わらないことを確かめた（2026-11-12） |

### 5.2 UE での軸の確認

FBX の軸は、矢印のメッシュ `SM_AxisTest`（`build_axis_test.py`。+X は矢じりつきの 1.0 m、+Y は 0.4 m の棒、+Z は 0.2 m の柱）で確かめます。

- 結果（2026-11-04。[ART-03 §3](../../Docs/07_Art_Animation/03_Pipeline_Blender_UE.md)）：**X と Z はそのまま、Y だけ反転する**（Blender の +Y → UE の −Y）。コート・ネット・ボール・ラケット・小物は Y について対称に作るので影響しない
- 取り込んだ `SM_AxisTest` は `Content/Tennis/Art/Dev/` にコミットしてある。自動テスト `Tennis.Game.Data.ArtAssets.AxisTest_KeepsTheFbxAxisConvention` が、境界（X は +100 cm まで、Y は −40 cm まで、Z は +20 cm まで）で向きを確かめる
- `export_all.py` の軸の設定（`axis_forward`・`axis_up`）や取り込みのツールを変えたら、`build_axis_test.py` → `export_all.py` → UE の `TennisTools.import_exported_fbx(["SM_AxisTest.fbx"])` で取り込み直し、このテストを実行する。向きが変わっていればテストが失敗する

### 5.3 仮の選手の作り直し（`build_players.py`）

1. UE（Unreal MCP）で `TennisTools.export_player_base_fbx` を呼ぶ。`Export/Base/` に Manny・Quinn の FBX ができる（入力。Git の対象外）
2. Blender で `build_players.py` を実行する（約 20 秒）。最後の 2 行が `[OK] SKM_Player_*：三角形 …（上限 60,000）` であることを確かめる
3. **骨格（`*_Rig`）を表示したまま** `export_all.py` を実行する（隠れた骨格は選べず、骨が書き出されない。`export_all.py` は止める）
4. UE で `TennisTools.import_exported_fbx(["SKM_Player_Male01.fbx", "SKM_Player_Female01.fbx"])` を呼ぶ（既存の骨格 `SK_TennisPlayer_Base` に取り込み、LOD を 4 つ作る）
5. 自動テスト `Tennis.Game.Data.ArtAssets` で、骨格・骨の名前と参照姿勢（元の体と同じ）・LOD・三角形の数を確かめる

### 5.4 仮のストロークのアニメーションの作り直し（`build_stroke_anims.py`）

1. `Export/Base/SKM_Manny_Simple.fbx` があることを確かめる（なければ UE の `TennisTools.export_player_base_fbx`）
2. Blender で `build_stroke_anims.py` を実行する。ログに、各ストロークのインパクトのスイートスポットの位置（選手の座標、m）と `[OK] AS_* → …` が出る
3. `look` の `frames`（例：0・14・19・23・31・38 フレーム）で動きを確かめる。メッシュ `StrokeAnim_Rig_Body` は確認用で、書き出さない
4. 動きを変えるときは、`tennis_blender/stroke_motion.py` のキーを変えて実行し直す（骨を手で動かさない）

| 設定 | 値 | 理由 |
|---|---|---|
| 対象 | `object_types={'ARMATURE'}`、`bake_anim=True`、`bake_anim_use_all_bones=True`、`bake_anim_simplify_factor=0` | 骨格とアニメーションだけを、全部の骨・全フレームのキーで書き出す（UE の圧縮に任せる） |
| 骨の軸・拡大 | 選手のメッシュ（§5.1 の `SKM_`）と同じ | UE の骨格 `SK_TennisPlayer_Base` と同じ骨の向きにする |
| フレーム | 30 fps、0 フレームから | ART-02 §1 |

## センターコート A の試合用セット

空の作業シーンで §2 の手順を使い、`build_match_court.py` を実行する。規格ラインを埋め込んだ試合床、座席、選手用の椅子、飲料テーブル、放送カメラを作り、寸法の照合後に各 FBX と組み立てシーン `Export/CenterCourtA/CenterCourtA.blend` を書き出す。Blender は開いているファイルを保存せず、組み立てシーンだけを別ファイルに書く。FBX はアセットごとの原点、組み立てシーンは配置後の座標を持つ。俯瞰・コート・備品のカメラを切り替えて確認できる。

UE では `SM_Court_A_MatchDeck` を規格の床とラインの代わりに置き、客席は従来の `SM_Stadium_A_Stands` に重ねる。デッキには選手用コリジョンを保持し、客席と備品には自動コリジョンを付けない。取り込みは `TennisTools.import_exported_fbx` を使い、客席と `SM_Prop_A_` の備品は本体 FBX の後に `_LOD1`、`_LOD2` を渡すと同じメッシュに統合される。
## B〜E・練習場の試合床と観客席

Blender のバックグラウンドで `blender --background --factory-startup --python-exit-code 2 --python Tools/Blender/build_production_venue.py -- B Export --render` を実行する。会場コードは B、C、D、E、Practice。各 FBX は Export 直下、編集可能な組み立てシーン・レンダー・三角形数は `Export/ProductionVenues/<会場>` に書き出す。UE では規格線込みの MatchDeck、Bowl、Seats、Architecture をコート原点に重ね、ネットと備品は共通メッシュを置く。客席と躯体の LOD1 を本体の後に取り込む。練習場は専用 MatchDeck と既存フェンス・ベンチ・照明を使う。
