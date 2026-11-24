"""ストロークのアセットを作る処理（unreal に依存する。名前・場所・Notify の時刻の検証は stroke_assets）。

仮のストローク（PLACEHOLDER_STROKES）は、AS_*（取り込んだアニメーション）→ AM_*（モンタージュ。Notify は AS_*.json の
時刻）→ DA_Stroke_*（UStrokeData。条件は向きと、打点の高さの範囲）の順に作り、StrokeMetaBaker で時刻を焼く（体ごとのスイートスポットは、
見た目のデータに bake_appearances で焼く）。どれもこのツールが持つアセットで、
手で編集しない（作り直すと、モンタージュの Notify と DA_Stroke_* の条件は上書きされる）。
持ち主でない既存のアセット（Export/ から取り込んでいない AS_*、目印 OWNER_TAG のない AM_*・DA_Stroke_*。本番のもの）が
1 つでもあれば、何も変えずに止める。
"""
import os

import unreal

from . import fbx_import
from . import stroke_assets as sa
from .import_rules import ImportPlan, plan_import

_PLAYER_SETTINGS = "/Script/Tennis.Default__TennisPlayerSettings"
PLAYERS_FOLDER = "/Game/Tennis/Characters/Players"  # 選手の見た目のデータ（DA_Appearance_*）を探す場所


def build_placeholders(export_dir: str) -> list[str]:
    """仮のストロークを作る・作り直して保存する。作った DA_Stroke_* のオブジェクトパスを返す。"""
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        raise RuntimeError("Stop PIE or Simulate first (assets are not edited during play)")
    plans: list[ImportPlan] = []
    times: list[sa.NotifyTimes] = []
    for stroke in sa.PLACEHOLDER_STROKES:  # 1 つでも足りない・決まりに合わなければ何もしない
        plan = plan_import(stroke.animation + ".fbx", export_dir)
        notify_file = os.path.join(export_dir, stroke.animation + ".json")
        if not os.path.isfile(plan.source_file) or not os.path.isfile(notify_file):
            raise FileNotFoundError(f"Run Tools/Blender/build_stroke_anims.py first: {plan.source_file}, {notify_file}")
        plans.append(plan)
        times.append(sa.read_notify_times(notify_file, stroke.animation))
    _check_owner(plans, export_dir)

    fbx_import.import_plans(plans, export_dir)
    built = []
    for stroke, plan, notify_times in zip(sa.PLACEHOLDER_STROKES, plans, times):
        sequence = unreal.load_asset(f"{plan.destination_path}/{stroke.animation}.{stroke.animation}")
        if not isinstance(sequence, unreal.AnimSequence):
            raise RuntimeError(f"Not an animation: {plan.destination_path}/{stroke.animation}")
        if abs(sequence.get_play_length() - notify_times.length) > 0.5 / sa.ANIMATION_FPS:
            raise RuntimeError(f"{stroke.animation} is {sequence.get_play_length():.3f} s, the notify file says {notify_times.length:.3f} s")
        montage = _montage(plan.destination_path, stroke.montage, sequence)
        _set_notifies(montage, notify_times)
        data = _stroke_data(stroke, montage)
        if unreal.TennisStrokeMetaBaker.bake(data) is None:  # bool を返す UFUNCTION は、失敗で None（成功で OutError）
            raise RuntimeError(f"Could not bake {stroke.data} (see LogTennisStroke [StrokeMetaBaker])")
        for asset in (montage, data):
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError(f"Could not save {asset.get_path_name()}")
        unreal.log(f"[TennisTools] built {stroke.data}: impact {data.get_editor_property('impact_time'):.3f} s")
        built.append(data.get_path_name())
    return built


