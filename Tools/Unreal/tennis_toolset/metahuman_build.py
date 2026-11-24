"""MetaHuman の選手を作る処理（unreal に依存する。値・場所・入力の検証は metahuman_players）。ART-03 §6。

1 回の呼び出しで次の 1 段階を進めて、その段階を返す（クラウドの自動リグとテクスチャの取り寄せは数分かかり、その間エディタは
止まる。リグの状態を知る関数は Python に公開されていないので、待つ頼み方にして、終わったら目印 STAGE_TAG に書いて覚える）：
  1. なければ Core Data のプリセットを写して MHC_Player_* を作り（目印 OWNER_TAG）、体の測定値を当てて確定し、髪を替え、服を外す。
     顔の自動リグ（クラウド。関節だけ）を頼んで終わりまで待つ → "rigged"
  2. 肌のテクスチャの元を取り寄せて待つ → "textures"
  3. Optimized・Medium で組み立て（共通のアセットは COMMON_FOLDER）、体のメッシュにラケットのソケットを、共通の骨格に互換の骨格を
     同じ結果になるように足し、見た目のデータ（体・顔・髪の Binding。取り込んであれば服も）を作って、その体で焼く → "done"
組み立ての後に服（SKM_Wear_<key>）を取り込んだら、apply_wear で見た目のデータの追従する部品を顔と服にし直す（組み立てはしない）。
目印のない既存の MHC_Player_*・見た目のデータ（手で作ったもの）には触れない。サインインと利用規約の同意は利用者がする。
"""
import unreal

from . import metahuman_players as mp
from . import stroke_build

OWNER_TAG = "TennisTools.Owner"
OWNER_VALUE = "build_metahuman_player"
KNOWN_ASSEMBLY_WARNING = "Cannot break link"  # 組み立てが例外として返すが無害な警告（顔のコントロールリグ）
STAGE_TAG = "TennisTools.Stage"  # 自動リグが返ったら "rigged"（体・髪・服を変えて作り直すときは、MHC_Player_* を消してから呼ぶ）
RACKET_SOCKETS = ("racket_attach_r", "racket_attach_l")  # ART-01 §3.1。SK_TennisPlayer_Base と同じ位置と向き
BASE_SKELETON = "/Game/Tennis/Characters/Base/SK_TennisPlayer_Base"


def build_player(key: str) -> str:
    """選手 key（例："M01"）を作る・続きを進める。進めた段階（"rigged"・"textures"・"done"）を返す。"""
    player = mp.find_player(key)  # パスを組み立てる前に入力を確かめる
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        raise RuntimeError("Stop PIE or Simulate first (assets are not edited during play)")
    library = unreal.EditorAssetLibrary
    _check_owner(player)
    sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)

    created = not library.does_asset_exist(player.character_path)
    if created:
        if not library.does_asset_exist(player.preset_path):
            raise FileNotFoundError(f"Install MetaHuman Creator Core Data (no preset {player.preset_path})")
        character = library.duplicate_asset(player.preset_path, player.character_path)
        library.set_metadata_tag(character, OWNER_TAG, OWNER_VALUE)
    character = unreal.load_asset(player.character_path)
    if not isinstance(character, unreal.MetaHumanCharacter):
        raise RuntimeError(f"Not a MetaHuman Character: {player.character_path}")
    if not sub.is_object_added_for_editing(character) and not sub.try_add_object_to_edit(character):
        raise RuntimeError(f"Cannot edit {player.character} (close its editor window)")
    if created:
        _apply_body(sub, character, player)
        _apply_slots(character, player)
        library.save_loaded_asset(character, only_if_is_dirty=False)

    if library.get_metadata_tag(character, STAGE_TAG) != "rigged":
        request = unreal.MetaHumanCharacterAutoRiggingRequestParams()
        request.blocking = True  # 終わるまで待つ（数分）
        request.report_progress = True
        request.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        sub.request_auto_rigging(character, request)  # 失敗しても例外は出ない。そのときは組み立ての前の can_build_meta_human が止める
        library.set_metadata_tag(character, STAGE_TAG, "rigged")
        library.save_loaded_asset(character, only_if_is_dirty=False)
        unreal.log(f"[TennisTools] {player.character}: auto rig done (call again for the textures)")
        return "rigged"
    if not character.get_editor_property("has_high_resolution_textures"):
        request = unreal.MetaHumanCharacterTextureRequestParams()
        request.blocking = True
        request.report_progress = True
        sub.request_texture_sources(character, request)
        library.save_loaded_asset(character, only_if_is_dirty=False)
        unreal.log(f"[TennisTools] {player.character}: texture sources downloaded (call again to assemble)")
        return "textures"
    if not sub.can_build_meta_human(character, True):  # 理由は LogMetaHumanCharacterEditor に出る
        raise RuntimeError(f"{player.character} is not ready to assemble (see LogMetaHumanCharacterEditor; sign in to Epic for the cloud steps)")

    params = unreal.MetaHumanCharacterEditorBuildParameters()
    params.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    params.pipeline_quality = unreal.MetaHumanQualityLevel.MEDIUM
    params.absolute_build_path = mp.METAHUMAN_FOLDER
    params.common_folder_path = mp.COMMON_FOLDER
    params.enable_wardrobe_item_validation = False
    try:
        sub.build_meta_human(character=character, params=params)
    except RuntimeError as error:
        # 組み立ては、エンジンの顔のコントロールリグのリンクの警告（Cannot break link …）を Python の例外として返す（2026-11-18。
        # 組み立て自体は成功する）。その警告だけを受け流し、ほかの例外は止める（作り直しで前のメッシュが残っていても成功に見せない）
        lines = [line for line in str(error).splitlines() if line.strip()]
        if not lines or any(KNOWN_ASSEMBLY_WARNING not in line for line in lines):  # 全部の行がその警告のときだけ
            raise
        unreal.log_warning(f"[TennisTools] {player.character}: the assembly reported the known control rig warning: "
                           f"{str(error).splitlines()[0][:200]}")
    finally:
        if sub.is_object_added_for_editing(character):
            sub.remove_object_to_edit(character)
    library.save_loaded_asset(character, only_if_is_dirty=False)
    for folder in (player.build_folder, mp.COMMON_FOLDER):  # 組み立ては保存しない（2026-11-18 に確かめた）
        if not library.save_directory(folder, only_if_is_dirty=True, recursive=True):
            raise RuntimeError(f"Could not save the assembled assets in {folder}")

    body, face, bindings = _built_parts(player)
    skeleton = body.get_editor_property("skeleton")
    mp.check_built_paths(player, body.get_path_name(), skeleton.get_path_name() if skeleton else "")  # 決まったフォルダの外を書き換えない
    _add_sockets_and_compatibility(body)
    appearance = _appearance(player, body, face, bindings)
    stroke_build.bake_appearances()
    unreal.log(f"[TennisTools] {player.character}: built {body.get_path_name()} face={face.get_name()} grooms={len(bindings)} "
               f"appearance={appearance.get_name()}")
    return "done"


