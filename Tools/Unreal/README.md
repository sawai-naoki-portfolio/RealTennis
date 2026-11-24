# Tools/Unreal — プロジェクト専用の Unreal MCP ツールセット

Unreal MCP の標準のツールセットにない作業を、エディタの Python のツールセットとして追加します（[ARCH-04 §4](../../Docs/02_Architecture/04_Unreal_MCP_Workflow.md)）。

| ファイル | 内容 |
|---|---|
| `tennis_toolset/tools.py` | MCP に公開するツール（`TennisTools`）。`unreal` に依存する |
| `tennis_toolset/fbx_import.py` | FBX の取り込みの処理（`unreal` に依存する。Interchange の既定の動きを規則どおりに直す） |
| `tennis_toolset/import_rules.py` | 取り込みの規則（取り込み先・Nanite・仮のアセットの判定、選手の骨格・物理アセット・LOD の数。純粋な関数で、`unreal` に依存しない） |
| `tennis_toolset/fbx_export.py` | スケルタルメッシュの FBX の書き出し（`unreal` に依存する） |
| `tennis_toolset/stroke_assets.py` | ストロークのデータと Chooser の表の場所、仮のストロークの名前、Notify の時刻のファイル（`AS_*.json`）の検証（純粋な関数で、`unreal` に依存しない） |
| `tennis_toolset/stroke_build.py` | 仮のストロークのアセット（モンタージュ・`DA_Stroke_*`）を作って焼く処理と、表の作り直しと、選手の見た目のデータの焼き（`unreal` に依存する） |
| `tennis_toolset/metahuman_players.py` | MetaHuman の選手の値（Core Data のプリセット・体の測定値・髪）と場所、`build_metahuman_player` の入力の検証（純粋な関数で、`unreal` に依存しない） |
| `tennis_toolset/metahuman_build.py` | MetaHuman の選手を作る処理（`unreal` に依存する） |
| `tennis_toolset/player_base.py` | 服を作る元の体（仮の選手の Manny・Quinn、MetaHuman の選手の組み立てた体）と書き出し先 `Export/Base/`（純粋なデータ） |
| `tennis_toolset/court_level_layout.py` | テストマップの配置（純粋なデータ） |
| `tennis_toolset/shoot_rules.py` | `shoot_ball` の入力の検証（球種のアセット名・地点の範囲。純粋な関数で、`unreal` に依存しない） |
| `tennis_toolset/__init__.py` | `register()`：ツールセットのレジストリに登録する |
| `tests/` | 取り込みの規則・配置・入力の検証の単体テスト（CI で実行） |
| `Content/Python/init_unreal.py`（公開版には未収録） | エディタの起動時に Unreal が実行する。このフォルダをパスに加えて `register()` を呼ぶだけ |

## ツール

