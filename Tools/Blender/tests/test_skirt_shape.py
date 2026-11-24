"""skirt_shape.py：スカートが体から離れて下へ広がること、ウェイト。"""
import math
import sys
import unittest
from pathlib import Path

sys.path[:0] = [str(Path(__file__).resolve().parents[1]), str(Path(__file__).resolve().parent)]

from tennis_blender import skirt_shape as sk  # noqa: E402


class SkirtTest(unittest.TestCase):
    def test_skirt_clears_the_body_and_widens_downward(self):
        body = [(0.15 * math.cos(a / 10), 0.12 * math.sin(a / 10), z / 100) for a in range(63) for z in range(70, 102, 2)]
        rings = sk.skirt_rings(body, (0.0, 0.0), 1.0, 0.75, rings=6, segments=32, clearance=0.01, flare=0.07, pleats=8)
        radius = [[math.hypot(p[0], p[1]) for p in r] for r in rings]
        self.assertTrue(all(r >= 0.12 + 0.01 - 1e-9 for r in radius[0]))
        self.assertTrue(all(radius[k + 1][j] >= radius[k][j] - 1e-9 for k in range(5) for j in range(32)))

    def test_skirt_weights(self):
        self.assertEqual(sk.skirt_weights(0.1, 0.0), [("pelvis", 1.0)])
        for x in (-0.1, 0.0, 0.1):
            w = dict(sk.skirt_weights(x, 1.0))
            self.assertAlmostEqual(sum(w.values()), 1.0)
            self.assertAlmostEqual(w["pelvis"], 0.4)
        self.assertNotIn("thigh_r", dict(sk.skirt_weights(0.1, 1.0)))  # 選手の左（+X）は左の太もも


if __name__ == "__main__":
    unittest.main()