def rebuild_chooser() -> int:
    """Chooser の表 CHT_StrokeSelect を DA_Stroke_* から作り直して保存する（なければ作る）。行の数を返す。"""
    library = unreal.EditorAssetLibrary
    folder, table_path = sa.STROKE_FOLDER, sa.STROKE_CHOOSER_PATH
    strokes = _assets_of_class(folder, unreal.StrokeData)
    table = unreal.load_asset(table_path) if library.does_asset_exist(table_path) else None
    if table is None:
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            sa.STROKE_CHOOSER_NAME, folder, unreal.ChooserTable, unreal.ChooserTableFactory())
    if not isinstance(table, unreal.ChooserTable):
        raise RuntimeError(f"Not a chooser table: {table_path}")
    rows = unreal.TennisStrokeChooserBuilder.rebuild(table, strokes)
    if rows < 0 or not library.save_loaded_asset(table, only_if_is_dirty=False):
        raise RuntimeError(f"Could not rebuild or save {table_path}")
    unreal.log(f"[TennisTools] rebuilt {table_path}: {rows} row(s) from {len(strokes)} stroke(s)")
    return rows


def bake_appearances() -> list[str]:
    """PLAYERS_FOLDER のすべての見た目のデータに、その体のスイートスポット（DA_Stroke_* ごと）と探索の体の位置を焼いて保存する
    （UTennisStrokeMetaBaker::BakeAppearance。DD-03 §7）。焼いたもののオブジェクトパスを名前の順に返す。"""
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        raise RuntimeError("Stop PIE or Simulate first (assets are not edited during play)")
    library = unreal.EditorAssetLibrary
    strokes = _assets_of_class(sa.STROKE_FOLDER, unreal.StrokeData)
    table = unreal.load_asset(sa.STROKE_CHOOSER_PATH) if library.does_asset_exist(sa.STROKE_CHOOSER_PATH) else None
    appearances = _assets_of_class(PLAYERS_FOLDER, unreal.TennisPlayerAppearance)
    racket = _racket()
    if not strokes or not isinstance(table, unreal.ChooserTable) or not appearances:
        raise FileNotFoundError(f"Need DA_Stroke_* and {sa.STROKE_CHOOSER_PATH} (build_placeholder_strokes) and appearances in {PLAYERS_FOLDER}")
    baked = []
    for appearance in appearances:
        if unreal.TennisStrokeMetaBaker.bake_appearance(appearance, strokes, table, racket) is None:  # 失敗で None（上と同じ）
            raise RuntimeError(f"Could not bake {appearance.get_name()} (see LogTennisStroke [StrokeMetaBaker])")
        if not library.save_loaded_asset(appearance, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save {appearance.get_path_name()}")
        unreal.log(f"[TennisTools] baked {appearance.get_name()}: {len(strokes)} stroke(s)")
        baked.append(appearance.get_path_name())
    return baked


def _assets_of_class(folder: str, asset_class: type) -> list:
    """folder（とその下）の asset_class のアセットを、パスの順に読む。アセットレジストリで型を絞ってから読む
    （選手のフォルダには MetaHuman の重いメッシュ・テクスチャがあるので、全部は読まない）。"""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    class_name = asset_class.static_class().get_name()
    found = [data for data in registry.get_assets_by_path(folder, recursive=True)
             if str(data.asset_class_path.asset_name) == class_name]
    assets = [data.get_asset() for data in sorted(found, key=lambda data: str(data.package_name))]
    return [asset for asset in assets if isinstance(asset, asset_class)]


def _check_owner(plans: list[ImportPlan], export_dir: str) -> None:
    """このツールが作ったのでない既存のアセットがあれば止める（本番のアセットを上書きしない）。"""
    library = unreal.EditorAssetLibrary
    others = []
    for stroke, plan in zip(sa.PLACEHOLDER_STROKES, plans):
        animation = f"{plan.destination_path}/{stroke.animation}"
        if library.does_asset_exist(animation) and not fbx_import.is_export_asset(unreal.load_asset(animation), export_dir):
            others.append(animation)
        for path in (f"{plan.destination_path}/{stroke.montage}", f"{sa.STROKE_FOLDER}/{stroke.data}"):
            if library.does_asset_exist(path) and library.get_metadata_tag(unreal.load_asset(path), sa.OWNER_TAG) != sa.OWNER_VALUE:
                others.append(path)
    if others:
        raise RuntimeError(f"Not built by this tool (left untouched; rename or remove them by hand): {others}")


def _racket() -> unreal.StaticMesh:
    """焼くときのラケット：Project Settings の Tennis Player の値（ATennisCharacter が使うものと同じ）。"""
    settings = unreal.load_object(None, _PLAYER_SETTINGS)
    racket = settings.get_editor_property("RacketMesh") if settings is not None else None
    if not isinstance(racket, unreal.StaticMesh):
        raise FileNotFoundError(f"Set RacketMesh in Project Settings > Tennis Player: {racket}")
    return racket


def _load_or_create(folder: str, name: str, asset_class: type, factory: object) -> object:
    """あれば読み、なければ作って目印を付ける（消して作り直さない：同じ名前を作り直すと、上書きの確認の画面が出て止まる）。"""
    path = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.load_asset(path)
    else:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, asset_class, factory)
        if asset is not None:
            unreal.EditorAssetLibrary.set_metadata_tag(asset, sa.OWNER_TAG, sa.OWNER_VALUE)
    if not isinstance(asset, asset_class):
        raise RuntimeError(f"Not a {asset_class.__name__}: {path}")
    return asset