| ツール | 内容 |
|---|---|
| `import_exported_fbx(file_names)` | `Tools/Blender/export_all.py` が `Export/` に書き出した FBX を取り込む（同じ名前なら置き換えて保存）。**受け付けるのは `Export/` の直下のファイル名だけ**で、取り込み先と Nanite の設定はアセット名の接頭辞で決まる（`import_rules.DESTINATIONS`。`SM_Court_`→Court、`SM_Net`・`SM_SinglesStick`→Net、`SM_Ball`→Ball、`SM_Racket`→Racket、`SM_Practice_`・`SM_Stadium_`→Stadium、`SM_Prop_`→Props、`SM_AxisTest`→Dev。どれも Nanite なし。ラケット・小物（練習コート・コートサイド）・スタジアムはコリジョンなし）。**スタジアム（`SM_Stadium_`）の `_LOD<n>` のファイル**は、取り込んだ後に元のメッシュ（同じ呼び出しか、既にあるもの）の LOD n に入れて、LOD 用のアセットを消す（`import_rules.LOD_MERGE_PREFIXES`。ボールの `SM_Ball_LOD1` は別のアセットのまま）。`Export/` から作られた仮のマテリアルは、FBX の値（Blender の確認用の色）に合わせ直す。ソケットは位置だけを FBX から取り、回転 0・スケール 1 にする。**選手（`SKM_Player_`）**は `Characters/Players/Interim` へ、既存の骨格 `SK_TennisPlayer_Base`・物理アセット `PA_Mannequin` で取り込み（新しく作らない。骨格の参照姿勢を変えない）、LOD を 4 つ作る（`import_rules.SKELETAL_DESTINATIONS`）。指定の骨格に入らなければエラーで止まる。**MetaHuman の選手のテニスウェア（`SKM_Wear_`）**は `Characters/Players/MetaHuman/Wear` へ、組み立てた体の共通の骨格 `metahuman_base_skel` で取り込み（物理アセットは作らず付けない：体に追従する部品）、LOD を体と同じ 3 つ作る。**ストロークのアニメーション（`AS_FH_`・`AS_BH1_`）**は `Characters/Animations/Strokes` へ、既存の骨格 `SK_TennisPlayer_Base` にアニメーションだけを取り込む（メッシュ・骨格を作らない。`import_rules.ANIMATION_DESTINATIONS`） |
| `export_player_base_fbx()` | 服を作る元の体（`player_base.BASE_MESHES`）の LOD0 を、骨格・ウェイトごと `Export/Base/` に FBX で書き出す。Manny・Quinn（`SKM_Manny_Simple`・`SKM_Quinn_Simple`）は `Tools/Blender/build_players.py` が読み、同じ骨格と参照姿勢のまま仮の選手を作る（[ART-03 §4](../../Docs/07_Art_Animation/03_Pipeline_Blender_UE.md)）。MetaHuman の選手の組み立てた体（`SKM_MHC_Player_<key>_BodyMesh`）は `Tools/Blender/build_metahuman_wear.py` が読み、その殻としてテニスウェアを作る（ART-03 §6 の手順 5）。`Export/Base/` は `import_exported_fbx` の対象にならない |
| `rebuild_stroke_chooser()` | Chooser の表 `/Game/Tennis/Data/Strokes/CHT_StrokeSelect` を、同じフォルダの `DA_Stroke_*`（`UStrokeData`）の条件 `SelectRows` から作り直して保存する（なければ作る）。行は条件の多い順。表の中身を書くのは C++ の `UTennisStrokeChooserBuilder::Rebuild`（`TennisEditor`。Chooser の列は Python から作れない）。**表は手で編集しない**（ストロークのデータを直して、これを呼ぶ。[DD-03 §5.3](../../Docs/04_DetailedDesign/03_Player_Movement_Stroke_Detail.md)） |
| `build_locomotion_database()` | Motion Matching のスキーマ `/Game/Tennis/Characters/Animations/Locomotion/PSS_Locomotion` とデータベース `PSD_Locomotion` を作る・入れ直して保存する（DD-03 §6.5）。なければ C++ の `UTennisLocomotionDatabaseBuilder` で作る（スキーマは骨格 `SK_TennisPlayer_Base` と PoseSearch の既定のチャンネル：軌跡と両足。エディタの「新しいスキーマ」と同じ）。データベースのアニメーションを、`locomotion_assets.PLACEHOLDER_ANIMATIONS`（待機 `MM_Idle` と、`Mannequin/Walk`・`Mannequin/Jog` の 8 方向ずつ）に入れ直す（前のものは消す）。**引数はない**。アニメーションが 1 つでも足りない・目印 `TennisTools.Owner` のない既存のアセット（本番のもの）があるときは、何も変えずに止まる。PIE の間は使えない。入れた数を返す |
| `build_placeholder_strokes()` | 仮のストローク（`stroke_assets.PLACEHOLDER_STROKES`：フォアと片手のバックと、その低く打つ変種 `Dip`）を作る・作り直す。`Tools/Blender/build_stroke_anims.py` が書き出した `Export/AS_*.fbx` を取り込み、モンタージュ `AM_*` を同じフォルダに作って、`Export/AS_*.json` の時刻に Notify「Tennis Impact」「Tennis Recover Start」を入れる（ほかの Notify は消す）。`/Game/Tennis/Data/Strokes/DA_Stroke_*` に、モンタージュと条件（向きと、打点の高さの範囲。範囲は `Dip` と上の行で境界を共有する片側だけ：DD-03 §5.3。ほかは Any）と離す時刻の先行時間（`ReleaseLeadTime`）と溜めの時間（`ChargeTime`）（どちらも操作仕様 §4.1 の表のフォアハンド（ドライブ）と片手バックの値。`stroke_assets.PLACEHOLDER_STROKES`）を入れ、`StrokeMetaBaker` で時刻を焼いて（体によらない値。DD-03 §5.4）保存し、表を作り直して、すべての選手の見た目のデータを焼く（`bake_player_appearances`）。**引数はなく、ファイルは名前の一覧のものだけを読む**。ファイルが 1 つでも足りない・JSON が決まり（4 KB まで、名前・30 fps・`0 < インパクト < リカバー ≤ 長さ`）に合わない・**このツールが作ったのでない既存のアセット**（`Export/` から取り込んでいない `AS_*`、目印のメタデータ `TennisTools.Owner` のない `AM_*`・`DA_Stroke_*`：本番のもの）があるときは、何も変えずに止まる。取り込んだ後の検査（骨格・長さ・焼く）で止まったときは、それまでに作ったものが残る。既存のアセットは消さずに更新する（同じ名前を作り直すと、上書きの確認の画面が出て MCP が止まる）。モンタージュの長さが取り込み直したアニメーションと合わなくなったら、その `AM_*` を消してから実行し直す。PIE の間は使えない |
| `build_metahuman_player(player_key)` | MetaHuman の選手（`metahuman_players.PLAYERS`：`M01` は Kelvin、`F01` は Celeste）を作る・続きを進め、今の段階を返す。**受け付けるのは一覧のキーだけ**。Core Data のプリセットを写して `MetaHuman/MHC_Player_<key>` を作り（体の測定値を当てて確定、髪を替え、服を外す）、1 回に 1 段階ずつ進める：顔の自動リグ（クラウド。関節だけ。終わるまで数分待ち、その間エディタは止まる）で `rigged`、肌のテクスチャの元を取り寄せて `textures`、Optimized・Medium で組み立て（共通のアセットは `MetaHuman/Common` の 1 つ）、体のメッシュにラケットのソケット（`SK_TennisPlayer_Base` と同じ位置と向き）を、共通の骨格に互換の骨格を足し、`DA_Appearance_Player_<key>`（体・顔・髪の Binding。服 `MetaHuman/Wear/SKM_Wear_<key>` を取り込んであれば、追従する部品に服も）を作り、すべての見た目のデータを焼いて（`bake_player_appearances` と同じ）`done`。**`done` になるまで呼び直す**。Epic のサインインと MetaHuman の利用規約の同意は利用者がする。目印 `TennisTools.Owner` のない既存の `MHC_Player_<key>`・`DA_Appearance_Player_<key>` は作り替えない（焼く段階では、すべての見た目のデータに焼いた値を書く）。組み立てた体と骨格が決まったフォルダ（その選手のフォルダ・`MetaHuman/Common`）の外なら、書き換えずに止まる。PIE の間は使えない（[ART-03 §6](../../Docs/07_Art_Animation/03_Pipeline_Blender_UE.md)） |
| `apply_metahuman_wear(player_key)` | `import_exported_fbx` で取り込んだテニスウェア `MetaHuman/Wear/SKM_Wear_<key>` を、`DA_Appearance_Player_<key>` の追従する部品（顔と服）に入れて保存する（組み立てはしない。`build_metahuman_player` も、服があれば入れる）。**受け付けるのは一覧のキーだけ**。服が取り込まれていない、服の骨格が共通の `metahuman_base_skel` でない、見た目のデータがこのツールで作ったものでない（目印 `TennisTools.Owner`）ときは止まる。PIE の間は使えない（[ART-03 §6](../../Docs/07_Art_Animation/03_Pipeline_Blender_UE.md) の手順 5・6） |
| `bake_player_appearances()` | `/Game/Tennis/Characters/Players` のすべての選手の見た目のデータ（`UTennisPlayerAppearance`）に、その体で焼いた値を書いて保存する：すべての `DA_Stroke_*` のインパクトのスイートスポット（`StrokeFits`）と、表が中くらいの高さのドライブで選ぶストロークから作る探索の体の位置（`ReachBody`）。書くのは C++ の `UTennisStrokeMetaBaker::BakeAppearance`（ラケットは Project Settings の Tennis Player の値。[DD-03 §5.4・§7](../../Docs/04_DetailedDesign/03_Player_Movement_Stroke_Detail.md)）。**ストロークのモンタージュ・選手の体・ラケットを変えたら呼ぶ**（焼いた値が古いと T-STRK-010・011 が落ちる）。引数はない。焼けない見た目のデータがあれば、そこで止まる（その前のものは保存済み）。PIE の間は使えない |
| `build_test_court_level()` | テストマップ `/Game/Tennis/Maps/L_TestCourt` を作る・作り直す（コート・ネットと、練習コートの小物＝フェンス・ベンチ・照明柱のメッシュを原点に置き、`ATennisCourt` を原点に、`ATennisBall` を相手の側（+X）のベースラインの後ろに置いて、可動の照明を足して保存。ライティングのビルドは不要）。ボールは打たれるまで置いた位置で待つ。打球ツール `ATennisShotTester`（`DA_Shot_*` をすべて、球種の順に渡す。打点は相手の側）と、選手の出現位置の `PlayerStart`（−X の側のベースラインの後ろ）も置く。PIE の視点は選手のカメラ（中継カメラ：DD-07 §1.1）が決めるので、カメラは置かない。作り直すときは、このツールが置いたアクタ（アウトライナーの `TennisTestCourt`）だけを消す |
| `shoot_ball(shot_type, target_x_m, target_y_m)` | PIE・Simulate の間に、打球ツールで球種（例：`DA_Shot_Topspin`）を指定して、コート局所の地点（m）へ打つ。打球ツールは相手の側（+X）から打つので、X が負の地点を狙うと選手に向かって球が来る。解けたら `true`（解けなくても打つ）。結果は `LogTennisShot` の `[Tester]` と画面に出る。入力の範囲は `shoot_rules.py` |
| `reload_tennis_toolset()` | このパッケージを読み込み直して登録し直す。**ツールを変えたら、エディタを再起動せずにこれを呼ぶ** |

