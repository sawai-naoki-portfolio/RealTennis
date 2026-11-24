"""Unreal MCP に公開するプロジェクト専用のツール（ARCH-04 §4）。標準のツールセットにない作業だけを置く。

注意：ツールの引数と戻り値の型の注釈は、登録の仕組みが実行時に読む。`from __future__ import annotations` を使わない。
"""
import os

import unreal

import toolset_registry

from . import court_level_layout as layout
from . import fbx_export, fbx_import, locomotion_build, metahuman_build, player_base, stroke_build
from .import_rules import plan_import
from .shoot_rules import validate_shoot_ball


def _export_dir() -> str:
    return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Export")


def _shot_type_assets() -> list:
    """打球ツールに渡す球種のデータ（SHOT_TESTER.shot_type_folder の DA_Shot_*）を、球種の列挙の順に並べる。"""
    paths = unreal.EditorAssetLibrary.list_assets(layout.SHOT_TESTER.shot_type_folder, recursive=False)
    shots = [asset for asset in (unreal.load_asset(path) for path in paths) if isinstance(asset, unreal.ShotTypeData)]
    return sorted(shots, key=lambda shot: shot.get_editor_property("type").value)


def _mesh_object_path(asset_name: str) -> str:
    """取り込みの規則（import_rules）と同じ場所を指す（DRY）。"""
    destination = plan_import(asset_name + ".fbx", _export_dir()).destination_path
    return f"{destination}/{asset_name}.{asset_name}"


