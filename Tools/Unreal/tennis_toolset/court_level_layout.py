"""テストマップ L_TestCourt の配置（unreal に依存しない純粋なデータ。CI で単体テストする）。

コート・ネット・ボールのメッシュは、Blender でコート局所座標（ネット中央の地面が原点）に作ってあるので、
原点に置けばコートの形になる（ART-03 §3）。位置の単位は cm（UE のワールド）。
"""
from __future__ import annotations

from dataclasses import dataclass

LEVEL_PATH = "/Game/Tennis/Maps/L_TestCourt"
OUTLINER_FOLDER = "TennisTestCourt"  # このツールが置いたアクタだけを入れる（作り直すときに消す範囲）


@dataclass(frozen=True)
class MeshPlacement:
    asset_name: str
    location_cm: tuple[float, float, float] = (0.0, 0.0, 0.0)


MESHES: tuple[MeshPlacement, ...] = (
    MeshPlacement("SM_Court_Surface"),
    MeshPlacement("SM_Court_Inner"),
    MeshPlacement("SM_Court_Lines"),
    MeshPlacement("SM_Net"),
    MeshPlacement("SM_NetBand"),
    MeshPlacement("SM_NetPost"),
    MeshPlacement("SM_SinglesStick"),
    # 練習コートの小物（A-STD-001）。コート局所座標で作ってあるので、原点に置くだけで周りにそろう
    MeshPlacement("SM_Practice_Fence"),
    MeshPlacement("SM_Practice_Bench"),
    MeshPlacement("SM_Practice_LightPole"),
)

# シミュレーションの座標の基準（ATennisCourt）。原点（ネット中央の地面）に置く
COURT_ACTOR_CLASS = "TennisCourt"


@dataclass(frozen=True)
class BallPlacement:
    actor_class: str
    mesh_asset: str
    location_cm: tuple[float, float, float]


# ボールは打たれるまで、相手の側（+X。打球ツールが打つ側）のベースラインの後ろで待つ。打つのは打球ツール（SHOT_TESTER）
BALL = BallPlacement("TennisBall", "SM_Ball", (1190.0, 0.0, 95.0))


@dataclass(frozen=True)
class ShotTesterPlacement:
    actor_class: str
    shot_type_folder: str                    # DA_Shot_* を探すフォルダ（球種の列挙の順に並べて渡す）
    hit_point_m: tuple[float, float, float]  # 打点（m、コート局所。ATennisShotTester.HitPoint）


# 打球ツール（W-2.4）：相手の側（+X）のベースラインの内側の打点から、クリックした地点（MCP では shoot_ball）へ打つ。
# 選手の側（−X）を狙えば、選手に向かって球が来る（打点の探索とストロークの確認：W-3.3〜）
SHOT_TESTER = ShotTesterPlacement("TennisShotTester", "/Game/Tennis/Data/Shots", (11.0, 0.0, 1.0))

# 選手（ATennisCharacter）の出現位置：選手の側（−X）のベースラインの後ろ、ネットの方を向く。Z はカプセルの半分の高さ（0.9 m）より上
PLAYER_START_CLASS = "PlayerStart"
PLAYER_START_LOCATION_CM = (-1250.0, 0.0, 100.0)
PLAYER_START_ROTATION = (0.0, 0.0, 0.0)  # (pitch, yaw, roll)

# PIE の視点は、選手のカメラ（ATennisPlayerCameraManager の中継カメラ。DD-07 §1.1）が決めるので、カメラは置かない

# 見るための最小限の照明（アクタのクラス名, 回転 (pitch, yaw, roll)）
LIGHTS: tuple[tuple[str, tuple[float, float, float]], ...] = (
    ("DirectionalLight", (-45.0, 30.0, 0.0)),
    ("SkyLight", (0.0, 0.0, 0.0)),
    ("SkyAtmosphere", (0.0, 0.0, 0.0)),
)
