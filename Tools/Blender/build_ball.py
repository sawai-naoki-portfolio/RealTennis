"""ボールを作る（A-BAL-001）：SM_Ball（32 × 16）、SM_Ball_LOD1（16 × 8）、Type 3 用の SM_Ball_Large。

原点は球の中心。3 つとも原点に重ねて置く（トランスフォームは単位のまま。FBX でも原点がずれない）。
何度実行しても同じ結果になる。
"""
import bmesh

from tennis_blender import bpy_utils as bu
from tennis_blender import dims as d
from tennis_blender import materials as mat

COLLECTION = "Tennis_Ball"
BALLS = (
    ("SM_Ball", d.BALL_RADIUS, d.BALL_SEGMENTS, d.BALL_RINGS),
    ("SM_Ball_LOD1", d.BALL_RADIUS, d.BALL_LOD1_SEGMENTS, d.BALL_LOD1_RINGS),
    ("SM_Ball_Large", d.BALL_RADIUS_LARGE, d.BALL_SEGMENTS, d.BALL_RINGS),
)


def _sphere(name, radius, segments, rings, collection):
    bm = bmesh.new()
    bm.loops.layers.uv.new("UVMap")  # calc_uvs は UV のレイヤーがあるときだけ働く
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=radius, calc_uvs=True)
    for face in bm.faces:
        face.smooth = True
    return bu.bmesh_object(name, bm, collection, mat.BALL)


def build() -> None:
    coll = bu.reset_collection(COLLECTION)
    for name, radius, segments, rings in BALLS:
        _sphere(name, radius, segments, rings, coll)
    bu.log(f"{COLLECTION}: " + "・".join(f"{n}（半径 {r * 100:.3f} cm、{s} x {g}）" for n, r, s, g in BALLS) + " を作った")


if __name__ == "__main__":
    build()