@unreal.uclass()
class TennisTools(unreal.ToolsetDefinition):
    """Project-specific tools for RealTennis (asset import, test level, shot tester, toolset reload)."""

    @toolset_registry.tool_call
    @staticmethod
    def import_exported_fbx(file_names: list[str]) -> list[str]:
        """Imports (or reimports) FBX files written by Tools/Blender/export_all.py.

        Only file names in the project's Export/ folder are accepted; the destination folder and
        the Nanite setting are decided by the asset-name prefix (see import_rules.DESTINATIONS).
        Existing assets are replaced and the result is saved. Preview materials created from Export/
        are refreshed from the FBX values (Interchange alone keeps the old values on reimport).
        Static mesh sockets (SOCKET_ empties) keep their FBX location and get zero rotation and unit
        scale (Interchange alone leaves the FBX axis and unit conversion on them).
        SKM_Player_ files are imported as skeletal meshes onto the existing SK_TennisPlayer_Base
        skeleton and PA_Mannequin physics asset (none are created), and 4 LODs are generated.
        SKM_Wear_ files (MetaHuman player tennis wear) go to /Game/Tennis/Characters/Players/MetaHuman/Wear
        on the shared metahuman_base_skel skeleton without a physics asset, with 3 LODs (ART-03 6).
        AS_FH_ and AS_BH1_ files (stroke animations) are imported as animations only onto
        SK_TennisPlayer_Base, into /Game/Tennis/Characters/Animations/Strokes.

        Args:
            file_names: File names in Export/, e.g. ["SM_Court_Lines.fbx", "SM_Net.fbx"].

        Returns:
            The imported asset object paths, in input order.
        """
        if not file_names:
            raise ValueError("file_names is empty")
        plans = [plan_import(name, _export_dir()) for name in file_names]  # 1 つでも規則に合わなければ何もしない
        missing = [p.source_file for p in plans if not os.path.isfile(p.source_file)]
        if missing:
            raise FileNotFoundError(f"Not found in Export/: {missing}")

        imported = fbx_import.import_plans(plans, _export_dir())
        unreal.log(f"[TennisTools] imported {len(imported)} asset(s): {imported}")
        return imported

    @toolset_registry.tool_call
    @staticmethod
    def export_player_base_fbx() -> list[str]:
        """Exports the base bodies (LOD0, skeleton, weights) to Export/Base/ as FBX.

        The bodies are player_base.BASE_MESHES: Manny and Quinn, which Tools/Blender/build_players.py
        reads to build the interim players on the same skeleton and reference pose (ART-01 A-CHR-004/005),
        and the assembled MetaHuman player bodies, which Tools/Blender/build_metahuman_wear.py reads to
        build the tennis wear as shells of each body (A-CHR-040..043, ART-03 6 step 5). Export/Base/ is
        not imported back by import_exported_fbx (only files directly in Export/ are). Existing files are
        overwritten.

        Returns:
            The written FBX file paths.
        """
        written = []
        for name, asset_path in player_base.BASE_MESHES.items():
            out_file = player_base.base_fbx_path(_export_dir(), name)
            fbx_export.export_skeletal_mesh(asset_path, out_file)
            written.append(out_file)
        unreal.log(f"[TennisTools] exported {len(written)} base mesh(es): {written}")
        return written

    @toolset_registry.tool_call
    @staticmethod
    def build_test_court_level() -> list[str]:
        """Creates or rebuilds the test level /Game/Tennis/Maps/L_TestCourt and saves it.

        Places the imported court, net and practice-court prop meshes (fence, benches, light poles) at
        the court-local origin, an ATennisCourt at the origin, an ATennisBall behind the opponent's
        baseline (court_level_layout.BALL), an ATennisShotTester on the opponent's side (+X) with every
        DA_Shot_* (click the court in PIE, or use shoot_ball), a PlayerStart where the player character
        appears (-X side), and minimal movable lighting (no lighting build needed), all inside the outliner folder
        "TennisTestCourt". When the level already exists, only the actors in that folder are replaced.
        Nothing is changed if any mesh asset or shot type is missing.

        Returns:
            The labels of the placed actors.
        """
        mesh_paths = {name: _mesh_object_path(name) for name in [p.asset_name for p in layout.MESHES] + [layout.BALL.mesh_asset]}
        missing = [path for path in mesh_paths.values() if not unreal.EditorAssetLibrary.does_asset_exist(path)]
        if missing:
            raise FileNotFoundError(f"Missing mesh assets (run import_exported_fbx first): {missing}")
        shot_types = _shot_type_assets()
        if not shot_types:
            raise FileNotFoundError(f"No shot types in {layout.SHOT_TESTER.shot_type_folder}")

        level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if unreal.EditorAssetLibrary.does_asset_exist(layout.LEVEL_PATH):
            if not level_editor.load_level(layout.LEVEL_PATH):
                raise RuntimeError(f"Could not load {layout.LEVEL_PATH}")
            for actor in actors.get_all_level_actors():  # このツールが置いたものだけを消す
                if str(actor.get_folder_path()) == layout.OUTLINER_FOLDER:
                    actors.destroy_actor(actor)
        elif not level_editor.new_level(layout.LEVEL_PATH):
            raise RuntimeError(f"Could not create {layout.LEVEL_PATH}")

        placed: list[str] = []

        def _finish(actor: unreal.Actor, label: str) -> None:
            actor.set_actor_label(label)
            actor.set_folder_path(layout.OUTLINER_FOLDER)
            placed.append(label)

        for placement in layout.MESHES:
            mesh = unreal.load_asset(mesh_paths[placement.asset_name])
            actor = actors.spawn_actor_from_object(mesh, unreal.Vector(*placement.location_cm))
            _finish(actor, placement.asset_name)

        court = actors.spawn_actor_from_class(getattr(unreal, layout.COURT_ACTOR_CLASS), unreal.Vector(0.0, 0.0, 0.0))
        _finish(court, layout.COURT_ACTOR_CLASS)

        ball_spec = layout.BALL
        ball = actors.spawn_actor_from_class(getattr(unreal, ball_spec.actor_class), unreal.Vector(*ball_spec.location_cm))
        ball.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(unreal.load_asset(mesh_paths[ball_spec.mesh_asset]))
        _finish(ball, ball_spec.actor_class)

        tester_spec = layout.SHOT_TESTER
        tester = actors.spawn_actor_from_class(getattr(unreal, tester_spec.actor_class), unreal.Vector(0.0, 0.0, 0.0))
        tester.set_editor_property("ball", ball)
        tester.set_editor_property("shot_types", shot_types)
        tester.set_editor_property("hit_point", unreal.Vector(*tester_spec.hit_point_m))
        _finish(tester, tester_spec.actor_class)

        pitch, yaw, roll = layout.PLAYER_START_ROTATION
        start = actors.spawn_actor_from_class(getattr(unreal, layout.PLAYER_START_CLASS), unreal.Vector(*layout.PLAYER_START_LOCATION_CM),
                                              unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
        _finish(start, layout.PLAYER_START_CLASS)

        for class_name, (pitch, yaw, roll) in layout.LIGHTS:
            actor = actors.spawn_actor_from_class(getattr(unreal, class_name), unreal.Vector(0.0, 0.0, 500.0),
                                                  unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
            actor.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)  # ライティングのビルドを不要にする
            sky = actor.get_component_by_class(unreal.SkyLightComponent)
            if sky is not None:
                sky.set_editor_property("real_time_capture", True)  # 可動の空の光は、リアルタイムで空を取り込む
            _finish(actor, class_name)

        if not level_editor.save_current_level():
            raise RuntimeError(f"Could not save {layout.LEVEL_PATH}")
        unreal.log(f"[TennisTools] built {layout.LEVEL_PATH}: {placed}")
        return placed

    @toolset_registry.tool_call
    @staticmethod
    def shoot_ball(shot_type: str, target_x_m: float, target_y_m: float) -> bool:
        """While PIE or Simulate is running, shoots the ball with the level's ATennisShotTester.

        The ball is solved with the shot solver from the tester's hit point and launched; the result
        (solved, fallback, predicted landing, miss) is logged to LogTennisShot. Turn on
        tennis.Debug.Trajectory (Project Settings > TennisDebug) to see the predicted path.

        Args:
            shot_type: A shot type asset name in /Game/Tennis/Data/Shots, e.g. "DA_Shot_Topspin".
            target_x_m: Target X in court-local metres (net at 0). The tester hits from the opponent's
                side (+X), so a negative X sends the ball to the player's side.
            target_y_m: Target Y in court-local metres (centre line at 0).

        Returns:
            True if the solver converged (the ball is launched either way).
        """
        validate_shoot_ball(shot_type, target_x_m, target_y_m)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if world is None:
            raise RuntimeError("PIE or Simulate is not running")
        testers = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.TennisShotTester)
        if not testers:
            raise RuntimeError("No TennisShotTester in the level (rebuild it with build_test_court_level)")
        path = f"{layout.SHOT_TESTER.shot_type_folder}/{shot_type}.{shot_type}"
        shot = unreal.load_asset(path)
        if not isinstance(shot, unreal.ShotTypeData):
            raise FileNotFoundError(f"Not a shot type: {path}")
        solved = testers[0].shoot_with(shot, unreal.Vector(target_x_m, target_y_m, 0.0))
        unreal.log(f"[TennisTools] shoot_ball {shot_type} target=({target_x_m}, {target_y_m}) solved={solved}")
        return solved

    @toolset_registry.tool_call
    @staticmethod
    def rebuild_stroke_chooser() -> int:
        """Rebuilds the chooser table CHT_StrokeSelect from the DA_Stroke_* assets and saves it.

        Rows come from each stroke's SelectRows (DD-03 5.3); more specific rows go first. The table
        asset is created if it does not exist. Do not edit the table by hand: edit the strokes and
        call this again.

        Returns:
            The number of rows in the rebuilt table.
        """
        return stroke_build.rebuild_chooser()

    @toolset_registry.tool_call
    @staticmethod
    def build_locomotion_database() -> int:
        """Builds the Motion Matching schema PSS_Locomotion and database PSD_Locomotion (DD-03 6.5) and saves them.

        Creates them if they do not exist (the schema uses the PoseSearch default channels: trajectory
        and both feet, on SK_TennisPlayer_Base), then replaces the database animations with the M3
        placeholder locomotion (idle and the 8 walk and 8 jog directions in
        Characters/Animations/Locomotion/Mannequin). Nothing is changed if an animation is missing or an
        existing asset was not made by this tool. Not available during PIE.

        Returns:
            The number of animations in the database.
        """
        return locomotion_build.build_database()

    @toolset_registry.tool_call
    @staticmethod
    def build_placeholder_strokes() -> list[str]:
        """Builds the placeholder forehand and backhand strokes (W-3.4) and rebuilds CHT_StrokeSelect.

        For each stroke in stroke_assets.PLACEHOLDER_STROKES: imports Export/AS_*.fbx (written by
        Tools/Blender/build_stroke_anims.py) as an animation on SK_TennisPlayer_Base, creates or updates
        the montage AM_* with the Tennis Impact and Tennis Recover Start notifies at the times in
        Export/AS_*.json, creates or updates DA_Stroke_* (montage, select row by side, release lead time, charge time), bakes its times
        with StrokeMetaBaker (no body is needed), and saves everything.
        Rebuilding overwrites: AS_* is reimported, all other notifies on AM_* are removed, and the
        select rows of DA_Stroke_* are replaced. Nothing is changed if any file is missing, a JSON file
        is invalid, or an existing asset was not made by this tool (an AS_* not imported from Export/,
        or an AM_*/DA_Stroke_* without its owner tag). If a later check fails (skeleton, length, bake),
        the assets done so far are kept. Then bakes every player appearance (see bake_player_appearances).
        Not available during PIE.

        Returns:
            The object paths of the built DA_Stroke_* assets.
        """
        built = stroke_build.build_placeholders(_export_dir())
        stroke_build.rebuild_chooser()
        stroke_build.bake_appearances()
        return built

    @toolset_registry.tool_call
    @staticmethod
    def build_metahuman_player(player_key: str) -> str:
        """Builds (or continues building) a MetaHuman player (W-3.10, ART-03 6) and returns its stage.

        player_key is one of metahuman_players.PLAYERS ("M01": Kelvin, "F01": Celeste). Each call does
        one stage and returns it: creates MHC_Player_<key> from the Core Data preset (body measurements,
        hair, no outfit) and runs the cloud auto rig, waiting for it ("rigged", minutes; the editor is busy),
        downloads the texture sources ("textures"; the assembly needs them), then assembles it (Optimized,
        Medium; shared Common folder), adds the racket sockets to the body mesh and SK_TennisPlayer_Base as a
        compatible skeleton to the shared skeleton, writes DA_Appearance_Player_<key> (body, face, groom
        bindings) and bakes every player appearance like bake_player_appearances ("done"). Call again until
        it returns "done". The Epic sign-in and the MetaHuman EULA are the user's. An existing
        MHC_Player_<key> or DA_Appearance_Player_<key> without this tool's owner tag is left untouched.
        Not available during PIE.

        Args:
            player_key: The player key, e.g. "M01".

        Returns:
            "rigged", "textures" or "done".
        """
        return metahuman_build.build_player(player_key)

    @toolset_registry.tool_call
    @staticmethod
    def apply_metahuman_wear(player_key: str) -> list[str]:
        """Sets the follower parts of DA_Appearance_Player_<key> to its face and tennis wear and saves it.

        Run after import_exported_fbx imported SKM_Wear_<key>.fbx (MetaHuman/Wear, on the shared
        metahuman_base_skel; ART-03 6 step 5) to put the wear on the player without assembling again
        (build_metahuman_player also adds the wear when it exists). player_key is one of
        metahuman_players.PLAYERS ("M01", "F01"). Stops if the wear is not imported, or if the appearance
        was not made by build_metahuman_player. Not available during PIE.

        Args:
            player_key: The player key, e.g. "M01".

        Returns:
            The follower mesh paths (face, wear).
        """
        return metahuman_build.apply_wear(player_key)

    @toolset_registry.tool_call
    @staticmethod
    def bake_player_appearances() -> list[str]:
        """Bakes every player appearance (UTennisPlayerAppearance in /Game/Tennis/Characters/Players) and saves it.

        For each appearance, StrokeMetaBaker.BakeAppearance poses its Body at the impact of every
        DA_Stroke_* and writes the racket sweet spot per stroke (StrokeFits), and the reach body
        offsets from the strokes CHT_StrokeSelect picks for mid-height drives (ReachBody). DD-03 7.
        Call this after changing a stroke montage, a player body or the racket. Stops at the first
        appearance that cannot be baked (the ones before it are saved). Not available during PIE.

        Returns:
            The object paths of the baked appearances, in name order.
        """
        return stroke_build.bake_appearances()

    @toolset_registry.tool_call
    @staticmethod
    def reload_tennis_toolset() -> str:
        """Reloads the tennis_toolset Python package and re-registers its tools (no editor restart).

        Use after editing files in Tools/Unreal/tennis_toolset.

        Returns:
            A short status message.
        """
        import tennis_toolset

        toolset_registry.reload_module(tennis_toolset)
        return "tennis_toolset reloaded"