MCP からの呼び出しの例：

```
call_tool(toolset_name="Game.Tools.Unreal.tennis_toolset.tools.TennisTools", tool_name="import_exported_fbx",
          arguments={"file_names": ["SM_Court_Surface.fbx", "SM_Court_Lines.fbx"]})
```

標準の `StaticMeshTools.import_file` は新規の取り込みだけで（同じ名前のアセットがあると失敗し、保存もしない）、取り込み先の規則もない。Blender で直して取り込み直す繰り返しの作業のために、このツールを置く。

### Interchange（UE 5.8）の既定の動きと、このツールでの直し方

| 既定の動き | 困ること | このツール |
|---|---|---|
| スタティックメッシュを **Nanite を有効にして**取り込む | コート・ネット・ボールは Nanite を使わない（ART-01 §1、NFR-MOB-002）。SM6 がないと正しく表示されない | 取り込んだ後、規則どおりに Nanite を切り替えて保存する |
| 既存のアセットへ取り込み直すと、**マテリアルに触れない**（再取り込みの方針の既定が `APPLY_NO_PROPERTIES`。方針を変えても、メッシュの再取り込みではマテリアルを更新しない。マテリアルだけを再取り込みしても変わらない） | Blender で変えた確認用の色が反映されない | 一時フォルダー（`/Game/Tennis/_ImportTemp`）へ新しく取り込み、そのパラメーターの値を、`Export/` から作られた既存のマテリアルに写す。一時フォルダーは最後に必ず消す（消せなければエラーのログ） |
| `UCX_` のコリジョンがないメッシュに、**全体を覆う凸包（18-DOP）を自動で付ける**（Fallback Collision Type の既定） | フェンスの凸包は中のコート全体を覆い、その中に出現した選手が地面をすり抜けて落ちる（2026-11-11、PIE で選手の Z が −292 m）。ラケットと練習コートの小物はコリジョンなしの設計（[ART-03 §5](../../Docs/07_Art_Animation/03_Pipeline_Blender_UE.md)） | 規則（`import_rules.DESTINATIONS` の「コリジョンを残すか」）が False のアセットから、取り込んだ後に単純コリジョンを消して保存する。凸包は `get_convex_collision_count` で数える（`get_simple_collision_count` は箱・球・カプセルだけ）。結果は自動テスト `Tennis.Game.Data.ArtAssets` で確かめる |
| スケルタルメッシュを取り込むと、**新しい骨格と物理アセットを作る**。`AssetImportTask.options` に Interchange のパイプライン（`InterchangeGenericAssetsPipeline`）を渡しても使われない | 仮の選手が別の骨格になり、今のアニメーションとラケットのソケットが使えない | 選手だけは `InterchangeManager.import_asset` に、骨格・物理アセットを指定したパイプラインを `override_pipelines` で渡す。既存のアセットへの取り込み直しは、そのアセットの骨格のままになるので、骨格を間違えたものは消してから取り込む |
| Blender の FBX には骨格のバインドポーズがなく、「無効なバインド ポーズ…時刻 0 のポーズで再バインド」の警告が出る | なし（時刻 0 のポーズ＝Blender のレスト＝元の参照姿勢。往復で骨の差は 0.0002 cm・0.0004°） | 何もしない（警告は残る） |
| スタティックメッシュのソケット（`SOCKET_` の Empty）に、**FBX の軸と単位の変換の逆を掛ける**（`InterchangeMeshHelper.cpp` の「Apply axis transformation inverse」） | `export_all.py` の書き出し（X 前・Z 上・`FBX_SCALE_UNITS`）では、位置は正しいが、回転 90°（pitch）・スケール 100 が残る。付けたものが 100 倍になり、向きも設計（メッシュの軸のまま：[ART-03 §3](../../Docs/07_Art_Animation/03_Pipeline_Blender_UE.md)）と合わない | 取り込んだ後、ソケットの回転を 0・スケールを 1 にして保存する（位置は FBX のまま）。Blender の側の Empty の回転が単位であることは `validate_dimensions.py` が確かめている。結果は自動テスト `Tennis.Game.Data.ArtAssets` で確かめる |

UE で作った本番のマテリアル（取り込み元が `Export/` でないもの）には触れない。

## 追加の規則

- 標準のツールセットでできることは、ここに作らない（`describe_toolset` で先に確かめる）
- ツールは今のマイルストーンで必要になったものだけを追加する（YAGNI）
- 入力の検証は純粋な関数に分け、`tests/` に単体テストを書く
- 引数と戻り値の型の注釈は、登録の仕組みが実行時に読む。`from __future__ import annotations` を `tools.py` で使わない
- 変更したツールは `reload_tennis_toolset()` で読み込み直す（新しいファイルを足したときも同じ）。`init_unreal.py` を変えたときだけ再起動する
