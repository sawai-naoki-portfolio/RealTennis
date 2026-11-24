"""RES-02（ボール物理の調査報告）に書いた数値が、参照シミュレーションで再現されることを確かめる。

実行：python -I -m unittest discover -s Tools/Reference/tests -p "test_*.py"
数値を変えるときは、RES-02 と DATA-02 の表も同じ PR で更新する。
"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ballsim_reference as sim  # noqa: E402

# 値, 許容誤差
SCALARS = {
    "magnus_az": (-5.96, 0.05), "kd": (0.02033, 0.00005), "terminal": (21.97, 0.05),
    "drop_v_0.76": (6.88, 0.02),
    "drop_h_0.73": (125.4, 0.5), "drop_h_0.75": (132.1, 0.5), "drop_h_0.76": (135.6, 0.5),
    "drop_h_0.77": (139.1, 0.5), "drop_h_0.78": (142.6, 0.5),
}
CPR = {  # 名前: (μ 実効, CPR, 区分, 滑り/グリップ)
    "EXISTING Hard": (0.600, 49.0, 5, "slide"), "EXISTING Clay": (0.719, 41.6, 4, "grip"),
    "EXISTING Grass": (0.450, 71.5, 5, "slide"), "EXISTING Carpet": (0.500, 62.0, 5, "slide"),
    "PROPOSED Clay": (0.693, 26.2, 1, "grip"), "PROPOSED Hard-Medium": (0.630, 38.5, 3, "slide"),
    "PROPOSED Hard-MedFast": (0.600, 43.0, 4, "slide"), "PROPOSED Grass": (0.620, 51.5, 5, "slide"),
    "PROPOSED Indoor-Fast": (0.550, 51.0, 5, "slide"),
}
BENCH = {  # 名前: (項目 → (値, 許容誤差), 滑ったか)
    "rally_fh": ({"elev": (7.95, 0.05), "net": (0.53, 0.01), "apex": (1.58, 0.01), "t1": (0.754, 0.005)}, False),
    "heavy_fh": ({"elev": (9.52, 0.05), "net": (0.68, 0.01), "t1": (0.783, 0.005)}, False),
    "flat_fh": ({"elev": (3.20, 0.05), "net": (0.13, 0.01), "t1": (0.608, 0.005)}, True),
    "bh_slice": ({"elev": (4.82, 0.05), "net": (0.17, 0.01), "t1": (0.937, 0.005)}, True),
    "serve_flat": ({"elev": (-5.06, 0.05), "net": (0.37, 0.01), "t1": (0.425, 0.005), "t_ret": (0.712, 0.005)}, True),
    "serve_kick": ({"elev": (-1.41, 0.05), "net": (0.50, 0.01), "t1": (0.521, 0.005), "h_ret": (1.40, 0.01)}, False),
}
DRAG = {0.50: (0.69, 0.812), 0.55: (0.72, 0.829), 0.60: (0.78, 0.846)}      # C_d: (ネットの上の余裕, 飛行時間)
DENSITY = {1.21: (0.03, 0.829), 1.058: (1.26, 0.858), 0.957: (2.27, 0.883)}  # ρ: (着地のずれ, 飛行時間)


class ReferenceSimulationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = sim.compute()

    def test_scalars(self):
        for key, (expected, tol) in SCALARS.items():
            with self.subTest(key=key):
                self.assertAlmostEqual(self.r[key], expected, delta=tol)

    def test_court_pace_rating(self):
        for name, (mu, cpr, cat, mode) in CPR.items():
            got_mu, got_cpr, got_cat, got_mode = self.r[f"cpr_{name}"]
            with self.subTest(surface=name):
                self.assertAlmostEqual(got_mu, mu, delta=0.001)
                self.assertAlmostEqual(got_cpr, cpr, delta=0.1)
                self.assertEqual((got_cat, got_mode), (cat, mode))

    def test_benchmark_shots(self):
        for name, (items, slid) in BENCH.items():
            b = self.r[f"bench_{name}"]
            for key, (expected, tol) in items.items():
                with self.subTest(shot=name, key=key):
                    self.assertAlmostEqual(b[key], expected, delta=tol)
            with self.subTest(shot=name, key="slid"):
                self.assertEqual(b["slid"], slid)

    def test_drag_sensitivity(self):
        for cd, (net, flight) in DRAG.items():
            with self.subTest(cd=cd):
                self.assertAlmostEqual(self.r[f"cd_{cd}"][0], net, delta=0.01)
                self.assertAlmostEqual(self.r[f"cd_{cd}"][1], flight, delta=0.003)

    def test_air_density(self):
        for rho, (dx, flight) in DENSITY.items():
            with self.subTest(rho=rho):
                self.assertAlmostEqual(self.r[f"rho_{rho}"][0], dx, delta=0.03)
                self.assertAlmostEqual(self.r[f"rho_{rho}"][1], flight, delta=0.003)


if __name__ == "__main__":
    unittest.main()
