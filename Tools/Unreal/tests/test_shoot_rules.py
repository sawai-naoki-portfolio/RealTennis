"""shoot_rules.py の単体テスト：shoot_ball が受け付ける入力と、受け付けない入力。"""
import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tennis_toolset.shoot_rules import MAX_TARGET_X_M, validate_shoot_ball  # noqa: E402


class ShootRulesTest(unittest.TestCase):
    def test_accepts_a_shot_type_name_and_a_target_on_the_court(self):
        validate_shoot_ball("DA_Shot_Topspin", 9.0, -2.5)
        validate_shoot_ball("DA_Shot_ServeKick", MAX_TARGET_X_M, 0.0)

    def test_rejects_paths_and_other_names(self):
        for name in ("", "Topspin", "DA_Shot_", "/Game/Tennis/Data/Shots/DA_Shot_Lob", "DA_Shot_Lob.DA_Shot_Lob", "DA_Ball_Type2"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    validate_shoot_ball(name, 9.0, 0.0)

    def test_rejects_targets_off_the_range_or_not_finite(self):
        for x, y in ((MAX_TARGET_X_M + 0.1, 0.0), (0.0, -20.1), (math.nan, 0.0), (0.0, math.inf)):
            with self.subTest(x=x, y=y):
                with self.assertRaises(ValueError):
                    validate_shoot_ball("DA_Shot_Flat", x, y)


if __name__ == "__main__":
    unittest.main()