def _check_owner(player: mp.MetaHumanPlayer) -> None:
    library = unreal.EditorAssetLibrary
    others = [path for path in (player.character_path, player.appearance_path)
              if library.does_asset_exist(path) and library.get_metadata_tag(unreal.load_asset(path), OWNER_TAG) != OWNER_VALUE]
    if others:
        raise RuntimeError(f"Not built by this tool (left untouched; rename or remove them by hand): {others}")


def _apply_body(sub, character, player: mp.MetaHumanPlayer) -> None:
    """測定値を当てて確定する（確定しないと残らない：2026-11-12 に確かめた）。"""
    targets = dict(player.body)
    constraints = list(sub.get_body_constraints(character))
    found = set()
    for constraint in constraints:
        name = str(constraint.get_editor_property("name"))
        if name in targets:
            constraint.set_editor_property("is_active", True)
            constraint.set_editor_property("target_measurement", float(targets[name]))
            found.add(name)
    missing = sorted(set(targets) - found)
    if missing:
        raise RuntimeError(f"{player.character}: no body measurement {missing} in this engine")
    sub.set_body_constraints(character, constraints)
    sub.commit_body_state(character)


def _apply_slots(character, player: mp.MetaHumanPlayer) -> None:
    """髪を替え（指定があれば）、服を外す（テニスウェアは Blender で作る：ART-03 §6 の手順 5）。"""
    collection = character.get_editor_property("internal_collection")
    instance = collection.get_editor_property("default_instance")
    if player.hair_path:
        hair = unreal.load_asset(player.hair_path)
        if hair is None:
            raise FileNotFoundError(f"No hair {player.hair_path}")
        key = collection.try_add_item_from_wardrobe_item(mp.HAIR_SLOT, hair)
        instance.set_single_slot_selection(mp.HAIR_SLOT, key)
    instance.set_single_slot_selection(mp.OUTFIT_SLOT, unreal.MetaHumanPaletteItemKey())


def _built_parts(player: mp.MetaHumanPlayer) -> tuple:
    """組み立てた体・顔のメッシュと、髪の Binding（名前の順）。"""
    folder = player.build_folder
    body = unreal.load_asset(player.body_mesh_path)
    face = unreal.load_asset(player.face_mesh_path)
    if not isinstance(body, unreal.SkeletalMesh) or not isinstance(face, unreal.SkeletalMesh):
        raise RuntimeError(f"The assembly did not make the body and face meshes in {folder}")
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    bindings = sorted((data.get_asset() for data in registry.get_assets_by_path(f"{folder}/Grooms", recursive=False)
                       if str(data.asset_class_path.asset_name) == "GroomBindingAsset"), key=lambda asset: asset.get_path_name())
    return body, face, bindings


