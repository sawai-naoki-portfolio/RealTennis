"""生成したアセットの寸法を検査する（許容 5 mm。T-CRT-001）。

1 項目 1 行で [OK]／[NG] を表示し、最後の行に全体の結果を出す。NG があっても止めずに、すべての項目を検査する。
期待値は tennis_blender/expected.py にある。
"""
import bpy
from mathutils import Vector

from tennis_blender import bpy_utils as bu
from tennis_blender import dims
from tennis_blender import expected as ex
from tennis_blender import materials as mat


def _fmt(v) -> str:
    return " x ".join(f"{c:.4f}" for c in v)


def _world_bounds(obj):
    pts = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    lo = tuple(min(p[i] for p in pts) for i in range(3))
    hi = tuple(max(p[i] for p in pts) for i in range(3))
    return lo, hi


def check_units():
    u = bpy.context.scene.unit_settings
    ok = u.system == 'METRIC' and ex.within(u.scale_length, 1.0, 1e-9)
    return ok, f"単位：{u.system}、Unit Scale {u.scale_length}（期待 METRIC、1.0）"


def check_bounds(name, lo_expected, hi_expected):
    """大きさと位置（バウンディングボックスの両端）、トランスフォームが単位であること。"""
    obj = bpy.data.objects.get(name)
    if obj is None:
        return False, f"{name}：見つからない（build_*.py を実行したか）"
    lo, hi = _world_bounds(obj)
    ok = all(ex.within(a, e) for a, e in zip(lo + hi, lo_expected + hi_expected))
    size = [h - l for l, h in zip(lo, hi)]
    size_expected = [h - l for l, h in zip(lo_expected, hi_expected)]
    msg = f"{name}：{_fmt(size)} m（期待 {_fmt(size_expected)}）、z {lo[2]:.4f}〜{hi[2]:.4f}"
    identity = bu.has_identity_transform(obj)
    if not identity:
        msg += "、トランスフォームが単位でない（頂点を直接その位置に作る）"
    return ok and identity, msg


def check_court_lines():
    """ラインの頂点の X・Y が、ITF の寸法から求めた縁の座標と過不足なく一致すること（T-CRT-001）。"""
    obj = bpy.data.objects.get("SM_Court_Lines")
    if obj is None:
        return False, "SM_Court_Lines の縁：見つからない"
    verts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    xs = sorted({round(v.x, 4) for v in verts})
    ys = sorted({round(v.y, 4) for v in verts})
    exp_x, exp_y = ex.court_line_edges()
    extra = ex.unmatched(xs, exp_x) + ex.unmatched(ys, exp_y)
    missing = ex.unmatched(exp_x, xs) + ex.unmatched(exp_y, ys)
    msg = f"SM_Court_Lines の縁：X {len(xs)} 種・Y {len(ys)} 種（期待 {len(exp_x)}・{len(exp_y)}）"
    if extra or missing:
        msg += f"、余分 {extra}、不足 {missing}"
    return not extra and not missing, msg


def check_face_count(name, expected):
    """面の数が分割の仕様どおりであること。"""
    obj = bpy.data.objects.get(name)
    if obj is None:
        return False, f"{name} の面の数：見つからない"
    count = len(obj.data.polygons)
    return count == expected, f"{name} の面の数：{count}（期待 {expected}）"


def check_net_top():
    """ネットの上辺の高さ（中央・スティック・ポスト）。"""
    obj = bpy.data.objects.get("SM_Net")
    if obj is None:
        return False, "SM_Net の上辺：見つからない"
    ok = True
    heights = []
    for y, h in ex.NET_TOP_HEIGHTS:
        zs = [v.co.z for v in obj.data.vertices if abs(v.co.y - y) < 1e-4]
        top = max(zs) if zs else float("nan")
        ok = ok and bool(zs) and ex.within(top, h)
        heights.append(f"y={y:+.3f}:{top:.3f}")
    return ok, f"SM_Net の上辺：{' '.join(heights)}"


