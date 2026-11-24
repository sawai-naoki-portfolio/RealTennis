"""Create the playable Center Court A level once, in the editor, after importing the art kit.

Run from Unreal's Python environment. Refuse to replace existing levels or switch away
from unsaved work. All transforms are in UE centimetres; the net centre is the origin.
"""
import math

import unreal

from tennis_toolset import court_level_layout as layout
from tennis_toolset.import_rules import DESTINATIONS


LEVEL = "/Game/Tennis/Maps/L_CenterCourtA"
# Each tuple is (asset, x, y, yaw). FBX reflects Blender Y, so yaw also changes sign.
MESHES = [
    (name, 0, 0, 0) for name in (
        "SM_Court_A_MatchDeck", "SM_Stadium_A_Stands", "SM_Stadium_A_Barrier",
        "SM_Stadium_A_SeatBanks", "SM_Net", "SM_NetBand", "SM_NetPost", "SM_SinglesStick",
    )
] + [
    ("SM_Prop_UmpireChair", 0, -729.9, 90),
    ("SM_Prop_Scoreboard", -1690, 994, -90),
    ("SM_Prop_A_PlayersChair", -145, -770, 90),
    ("SM_Prop_A_PlayersChair", 145, -770, 90),
    ("SM_Prop_A_DrinksTable", -214, -770, 90),
    ("SM_Prop_A_DrinksTable", 214, -770, 90),
    ("SM_Prop_A_BroadcastCamera", -410, -860, 90 - math.degrees(.23)),
]


def load_mesh(name):
    folder = next(folder for prefix, folder, _, _ in DESTINATIONS if name.startswith(prefix))
    mesh = unreal.load_asset(f"{folder}/{name}")
    if not isinstance(mesh, unreal.StaticMesh):
        raise ValueError(f"Import the static mesh {folder}/{name} first")
    return mesh


def build():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if world.get_game_world() or unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
        raise RuntimeError("End PIE and save the current work before building the venue")
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
        raise FileExistsError(LEVEL)
    meshes = {name: load_mesh(name) for name in {row[0] for row in MESHES} | {layout.BALL.mesh_asset}}
    net_material = unreal.load_asset("/Game/Tennis/Art/Net/M_CenterA_NetWeave")
    if not isinstance(net_material, unreal.MaterialInterface):
        raise ValueError("Import M_CenterA_NetWeave before building the venue")
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not editor.new_level(LEVEL):
        raise RuntimeError(f"Could not create {LEVEL}")
    for name, x, y, yaw in MESHES:
        actor = actors.spawn_actor_from_object(meshes[name], unreal.Vector(x, y, 0), unreal.Rotator(yaw=yaw))
        if name == "SM_Net":
            actor.get_component_by_class(unreal.StaticMeshComponent).set_material(0, net_material)
        actor.set_actor_label(name, mark_dirty=True)
        actor.set_folder_path("Venue/Meshes")
    actors.spawn_actor_from_class(unreal.TennisCourt, unreal.Vector())
    actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(*layout.PLAYER_START_LOCATION_CM))
    ball = actors.spawn_actor_from_class(unreal.TennisBall, unreal.Vector(*layout.BALL.location_cm))
    ball.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(meshes[layout.BALL.mesh_asset])
    for kind, (pitch, yaw, roll) in layout.LIGHTS:
        light = actors.spawn_actor_from_class(getattr(unreal, kind), unreal.Vector(0, 0, 500),
                                             unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))
        light.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky = light.get_component_by_class(unreal.SkyLightComponent)
        if sky:
            sky.set_editor_property("real_time_capture", True)
        light.set_folder_path("Venue/Daylight")
    if not editor.save_current_level():
        raise RuntimeError(f"Could not save {LEVEL}")
    return LEVEL


if __name__ == "__main__":
    build()
