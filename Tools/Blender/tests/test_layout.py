"""layout.py：コートのラインの位置が ITF の寸法（T-CRT-001、±5 mm）どおりで、互いに重ならないこと。"""
import itertools
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tennis_blender import layout  # noqa: E402
from tennis_blender.layout import Rect  # noqa: E402

TOL = 1e-9


def _by_name():
    return {r.name: r for r in layout.court_lines()}


def _signed_area(verts, face):
    pts = [verts[i] for i in face]
    return 0.5 * sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]))


class RectTest(unittest.TestCase):
    def test_rejects_empty_rect(self):
        with self.assertRaises(ValueError):
            Rect("bad", 1.0, 1.0, 0.0, 1.0)
        with self.assertRaises(ValueError):
            Rect("bad", 0.0, 1.0, 2.0, 1.0)


class CourtOutlineTest(unittest.TestCase):
    def test_inner_is_doubles_court(self):
        r = layout.court_inner()
        self.assertAlmostEqual(r.size_x, 23.77, delta=TOL)
        self.assertAlmostEqual(r.size_y, 10.97, delta=TOL)
        self.assertAlmostEqual(r.x_min + r.x_max, 0.0, delta=TOL)

    def test_surface_includes_runoff(self):
        r = layout.court_surface()
        self.assertAlmostEqual(r.size_x, 23.77 + 2 * 6.40, delta=TOL)
        self.assertAlmostEqual(r.size_y, 10.97 + 2 * 3.66, delta=TOL)


class CourtLinesTest(unittest.TestCase):
    def test_line_count(self):
        self.assertEqual(len(layout.court_lines()), 11)

    def test_baseline(self):
        r = _by_name()["baseline+"]
        self.assertAlmostEqual(r.x_max, 11.885, delta=TOL)   # 外側の縁がコートの端
        self.assertAlmostEqual(r.x_min, 11.785, delta=TOL)   # 幅 10 cm
        self.assertAlmostEqual(r.y_max, 5.485, delta=TOL)
        self.assertAlmostEqual(_by_name()["baseline-"].x_min, -11.885, delta=TOL)

    def test_sidelines(self):
        lines = _by_name()
        self.assertAlmostEqual(lines["sideline_doubles+"].y_max, 5.485, delta=TOL)
        self.assertAlmostEqual(lines["sideline_doubles+"].y_min, 5.435, delta=TOL)
        self.assertAlmostEqual(lines["sideline_singles+"].y_max, 4.115, delta=TOL)
        self.assertAlmostEqual(lines["sideline_singles+"].y_min, 4.065, delta=TOL)
        self.assertAlmostEqual(lines["sideline_singles-"].y_min, -4.115, delta=TOL)
        # 長さはベースラインの内側の縁まで
        self.assertAlmostEqual(lines["sideline_singles+"].x_max, 11.785, delta=TOL)

    def test_service_lines(self):
        r = _by_name()["service+"]
        self.assertAlmostEqual(r.x_max, 6.40, delta=TOL)     # 外側の縁がネットから 6.40 m
        self.assertAlmostEqual(r.x_min, 6.35, delta=TOL)
        self.assertAlmostEqual(r.y_max, 4.065, delta=TOL)    # シングルスのサイドラインの内側の縁まで
        self.assertAlmostEqual(_by_name()["service-"].x_min, -6.40, delta=TOL)

    def test_center_service_line(self):
        r = _by_name()["center_service"]
        self.assertAlmostEqual(r.size_y, 0.05, delta=TOL)
        self.assertAlmostEqual(r.y_min + r.y_max, 0.0, delta=TOL)
        self.assertAlmostEqual(r.x_max, 6.35, delta=TOL)     # サービスラインの内側の縁まで

    def test_center_marks(self):
        r = _by_name()["center_mark+"]
        self.assertAlmostEqual(r.size_x, 0.10, delta=TOL)    # 長さ 10 cm
        self.assertAlmostEqual(r.size_y, 0.05, delta=TOL)    # 幅 5 cm
        self.assertAlmostEqual(r.x_max, 11.785, delta=TOL)   # ベースラインからコートの内側へ
        self.assertAlmostEqual(_by_name()["center_mark-"].x_min, -11.785, delta=TOL)

    def test_lines_do_not_overlap(self):
        for a, b in itertools.combinations(layout.court_lines(), 2):
            ox = min(a.x_max, b.x_max) - max(a.x_min, b.x_min)
            oy = min(a.y_max, b.y_max) - max(a.y_min, b.y_min)
            self.assertFalse(ox > TOL and oy > TOL, f"{a.name} と {b.name} が重なる")

    def test_lines_inside_inner(self):
        inner = layout.court_inner()
        for r in layout.court_lines():
            self.assertGreaterEqual(r.x_min, inner.x_min - TOL, r.name)
            self.assertLessEqual(r.x_max, inner.x_max + TOL, r.name)
            self.assertGreaterEqual(r.y_min, inner.y_min - TOL, r.name)
            self.assertLessEqual(r.y_max, inner.y_max + TOL, r.name)

    def test_lines_symmetric(self):
        lines = _by_name()
        for name, r in lines.items():
            if name.endswith("+"):
                m = lines[name[:-1] + "-"]
                if name.startswith("sideline"):
                    self.assertAlmostEqual(r.y_max, -m.y_min, delta=TOL, msg=name)
                else:
                    self.assertAlmostEqual(r.x_max, -m.x_min, delta=TOL, msg=name)


class QuadsTest(unittest.TestCase):
    def test_rects_to_quads(self):
        rects = layout.court_lines()
        verts, faces = layout.rects_to_quads(rects)
        self.assertEqual(len(verts), 4 * len(rects))
        self.assertEqual(len(faces), len(rects))
        for face, r in zip(faces, rects):
            self.assertAlmostEqual(_signed_area(verts, face), r.size_x * r.size_y, delta=1e-9)

    def test_frame_to_quads_covers_ring_with_up_normals(self):
        outer, inner = layout.court_surface(), layout.court_inner()
        verts, faces = layout.frame_to_quads(outer, inner)
        self.assertEqual((len(verts), len(faces)), (8, 4))
        areas = [_signed_area(verts, f) for f in faces]
        self.assertTrue(all(a > 0 for a in areas))          # 法線は +Z
        expected = outer.size_x * outer.size_y - inner.size_x * inner.size_y
        self.assertAlmostEqual(sum(areas), expected, delta=1e-6)

    def test_frame_rejects_inner_outside(self):
        with self.assertRaises(ValueError):
            layout.frame_to_quads(layout.court_inner(), layout.court_surface())


if __name__ == "__main__":
    unittest.main()