def _montage(folder: str, name: str, sequence: unreal.AnimSequence) -> unreal.AnimMontage:
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", sequence)
    factory.set_editor_property("target_skeleton", sequence.get_editor_property("skeleton"))
    montage = _load_or_create(folder, name, unreal.AnimMontage, factory)
    # 既存のモンタージュは、取り込み直したアニメーションをそのまま指すが、区間の長さは変わらない
    if abs(montage.get_play_length() - sequence.get_play_length()) > 1.0e-3:
        raise RuntimeError(f"{name} is {montage.get_play_length():.3f} s but {sequence.get_name()} is "
                           f"{sequence.get_play_length():.3f} s: delete {folder}/{name} and run again")
    return montage


def _set_notifies(montage: unreal.AnimMontage, times: sa.NotifyTimes) -> None:
    """Notify をすべて消し、インパクトとリカバーの目印だけを入れ直す（DD-03 §5.4 の StrokeMetaBaker が読む）。"""
    library = unreal.AnimationLibrary
    library.remove_all_animation_notify_tracks(montage)
    library.add_animation_notify_track(montage, sa.NOTIFY_TRACK)
    library.add_animation_notify_event(montage, sa.NOTIFY_TRACK, times.impact_time, unreal.AnimNotify_TennisImpact)
    library.add_animation_notify_event(montage, sa.NOTIFY_TRACK, times.recover_time, unreal.AnimNotify_TennisRecoverStart)


def _stroke_data(stroke: sa.PlaceholderStroke, montage: unreal.AnimMontage) -> unreal.StrokeData:
    data = _load_or_create(sa.STROKE_FOLDER, stroke.data, unreal.StrokeData, unreal.DataAssetFactory())
    row = unreal.StrokeSelectRow()
    row.set_editor_property("side", getattr(unreal.StrokeSide, stroke.side))
    row.set_editor_property("has_hit_height_min", stroke.hit_height_min is not None)
    row.set_editor_property("hit_height_min", stroke.hit_height_min if stroke.hit_height_min is not None else 0.0)
    row.set_editor_property("has_hit_height_max", stroke.hit_height_max is not None)
    row.set_editor_property("hit_height_max", stroke.hit_height_max if stroke.hit_height_max is not None else 0.0)
    data.set_editor_property("montage", montage)
    data.set_editor_property("select_rows", [row])
    data.set_editor_property("release_lead_time", stroke.release_lead_time)
    data.set_editor_property("charge_time", stroke.charge_time)
    return data
