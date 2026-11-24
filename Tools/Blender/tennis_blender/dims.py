"""ITF 規格の寸法（単位 m）。bpy に依存しない。

座標はコート局所座標：原点＝ネット中央の地面、+X＝コートの長さ方向、+Z＝上（ラケットだけは自分の局所座標。下の節）。
出典：
- コート・ネット：ITF Rules of Tennis 2026, Rule 1（Docs/00_Research/01_Tennis_Rules_Research.md §1）
- ボール：Appendix I（同 §2）
- ラケット：Rule 4・Appendix II の上限（同 §3）の中で、一般的な大きさにする（ART-01 A-RKT-001、DD-03 §7）
- 余白・ポストの高さ：Docs/07_Art_Animation/01_Asset_List.md §2（A-CRT-001・A-NET-002）
"""

# ---- コート（Rule 1）。コートの寸法は、ラインの外側の縁まで測る ----
HALF_LENGTH = 23.77 / 2              # ベースラインの外側の縁
HALF_WIDTH_SINGLES = 8.23 / 2        # シングルスのサイドラインの外側の縁
HALF_WIDTH_DOUBLES = 10.97 / 2       # ダブルスのサイドラインの外側の縁
SERVICE_LINE_X = 6.40                # ネットからサービスラインの外側の縁まで
LINE_WIDTH = 0.05                    # ライン（センターサービスライン・センターマークを含む）
BASELINE_WIDTH = 0.10                # ベースライン（上限 10 cm）
CENTER_MARK_LENGTH = 0.10            # ベースラインからコートの内側へ

# ---- コートの外の余白（A-CRT-001） ----
RUNOFF_BACK = 6.40                   # ベースラインの後ろ
RUNOFF_SIDE = 3.66                   # ダブルスのサイドラインの横

# ---- ネット（Rule 1） ----
NET_HEIGHT_CENTER = 0.914            # 中央（ストラップで固定）
NET_HEIGHT_POST = 1.07               # ポストとシングルススティックでの高さ
POST_OFFSET = 0.914                  # ポスト・スティックの中心は、サイドラインのこの距離だけ外側
POST_Y = HALF_WIDTH_DOUBLES + POST_OFFSET      # 6.399
STICK_Y = HALF_WIDTH_SINGLES + POST_OFFSET     # 5.029
POST_DIAMETER = 0.15                 # 上限 15 cm
POST_HEIGHT = 1.095                  # 1.07 m ＋ ネットコードからの突出 2.5 cm（A-NET-002）
STICK_DIAMETER = 0.075               # 上限 7.5 cm
STICK_HEIGHT = 1.07
BAND_DEPTH = 0.05                    # 白帯の片面の深さ（5〜6.35 cm）
CORD_DIAMETER = 0.008                # ネットコードの直径（上限 0.8 cm）。白帯の厚みに使う
NET_SEGMENTS_Y = 64                  # 横の分割（ART-01 A-NET-001）
NET_SEGMENTS_Z = 12                  # 縦の分割（同）

# ---- ボール（Appendix I） ----
BALL_RADIUS = 0.0335                 # Type 1・2・高地球（直径 6.54〜6.86 cm の中央付近）
BALL_RADIUS_LARGE = 0.03575          # Type 3（直径 7.00〜7.30 cm の中央付近）
BALL_SEGMENTS, BALL_RINGS = 32, 16             # UV Sphere の分割（ART-03 §4）
BALL_LOD1_SEGMENTS, BALL_LOD1_RINGS = 16, 8    # LOD1 の分割（同）

