"""ロコモーションの Motion Matching のアセットを作る処理（unreal に依存する。名前・場所・アニメーションの一覧は locomotion_assets）。

スキーマ PSS_Locomotion（PoseSearch の既定のチャンネル）とデータベース PSD_Locomotion を、なければ C++ の
UTennisLocomotionDatabaseBuilder で作り（目印 OWNER_TAG）、データベースのアニメーションを仮のデータの一覧に入れ直して保存する
（DD-03 §6.5）。目印のない既存のアセット（本番のもの）があれば、何も変えずに止める。
"""
import unreal

from . import locomotion_assets as la


def build_database() -> int:
    """PSS_Locomotion と PSD_Locomotion を作る・入れ直して保存する。データベースに入れたアニメーションの数を返す。"""
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        raise RuntimeError("Stop PIE or Simulate first (assets are not edited during play)")
    library = unreal.EditorAssetLibrary
    for path in (la.SCHEMA_PATH, la.DATABASE_PATH):
        if library.does_asset_exist(path) and library.get_metadata_tag(unreal.load_asset(path), la.OWNER_TAG) != la.OWNER_VALUE:
            raise RuntimeError(f"Not made by this tool (no {la.OWNER_TAG}): {path}")
    skeleton = unreal.load_asset(la.SKELETON_PATH)
    if not isinstance(skeleton, unreal.Skeleton):
        raise FileNotFoundError(f"Skeleton not found: {la.SKELETON_PATH}")
    sequences = [unreal.load_asset(path) for path in la.animation_paths()]
    missing = [path for path, sequence in zip(la.animation_paths(), sequences) if not isinstance(sequence, unreal.AnimSequence)]
    if missing:
        raise FileNotFoundError(f"Locomotion animations not found: {missing}")
    # 何かを変える前に、骨格の違うアニメーションで止める（途中まで入れたデータベースを残さない）
    other = [path for path, sequence in zip(la.animation_paths(), sequences) if sequence.get_editor_property("skeleton") != skeleton]
    if other:
        raise RuntimeError(f"Not on {la.SKELETON_PATH}: {other}")

    builder = unreal.TennisLocomotionDatabaseBuilder
    schema = _load_or_create(la.SCHEMA_PATH, unreal.PoseSearchSchema,
                             lambda: builder.create_schema(la.SCHEMA_PATH, skeleton))
    database = _load_or_create(la.DATABASE_PATH, unreal.PoseSearchDatabase, lambda: builder.create_database(la.DATABASE_PATH, schema))
    added = builder.set_animations(database, sequences)
    if added != len(sequences):
        raise RuntimeError(f"{la.DATABASE_NAME}: {added} of {len(sequences)} animations were added (see LogTennisMove [LocomotionBuilder])")
    for asset in (schema, database):
        if not library.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {asset.get_path_name()}")
    unreal.log(f"[TennisTools] built {la.DATABASE_PATH}: {added} animation(s), schema {la.SCHEMA_PATH}")
    return added


def _load_or_create(path: str, asset_class: type, create) -> object:
    """あれば読み、なければ create() で作って目印を付ける（消して作り直さない。目印は作ったときだけ：stroke_build と同じ）。"""
    library = unreal.EditorAssetLibrary
    exists = library.does_asset_exist(path)
    asset = unreal.load_asset(path) if exists else create()
    if not isinstance(asset, asset_class):
        raise RuntimeError(f"Could not load or create {path} (see LogTennisMove [LocomotionBuilder])")
    if not exists:
        library.set_metadata_tag(asset, la.OWNER_TAG, la.OWNER_VALUE)
    return asset
