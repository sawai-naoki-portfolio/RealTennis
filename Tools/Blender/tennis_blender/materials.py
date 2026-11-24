"""マテリアルのスロットの名前と、確認用の色（bpy に依存しない純粋なデータ）。

本番のシェーダーは UE で作る（ART-03 §3）。ここの色は、テストマップでラインやネットを見分けるための仮の色で、
FBX の拡散色として UE の仮のマテリアル（Interchange の MaterialInstance）に渡る。値は sRGB に近い 0〜1。
"""
from __future__ import annotations

COURT_OUT = "M_Court_Out"
COURT_IN = "M_Court_In"
COURT_LINE = "M_Court_Line"
NET = "M_Net"
NET_BAND = "M_NetBand"
NET_POST = "M_NetPost"
BALL = "M_Ball"
RACKET_FRAME = "M_Racket_Frame"
RACKET_GRIP = "M_Racket_Grip"
RACKET_STRINGS = "M_Racket_Strings"
FENCE_POST = "M_Fence_Post"
FENCE_NET = "M_Fence_Net"
BENCH = "M_Bench"
LIGHT_POLE = "M_LightPole"
LIGHT_HEAD = "M_LightHead"
PROP_FRAME = "M_Prop_Frame"
PROP_DARK = "M_Prop_Dark"
PROP_ACCENT = "M_Prop_Accent"
PROP_LIGHT = "M_Prop_Light"
PROP_SCREEN = "M_Prop_Screen"
# センターコート A のスタジアムの仮の形（ART-01 A-STD-010）
STADIUM_CONCRETE = "M_Stadium_Concrete"
STADIUM_SEAT = "M_Stadium_Seat"
STADIUM_AISLE = "M_Stadium_Aisle"
STADIUM_SIGNAGE = "M_Stadium_Signage"
# 仮の選手（ART-01 A-CHR-004・005）。A＝男性、B＝女性。白（ショートパンツ・スカート・靴下）・靴・目は共有する
PLAYER_SKIN_A = "M_Player_Skin_A"
PLAYER_SKIN_B = "M_Player_Skin_B"
PLAYER_HAIR_A = "M_Player_Hair_A"
PLAYER_HAIR_B = "M_Player_Hair_B"
PLAYER_TOP_A = "M_Player_Top_A"
PLAYER_TOP_B = "M_Player_Top_B"
PLAYER_BAND_A = "M_Player_Band_A"
PLAYER_BAND_B = "M_Player_Band_B"
PLAYER_WHITE = "M_Player_White"
PLAYER_SHOE = "M_Player_Shoe"
PLAYER_SOLE = "M_Player_Sole"
PLAYER_LIPS = "M_Player_Lips"
PLAYER_EYE = "M_Player_Eye"
PLAYER_IRIS = "M_Player_Iris"

PREVIEW_COLORS: dict[str, tuple[float, float, float]] = {
    COURT_OUT: (0.10, 0.30, 0.18),   # 余白：濃い緑
    COURT_IN: (0.12, 0.28, 0.55),    # コート：青（ハードの見た目）
    COURT_LINE: (0.95, 0.95, 0.95),  # ライン：白
    NET: (0.03, 0.03, 0.03),         # 網：黒
    NET_BAND: (0.95, 0.95, 0.95),    # 白帯：白
    NET_POST: (0.15, 0.18, 0.20),    # ポスト・スティック：濃い灰色
    BALL: (0.80, 0.90, 0.10),        # ボール：黄色
    RACKET_FRAME: (0.70, 0.10, 0.08),    # ラケットのフレーム：赤（色の種類は UE のインスタンス。A-RKT-002）
    RACKET_GRIP: (0.06, 0.06, 0.06),     # グリップ：黒
    RACKET_STRINGS: (0.92, 0.92, 0.85),  # ストリング：生成り
    FENCE_POST: (0.16, 0.22, 0.18),      # フェンスの支柱・横桟：濃い緑の灰色
    FENCE_NET: (0.08, 0.20, 0.12),       # フェンスの網：濃い緑（UE で Masked・Two Sided）
    BENCH: (0.55, 0.36, 0.20),           # ベンチ：木の茶色
    LIGHT_POLE: (0.55, 0.57, 0.60),      # 照明柱・腕：明るい灰色
    LIGHT_HEAD: (0.95, 0.95, 0.85),      # 灯具：白（UE で発光にする）
    PROP_FRAME: (0.60, 0.62, 0.65),      # 小物の骨組み（審判台の脚・掲示板の支柱・受け箱）：明るい灰色
    PROP_DARK: (0.05, 0.05, 0.06),       # 小物の黒い部品（車輪・発射口・取っ手・掲示板の枠）
    PROP_ACCENT: (0.10, 0.35, 0.22),     # 小物の色のついた面（座面・本体・缶の胴・的の外側）：緑
    PROP_LIGHT: (0.93, 0.93, 0.90),      # 小物の白い部品（ふた・的の中）
    PROP_SCREEN: (0.02, 0.04, 0.10),     # 掲示板の画面：濃い紺（UE で発光と表示にする）
    STADIUM_CONCRETE: (0.46, 0.46, 0.45),  # スタジアムの壁・通路・手すり壁・外の壁：灰色
    STADIUM_SEAT: (0.14, 0.24, 0.40),      # 観客席の段（座席は後の作業で置く）：紺
    STADIUM_AISLE: (0.75, 0.75, 0.72),     # 縦の通路の帯：明るい灰色
    STADIUM_SIGNAGE: (0.05, 0.22, 0.14),   # 看板の壁（看板の絵は UE で付ける）：濃い緑
    PLAYER_SKIN_A: (0.72, 0.50, 0.38),   # 男性の肌
    PLAYER_SKIN_B: (0.88, 0.68, 0.56),   # 女性の肌
    PLAYER_HAIR_A: (0.08, 0.06, 0.04),   # 男性の髪：黒に近い茶色
    PLAYER_HAIR_B: (0.32, 0.17, 0.08),   # 女性の髪：明るい茶色
    PLAYER_TOP_A: (0.08, 0.16, 0.36),    # 男性のシャツ：紺
    PLAYER_TOP_B: (0.90, 0.38, 0.32),    # 女性のトップ：珊瑚色
    PLAYER_BAND_A: (0.08, 0.16, 0.36),   # 男性のリストバンド：紺
    PLAYER_BAND_B: (0.05, 0.50, 0.55),   # 女性のヘッドバンド・髪のゴム：青緑
    PLAYER_WHITE: (0.92, 0.92, 0.90),    # ショートパンツ・スカート・靴下：白
    PLAYER_SHOE: (0.85, 0.86, 0.88),     # 靴：明るい灰色がかった白
    PLAYER_SOLE: (0.30, 0.31, 0.33),     # 靴底：灰色
    PLAYER_LIPS: (0.62, 0.36, 0.33),     # 唇
    PLAYER_EYE: (0.90, 0.90, 0.88),      # 白目
    PLAYER_IRIS: (0.18, 0.11, 0.06),     # 黒目：濃い茶色
}