# ---- ラケット（A-RKT-001。上限は Appendix II：全長 73.7 cm・全幅 31.7 cm・打球面 39.4 × 29.2 cm） ----
# 局所座標：原点＝グリップエンド（バットキャップの端面の中心）、+X＝グリップ → 先端、Y＝フェイスの幅、Z＝フェイスの法線
RACKET_LENGTH = 0.686                # 全長（27 in）
RACKET_FACE_LENGTH = 0.32            # 打球面（フレームの内側の楕円）の長さ。約 97 平方インチ
RACKET_FACE_WIDTH = 0.25             # 打球面の幅
RACKET_FRAME_WIDTH = 0.010           # フレームの面内の幅（打球面の縁 → 外側の縁）
RACKET_BEAM = 0.023                  # フレームの厚さ（Z）
RACKET_FRAME_CHAMFER = 0.003         # フレーム・シャフトの断面の角を落とす長さ
RACKET_SHAFT_WIDTH = 0.022           # シャフトの先（腕に分かれる所）の幅（Y）
RACKET_THROAT_X = 0.25               # シャフトが 2 本の腕に分かれる位置
RACKET_THROAT_JOIN_DEG = 45.0        # 腕がヘッドに付く角度（打球面の中心から見て、グリップ側の軸から）
RACKET_GRIP_LENGTH = 0.19            # グリップエンド → グリップの端
RACKET_GRIP_ACROSS = 0.0335          # グリップの正八角形の対辺の距離（周 11.1 cm＝4 3/8 in、グリップサイズ 3）
RACKET_BUTT_ACROSS = 0.037           # バットキャップ（グリップエンドのふくらみ）の対辺の距離
RACKET_BUTT_LENGTH = 0.010           # バットキャップの長さ
RACKET_STRING_MAINS = 16             # 縦糸の本数（16 × 19 のパターン）
RACKET_STRING_CROSSES = 19           # 横糸の本数
RACKET_STRING_GAUGE = 0.0013         # 糸の太さ（16 ゲージ）
RACKET_OVERLAP_INSET = 0.0005        # 重なる部品（腕・横糸）を細くする量。面が同じ平面に乗るとちらつくため
RACKET_RING_SEGMENTS = 96            # ヘッドの周の分割（4 の倍数：楕円の軸の端に頂点を置き、寸法を正確にする）
RACKET_SWEETSPOT_OFFSET = 0.035      # スイートスポット：打球面の中心からグリップ寄り（DD-03 §7）
RACKET_GRIP_SOCKET_X = 0.06          # 握る位置（手のひらの中心）：グリップエンドから

# ---- 練習コートの小物（A-STD-001。屋外・ハード 1 面。コート局所座標。根拠は ART-01 §4） ----
FENCE_HALF_X = HALF_LENGTH + RUNOFF_BACK             # フェンスの中心線：余白の外周（SM_Court_Surface の縁）
FENCE_HALF_Y = HALF_WIDTH_DOUBLES + RUNOFF_SIDE
FENCE_HEIGHT = 3.05                  # 10 ft（3.048 m を丸める。ASTM F969 の 10 ft／12 ft のうち低い方）
FENCE_POST_SPACING_MAX = 3.05        # 支柱の間隔の上限 10 ft（同じく丸める。短い辺 18.29 m が 6 区間になる）。角の間を等分する
FENCE_POST_ACROSS = 0.06             # 支柱（鋼管 2 3/8 in 相当）。正八角柱
FENCE_RAIL_ACROSS = 0.042            # 上の横桟（1 5/8 in 相当）。正八角形の筒
BENCH_LENGTH = 1.80                  # 選手のベンチ（背なし）。X 方向に置く
BENCH_DEPTH = 0.40
BENCH_SEAT_HEIGHT = 0.45             # 座面の上の高さ（一般的な腰掛けの高さ）
BENCH_SEAT_THICKNESS = 0.05
BENCH_LEG_THICKNESS = 0.06           # 脚（板）の X 方向の厚さ
BENCH_LEG_INSET = 0.15               # 脚の外側の面は、座面の端からこの距離だけ内側
BENCH_Y = FENCE_HALF_Y - 0.85        # ベンチの中心：ネットのポストの後ろ、フェンスの手前（x = 0、左右に 1 つずつ）
LIGHT_POLE_HEIGHT = 8.0              # 照明柱（屋外のレクリエーション用は 6〜8 m が一般的）
LIGHT_POLE_ACROSS = 0.15             # 正八角柱
LIGHT_POLE_X = SERVICE_LINE_X        # 柱の位置：サービスラインの延長、フェンスの外（左右に 2 本ずつ、計 4 本）
LIGHT_POLE_Y = FENCE_HALF_Y + 0.50
LIGHT_HEAD_SIZE = (0.50, 0.60, 0.12)  # 灯具（箱）の大きさ：コートへ向く奥行き・幅（X）・厚さ
LIGHT_ARM_LENGTH = 0.40              # 柱の中心から灯具の根元まで（コートの方へ）
LIGHT_ARM_ACROSS = 0.08              # 腕（角材）の太さ