def check_sway_weight(name):
    """頂点カラー R（揺れの重み）：0〜1、下辺（z = 0）とポスト際で 0、上辺の中央で 1。メッシュの座標で見る（トランスフォームは check_bounds で確認）。"""
    obj = bpy.data.objects.get(name)
    attr = obj.data.color_attributes.active_color if obj else None
    if attr is None:
        return False, f"{name} の揺れの重み：オブジェクトか頂点カラーがない"
    pairs = [(v.co, c.color[0]) for v, c in zip(obj.data.vertices, attr.data)]
    rs = [r for _, r in pairs]
    edge = [r for co, r in pairs if abs(co.z) < 1e-6 or abs(abs(co.y) - dims.POST_Y) < 1e-6]
    peak = max(r for co, r in pairs if abs(co.y) < 1e-6)
    ok = min(rs) >= 0.0 and max(rs) <= 1.0 and all(r < 1e-3 for r in edge) and abs(peak - 1.0) < 1e-3
    return ok, f"{name} の揺れの重み：R {min(rs):.3f}〜{max(rs):.3f}、下辺・ポスト際の最大 {max(edge, default=0):.3f}、中央の最大 {peak:.3f}"


def check_socket(name, parent_name, location):
    """ソケットの Empty：親、親の座標での位置（UE のソケットの位置になる）、回転・拡大が単位であること。"""
    obj = bpy.data.objects.get(name)
    if obj is None or obj.type != 'EMPTY' or obj.parent is None or obj.parent.name != parent_name:
        return False, f"{name}：{parent_name} の子の Empty が見つからない（build_*.py を実行したか）"
    local = obj.parent.matrix_world.inverted() @ obj.matrix_world
    loc = local.to_translation()
    ok = all(ex.within(a, e) for a, e in zip(loc, location)) and bu.is_identity(local.to_3x3())
    return ok, f"{name}：{_fmt(loc)} m（期待 {_fmt(location)}）、回転・拡大が単位 {bu.is_identity(local.to_3x3())}"


def check_racket_limits():
    """Appendix II の上限：全長・全幅（メッシュ）と、打球面の長さ・幅（ストリングの頂点の範囲。端はフレームに埋まる分だけ広い）。"""
    obj = bpy.data.objects.get("SM_Racket")
    slot = obj.data.materials.find(mat.RACKET_STRINGS) if obj else -1
    ids = {i for p in obj.data.polygons if p.material_index == slot for i in p.vertices} if slot >= 0 else set()
    if not ids:
        return False, "SM_Racket の Appendix II：オブジェクトかストリングのスロットがない"
    lo, hi = _world_bounds(obj)
    cos = [obj.data.vertices[i].co for i in ids]
    sizes = (hi[0] - lo[0], hi[1] - lo[1],
             max(c.x for c in cos) - min(c.x for c in cos), max(c.y for c in cos) - min(c.y for c in cos))
    ok = all(s <= limit for s, limit in zip(sizes, ex.RACKET_LIMITS))
    return ok, (f"SM_Racket の Appendix II：全長 {sizes[0]:.3f}・全幅 {sizes[1]:.3f}・打球面 {sizes[2]:.3f} x {sizes[3]:.3f} m"
                f"（上限 {' / '.join(f'{v:.3f}' for v in ex.RACKET_LIMITS)}）")


def check_tri_budgets():
    """三角形の数が ART-01 §1 の予算の中であること。"""
    results = []
    for label, (names, limit) in ex.TRI_BUDGETS.items():
        total = 0
        for name in names:
            obj = bpy.data.objects.get(name)
            if obj is not None:
                obj.data.calc_loop_triangles()
                total += len(obj.data.loop_triangles)
        results.append((0 < total <= limit, f"三角形の数（{label}）：{total}（上限 {limit}）"))
    return results


def main() -> None:
    bpy.context.view_layer.update()  # 同じ実行の中で作った直後でも matrix_world を最新にする
    results = [check_units()]
    results += [check_bounds(name, lo, hi) for name, (lo, hi) in ex.BOUNDS.items()]
    results.append(check_court_lines())
    results += [check_face_count(name, count) for name, count in ex.FACE_COUNTS.items()]
    results.append(check_net_top())
    results += [check_sway_weight(name) for name in ("SM_Net", "SM_NetBand")]
    results += [check_socket(name, parent, loc) for name, (parent, loc) in ex.SOCKETS.items()]
    results.append(check_racket_limits())
    results += check_tri_budgets()
    for ok, msg in results:
        bu.log(f"[{'OK' if ok else 'NG'}] {msg}")
    ng = sum(not ok for ok, _ in results)
    bu.log(f"validate_dimensions：{'OK' if ng == 0 else 'NG'}（{len(results) - ng}/{len(results)} 項目が OK、許容 {ex.TOL * 1000:.0f} mm）")


if __name__ == "__main__":
    main()
