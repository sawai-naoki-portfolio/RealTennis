"""dims.py の値が ITF 2026 の規定値（RES-01 §1・§2）と一致すること。"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tennis_blender import dims as d  # noqa: E402

TOL = 1e-9


class CourtDimsTest(unittest.TestCase):
    def test_court_size(self):
        self.assertAlmostEqual(2 * d.HALF_LENGTH, 23.77, delta=TOL)
        self.assertAlmostEqual(2 * d.HALF_WIDTH_SINGLES, 8.23, delta=TOL)
        self.assertAlmostEqual(2 * d.HALF_WIDTH_DOUBLES, 10.97, delta=TOL)
        self.assertAlmostEqual(d.SERVICE_LINE_X, 6.40, delta=TOL)

    def test_line_widths(self):
        self.assertAlmostEqual(d.LINE_WIDTH, 0.05, delta=TOL)
        self.assertAlmostEqual(d.BASELINE_WIDTH, 0.10, delta=TOL)
        self.assertAlmostEqual(d.CENTER_MARK_LENGTH, 0.10, delta=TOL)

    def test_runoff(self):
        self.assertAlmostEqual(d.RUNOFF_BACK, 6.40, delta=TOL)
        self.assertAlmostEqual(d.RUNOFF_SIDE, 3.66, delta=TOL)


class NetDimsTest(unittest.TestCase):
    def test_heights(self):
        self.assertAlmostEqual(d.NET_HEIGHT_CENTER, 0.914, delta=TOL)
        self.assertAlmostEqual(d.NET_HEIGHT_POST, 1.07, delta=TOL)
        self.assertAlmostEqual(d.STICK_HEIGHT, d.NET_HEIGHT_POST, delta=TOL)
        # ポストはネットコードから 2.5 cm 以内の突出（Rule 1）
        self.assertLessEqual(d.POST_HEIGHT - d.NET_HEIGHT_POST, 0.025 + TOL)

    def test_post_and_stick_positions(self):
        self.assertAlmostEqual(d.POST_Y, 6.399, delta=TOL)
        self.assertAlmostEqual(d.STICK_Y, 5.029, delta=TOL)

    def test_post_and_stick_sizes_within_limits(self):
        self.assertLessEqual(d.POST_DIAMETER, 0.15 + TOL)
        self.assertLessEqual(d.STICK_DIAMETER, 0.075 + TOL)
        self.assertLessEqual(d.CORD_DIAMETER, 0.008 + TOL)
        self.assertTrue(0.05 - TOL <= d.BAND_DEPTH <= 0.0635 + TOL)


class BallDimsTest(unittest.TestCase):
    def test_diameters_within_appendix_i(self):
        self.assertTrue(0.0654 <= 2 * d.BALL_RADIUS <= 0.0686)
        self.assertTrue(0.0700 <= 2 * d.BALL_RADIUS_LARGE <= 0.0730)


class RacketDimsTest(unittest.TestCase):
    def test_spec_values(self):
        """ART-01 A-RKT-001：全長 68.6 cm、フェイス約 32 × 25 cm。"""
        self.assertAlmostEqual(d.RACKET_LENGTH, 0.686, delta=TOL)
        self.assertAlmostEqual(d.RACKET_FACE_LENGTH, 0.32, delta=TOL)
        self.assertAlmostEqual(d.RACKET_FACE_WIDTH, 0.25, delta=TOL)

    def test_within_appendix_ii(self):
        """全長 ≤ 73.7 cm、全幅 ≤ 31.7 cm、打球面 ≤ 39.4 × 29.2 cm（RES-01 §3）。"""
        self.assertLessEqual(d.RACKET_LENGTH, 0.737)
        self.assertLessEqual(d.RACKET_FACE_WIDTH + 2 * d.RACKET_FRAME_WIDTH, 0.317)
        self.assertLessEqual(d.RACKET_FACE_LENGTH, 0.394)
        self.assertLessEqual(d.RACKET_FACE_WIDTH, 0.292)

    def test_parts_fit_in_order(self):
        """グリップ → シャフト → ヘッドの順に並び、ソケットがそれぞれの部品の上にある。"""
        head_bottom = d.RACKET_LENGTH - d.RACKET_FACE_LENGTH - 2 * d.RACKET_FRAME_WIDTH
        self.assertLess(d.RACKET_BUTT_LENGTH, d.RACKET_GRIP_SOCKET_X)
        self.assertLess(d.RACKET_GRIP_SOCKET_X, d.RACKET_GRIP_LENGTH)
        self.assertLess(d.RACKET_GRIP_LENGTH, d.RACKET_THROAT_X)
        self.assertLess(d.RACKET_THROAT_X, head_bottom)
        self.assertLess(d.RACKET_SWEETSPOT_OFFSET, d.RACKET_FACE_LENGTH / 2)
        self.assertEqual(d.RACKET_RING_SEGMENTS % 4, 0)
        self.assertLess(d.RACKET_OVERLAP_INSET, d.RACKET_STRING_GAUGE / 2)


if __name__ == "__main__":
    unittest.main()