# ---- コートサイドの小物（A-PRP-001〜006。自分の局所座標：原点＝接地面の中心、+X＝正面、+Z＝上。XZ の面について対称。
#      根拠は ART-01 §4.2） ----
UMPIRE_CHAIR_SEAT_HEIGHT = 1.90      # 審判台の座面の上の高さ（ITF の推奨 1.82〜2.44 m の中）
UMPIRE_CHAIR_DEPTH = 0.90            # 脚の外側の奥行き（X）
UMPIRE_CHAIR_WIDTH = 0.70            # 脚の外側の幅（Y）
UMPIRE_CHAIR_BAR = 0.05              # 脚・桟（角材）の太さ
UMPIRE_CHAIR_SEAT_DEPTH = 0.45       # 座面の奥行き（後ろの脚の内側から前へ）
UMPIRE_CHAIR_BACK_HEIGHT = 0.50      # 背もたれ（後ろの脚）の上端（座面から）
UMPIRE_CHAIR_BACK_GAP = 0.15         # 座面と背もたれの板の間
UMPIRE_CHAIR_ARM_HEIGHT = 0.25       # ひじ掛けの上の面（座面から）
UMPIRE_CHAIR_FOOTREST_DROP = 0.45    # 足置きの上の面（座面から下へ）
UMPIRE_CHAIR_RUNGS = 5               # 正面のはしごの段の数（地面から座面までを等分。段の間は約 0.3 m）
BALL_MACHINE_BODY = (0.50, 0.40, 0.40)    # 球出し機の本体（箱）：長さ（X）・幅（Y）・高さ
BALL_MACHINE_CLEARANCE = 0.08             # 地面から本体の下の面まで
BALL_MACHINE_WHEEL_DIAMETER = 0.20        # 後ろの車輪（左右 1 つずつ、本体の外）
BALL_MACHINE_WHEEL_WIDTH = 0.04
BALL_MACHINE_WHEEL_X = -0.12              # 車輪の軸の位置（本体の中心から後ろ）
BALL_MACHINE_FOOT = 0.06                  # 前の脚（角材。左右 1 つずつ）の太さ
BALL_MACHINE_HOPPER = (0.44, 0.36, 0.28)  # 上の受け箱（上が開いた箱）の外側：長さ・幅・高さ
BALL_MACHINE_HOPPER_WALL = 0.02
BALL_MACHINE_MUZZLE_HEIGHT = 0.33         # 発射口の中心の高さ（地面から）
BALL_MACHINE_MUZZLE_ACROSS = 0.10         # 発射口の筒（正八角形）の対辺の距離
BALL_MACHINE_MUZZLE_LENGTH = 0.06         # 本体の正面からの出
BALL_CAN_DIAMETER = 0.076            # ボールの缶（3 個入り）の胴の直径
BALL_CAN_HEIGHT = 0.216              # ふたを含む高さ
BALL_CAN_LID_HEIGHT = 0.012
BALL_CAN_LID_OVERHANG = 0.001        # ふたは胴より少し太い（側面を同じ面に乗せない）
BALL_CAN_SIDES = 16                  # 胴の分割（4 の倍数：軸の上に頂点を置き、直径を正確にする）
COOLER_BODY = (0.60, 0.36, 0.34)     # クーラーボックスの本体：長さ（X）・幅（Y）・高さ
COOLER_LID_HEIGHT = 0.06
COOLER_LID_OVERHANG = 0.01           # ふたは本体より少し大きい
COOLER_HANDLE = (0.03, 0.16, 0.03)   # 両端の取っ手（箱）：出・幅・厚さ
COOLER_HANDLE_Z = 0.24               # 取っ手の下の面の高さ
SERVE_TARGET_SIZE = 1.00             # サーブ練習の的（正方形の板）の 1 辺。ゲームの側で大きさを変えてよい
SERVE_TARGET_INNER = 0.50            # 中の正方形の 1 辺
SERVE_TARGET_THICKNESS = 0.005       # ラインの厚み（1 mm）より上に出す
SCOREBOARD_PANEL = (0.15, 2.40, 1.20)     # 電光掲示板の盤：厚さ（X）・幅（Y）・高さ
SCOREBOARD_BOTTOM = 1.00                  # 盤の下の辺の高さ
SCOREBOARD_POST = 0.10                    # 支柱（角柱、2 本）の太さ
SCOREBOARD_POST_Y = 0.90                  # 支柱の中心（左右）
SCOREBOARD_SCREEN_MARGIN = 0.08           # 画面のまわりの枠の幅
SCOREBOARD_SCREEN_DEPTH = 0.005           # 画面は盤の正面から少し出す