def _add_sockets_and_compatibility(body) -> None:
    """体のメッシュに、ラケットのソケットを SK_TennisPlayer_Base と同じ位置と向きで足し（あれば合わせ直す）、共通の骨格を互換の骨格にする。
    骨格には足さない（エンジンの AddSocket は、骨格に足すとメッシュにも入れるので、両方にできてどちらが使われるか分かりにくくなる）。"""
    base = unreal.load_asset(BASE_SKELETON)
    skeleton = body.get_editor_property("skeleton")
    reference = unreal.load_asset("/Game/Tennis/Characters/Players/Interim/SKM_Player_Male01")  # 骨格のソケットを持つ仮の選手
    if not isinstance(base, unreal.Skeleton) or skeleton is None or reference is None:
        raise RuntimeError("No SK_TennisPlayer_Base or no skeleton on the MetaHuman body")
    sockets = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for name in RACKET_SOCKETS:
        source = reference.find_socket(name)
        if source is None:
            raise RuntimeError(f"SK_TennisPlayer_Base has no socket {name}")
        socket = body.find_socket(name)
        if socket is None:
            socket = unreal.new_object(unreal.SkeletalMeshSocket, outer=body)  # AddSocket は持ち主がメッシュのソケットだけを受け付ける
            socket.set_socket_parent(body, source.get_editor_property("bone_name"))
            body.add_socket(socket, False)  # メッシュにだけ足す（名前は "Socket" など。下で付け直す）
            if not sockets.rename_socket(body, socket.get_editor_property("socket_name"), name):
                raise RuntimeError(f"Could not name the socket {name}")
            socket = body.find_socket(name)
        socket.set_socket_local_transform(source.get_socket_local_transform())
    skeleton.add_compatible_skeleton(base)
    for asset in (skeleton, body):
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def apply_wear(key: str) -> list[str]:
    """選手 key の見た目のデータの追従する部品を、顔と服（SKM_Wear_<key>）にし直して保存する（組み立てはしない）。
    書いた部品のパスを返す。服がまだ取り込まれていなければ止める。"""
    player = mp.find_player(key)  # パスを組み立てる前に入力を確かめる
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        raise RuntimeError("Stop PIE or Simulate first (assets are not edited during play)")
    library = unreal.EditorAssetLibrary
    _check_owner(player)
    if not library.does_asset_exist(player.appearance_path):
        raise FileNotFoundError(f"No {player.appearance_path} (build the player with build_metahuman_player first)")
    if not library.does_asset_exist(player.wear_path):
        raise FileNotFoundError(f"No {player.wear_path} (import {player.wear}.fbx with import_exported_fbx first)")
    _, face, _ = _built_parts(player)
    followers = _followers(player, face)
    appearance = unreal.load_asset(player.appearance_path)
    if not isinstance(appearance, unreal.TennisPlayerAppearance):
        raise RuntimeError(f"Not a player appearance: {player.appearance_path}")
    appearance.set_editor_property("followers", followers)
    library.save_loaded_asset(appearance, only_if_is_dirty=False)
    paths = [part.get_path_name() for part in followers]
    unreal.log(f"[TennisTools] {player.appearance}: followers={paths}")
    return paths


def _followers(player: mp.MetaHumanPlayer, face) -> list:
    """追従する部品：顔と、取り込んであれば服（ART-03 §6 の手順 5・6。組み立て直しで服が外れないように、ここだけで決める）。"""
    if not unreal.EditorAssetLibrary.does_asset_exist(player.wear_path):
        return [face]
    wear = unreal.load_asset(player.wear_path)
    skeleton = wear.get_editor_property("skeleton") if isinstance(wear, unreal.SkeletalMesh) else None
    if skeleton is None or skeleton.get_path_name().split(".")[0] != mp.SKELETON:
        raise RuntimeError(f"{player.wear_path} is not a skeletal mesh on {mp.SKELETON}")
    return [face, wear]


def _appearance(player: mp.MetaHumanPlayer, body, face, bindings):
    """見た目のデータ：体、追従する部品は顔と服、髪は組み立てた Binding（DD-03 §7）。"""
    followers = _followers(player, face)  # 服の骨格が違えば、見た目のデータを作る前に止める
    library = unreal.EditorAssetLibrary
    if library.does_asset_exist(player.appearance_path):
        appearance = unreal.load_asset(player.appearance_path)
    else:
        appearance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            player.appearance, mp.METAHUMAN_FOLDER, unreal.TennisPlayerAppearance, unreal.DataAssetFactory())
        library.set_metadata_tag(appearance, OWNER_TAG, OWNER_VALUE)
    appearance.set_editor_property("body", body)
    appearance.set_editor_property("followers", followers)
    appearance.set_editor_property("groom_bindings", bindings)
    library.save_loaded_asset(appearance, only_if_is_dirty=False)
    return appearance
