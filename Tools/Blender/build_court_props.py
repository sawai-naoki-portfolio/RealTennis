"""コートサイドの小物を作る（A-PRP-001〜006）：SM_Prop_UmpireChair・SM_Prop_BallMachine・SM_Prop_BallCan・
SM_Prop_Cooler・SM_Prop_ServeTarget・SM_Prop_Scoreboard と、球出し機の子のソケット SOCKET_muzzle（Empty）。

どれも自分の局所座標（原点＝接地面の中心、+X＝正面）なので、Blender では原点に重なる。形は tennis_blender/court_props.py。
何度実行しても同じ結果になる。
"""
from tennis_blender import bpy_utils as bu
from tennis_blender import court_props as cp

COLLECTION = "Tennis_Props"


def build() -> None:
    coll = bu.reset_collection(COLLECTION)
    made = {name: bu.slotted_object(name, items, coll) for name, items in cp.parts().items()}
    for name, (parent, location) in cp.sockets().items():
        bu.socket(name, made[parent], location, coll)
    bu.log(f"{COLLECTION}: " + "・".join(f"{o.name}（三角形 {bu.triangle_count(o)}）" for o in made.values())
           + f" と {'・'.join(cp.sockets())} を作った")


if __name__ == "__main__":
    build()