# ---- センターコート A のスタジアムの仮の形（A-STD-010。屋外・ハード・観客 約 15,000。コート局所座標。根拠は ART-01 §4.3） ----
STADIUM_RUNOFF_BACK = 8.23           # ベースラインの後ろの余白（スタジアムのコートの推奨。練習コートの 6.40 m より広い）
STADIUM_RUNOFF_SIDE = 4.57           # ダブルスのサイドラインの横の余白（同。練習コートは 3.66 m）
STADIUM_HALF_X = HALF_LENGTH + STADIUM_RUNOFF_BACK         # 看板の壁の内側の面（20.115）
STADIUM_HALF_Y = HALF_WIDTH_DOUBLES + STADIUM_RUNOFF_SIDE  # 同（10.055）
STADIUM_BARRIER_HEIGHT = 1.00        # 看板の壁（推定）
STADIUM_BARRIER_THICKNESS = 0.20
STADIUM_CORNER_GAP = 2.40            # 四隅の出入口の幅（看板の壁と下の層を対角線の向きに切る。推定）
STADIUM_ROW_DEPTH = 0.80             # 列の奥行き（FIFA：800 mm 以上）
STADIUM_SEAT_WIDTH = 0.50            # 1 席の幅（FIFA：席の中心の間隔 500 mm 以上）。席の数の見積もりにだけ使う
STADIUM_LOWER_FRONT = STADIUM_BARRIER_HEIGHT  # 下の層の最前列の床の高さ（看板の壁の上の面にそろえる）
STADIUM_LOWER_ROWS = 18
STADIUM_LOWER_RISER = 0.32           # 1 列の段の高さ（21.8°）
STADIUM_WALKWAY = 1.80               # 下の層と上の層の間の横の通路（FIFA：車いすの通る通路は 1.8 m 以上）
STADIUM_UPPER_ROWS = 18
STADIUM_UPPER_RISER = 0.48           # 31.0°（英国の Green Guide の上限 35° の中）
STADIUM_PARAPET_HEIGHT = 1.10        # 最後列の後ろの手すり壁（推定）
STADIUM_PARAPET_THICKNESS = 0.20
STADIUM_AISLE_WIDTH = 1.20           # 縦の通路（FIFA：通路は 1.2 m 以上）。上の層の四隅の対角線の通路も同じ幅
STADIUM_LOWER_AISLES_X = (6.0, 14.0)  # 縦の通路の中心（± の両方）：下の層の長い辺（x）・短い辺（y）
STADIUM_LOWER_AISLES_Y = (4.5,)
STADIUM_UPPER_AISLES_X = (4.0, 12.0, 20.0, 28.0)  # 上の層の長い辺（x）・短い辺（y）
STADIUM_UPPER_AISLES_Y = (5.0, 15.0)
