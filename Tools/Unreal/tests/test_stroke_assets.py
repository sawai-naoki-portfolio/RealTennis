"""stroke_assets.py の単体テスト（unreal なしで動く）。"""
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tennis_toolset import stroke_assets as sa  # noqa: E402
from tennis_toolset.import_rules import plan_import  # noqa: E402

NAME = "AS_FH_Mid_Drive_Square"
GOOD = {"name": NAME, "fps": 30, "frames": 52, "impact_time": 23 / 30, "recover_time": 38 / 30}


def text(**changes) -> str:
    data = dict(GOOD)
    data.update(changes)
    return json.dumps(data)


MID_BAND_MIN = 0.7          # m（DD-03 §5.1 の Mid の帯の下限：TennisStrokeSelect）
MID_DRIVE_HIT_HEIGHT = 1.0  # m（焼きの入力の打点の高さ：TennisStrokeSelection::MidDriveHitHeight。DD-03 §4.2）


class StrokeAssetsTest(unittest.TestCase):
    def test_chooser_lives_with_the_strokes(self):
        # ARCH-02 §4：Data/Strokes に CHT_StrokeSelect と DA_Stroke_* を置く
        self.assertEqual(sa.STROKE_FOLDER, "/Game/Tennis/Data/Strokes")
        self.assertEqual(sa.STROKE_CHOOSER_PATH, "/Game/Tennis/Data/Strokes/CHT_StrokeSelect")
        self.assertTrue(sa.STROKE_CHOOSER_NAME.startswith("CHT_"))

    def test_placeholders_are_a_forehand_and_a_backhand_with_their_dip_variants(self):
        # DD-03 §5.3 の高さの変種：今の 2 本と、低く打つ変種 Dip の 2 本
        self.assertEqual([s.side for s in sa.PLACEHOLDER_STROKES], ["FOREHAND", "BACKHAND", "FOREHAND", "BACKHAND"])
        self.assertEqual([s.animation for s in sa.PLACEHOLDER_STROKES[2:]],
                         [s.animation + "_Dip" for s in sa.PLACEHOLDER_STROKES[:2]])
        for stroke in sa.PLACEHOLDER_STROKES:
            with self.subTest(stroke=stroke.animation):
                self.assertTrue(stroke.animation.startswith("AS_"))
                self.assertEqual(stroke.montage, "AM_" + stroke.animation[3:])       # ART-02 §1 の命名
                self.assertEqual(stroke.data, "DA_Stroke_" + stroke.animation[3:])
                plan_import(stroke.animation + ".fbx", "C:/proj/Export")             # 取り込みの規則がある

    def test_placeholders_use_the_release_lead_times_of_the_controls_spec(self):
        # 操作仕様 §4.1 の表：フォアハンド（ドライブ）0.12 s、バックハンド（片手。BH1 は片手：ART-02）0.14 s
        leads = {s.animation: s.release_lead_time for s in sa.PLACEHOLDER_STROKES}
        self.assertEqual(leads, {"AS_FH_Mid_Drive_Square": 0.12, "AS_BH1_Mid_Drive_Square": 0.14,
                                 "AS_FH_Mid_Drive_Square_Dip": 0.12, "AS_BH1_Mid_Drive_Square_Dip": 0.14})

    def test_placeholders_use_the_charge_times_of_the_controls_spec(self):
        # 操作仕様 §4.1 の表：フォアハンド（ドライブ）0.70 s、バックハンド（片手）0.80 s
        charges = {s.animation: s.charge_time for s in sa.PLACEHOLDER_STROKES}
        self.assertEqual(charges, {"AS_FH_Mid_Drive_Square": 0.70, "AS_BH1_Mid_Drive_Square": 0.80,
                                   "AS_FH_Mid_Drive_Square_Dip": 0.70, "AS_BH1_Mid_Drive_Square_Dip": 0.80})

    def test_dip_variants_split_the_mid_band_at_one_boundary_per_side(self):
        # DD-03 §5.3：Dip は [下限なし, 境界)、今の行は [境界, 上限なし)。片側だけの範囲で、同じ側は同じ境界を共有する（すき間・重なりがない）
        for upper, dip in zip(sa.PLACEHOLDER_STROKES[:2], sa.PLACEHOLDER_STROKES[2:]):
            with self.subTest(side=upper.side):
                self.assertEqual(upper.side, dip.side)
                self.assertIsNone(upper.hit_height_max)
                self.assertIsNone(dip.hit_height_min)
                self.assertEqual(upper.hit_height_min, dip.hit_height_max)
                self.assertTrue(MID_BAND_MIN < upper.hit_height_min < MID_DRIVE_HIT_HEIGHT)  # Mid の帯の中で、焼きの入力は上の行
        self.assertEqual((sa.FOREHAND_DIP_BOUNDARY, sa.BACKHAND_DIP_BOUNDARY), (0.93, 0.97))  # パラメータ表 §4.3


class ParseNotifyTimesTest(unittest.TestCase):
    def test_good_file_is_read(self):
        times = sa.parse_notify_times(text(), NAME)
        self.assertAlmostEqual(times.impact_time, 23 / 30)
        self.assertAlmostEqual(times.recover_time, 38 / 30)
        self.assertAlmostEqual(times.length, 52 / 30)

    def test_recover_may_be_the_last_frame(self):
        self.assertAlmostEqual(sa.parse_notify_times(text(recover_time=52 / 30), NAME).recover_time, 52 / 30)

    def test_bad_shapes_are_rejected(self):
        for bad in ("", "[]", "{", "null", '"x"', json.dumps({"name": NAME})):
            with self.subTest(bad=bad), self.assertRaises(sa.NotifyTimesError):
                sa.parse_notify_times(bad, NAME)

    def test_wrong_name_fps_and_frames_are_rejected(self):
        for changes in ({"name": "AS_Other"}, {"name": "../" + NAME}, {"fps": 60}, {"frames": 0}, {"frames": 601},
                        {"frames": 52.5}, {"frames": True}, {"frames": "52"}):
            with self.subTest(changes=changes), self.assertRaises(sa.NotifyTimesError):
                sa.parse_notify_times(text(**changes), NAME)

    def test_times_out_of_order_or_not_finite_are_rejected(self):
        for changes in ({"impact_time": 0.0}, {"impact_time": -0.1}, {"impact_time": 1.5}, {"recover_time": 2.0},
                        {"impact_time": "0.5"}, {"impact_time": True}, {"recover_time": None}):
            with self.subTest(changes=changes), self.assertRaises(sa.NotifyTimesError):
                sa.parse_notify_times(text(**changes), NAME)
        for bad in ('{"name": "%s", "fps": 30, "frames": 52, "impact_time": NaN, "recover_time": 1.2}' % NAME,
                    '{"name": "%s", "fps": 30, "frames": 52, "impact_time": 0.5, "recover_time": Infinity}' % NAME):
            with self.subTest(bad=bad), self.assertRaises(sa.NotifyTimesError):
                sa.parse_notify_times(bad, NAME)

    def test_files_are_read_up_to_the_limit(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, NAME + ".json")
            with open(path, "w", encoding="utf-8") as file:
                file.write(text())
            self.assertAlmostEqual(sa.read_notify_times(path, NAME).impact_time, 23 / 30)
            for content in (text(extra="x" * 10000).encode("utf-8"), b'{"name": "\xff"}'):
                with open(path, "wb") as file:
                    file.write(content)
                with self.subTest(content=content[:20]), self.assertRaises(sa.NotifyTimesError):
                    sa.read_notify_times(path, NAME)

    def test_huge_numbers_and_deep_nesting_raise_the_same_error(self):
        # float に直せない整数・深すぎる入れ子も、ほかの例外でなく NotifyTimesError で止める
        for bad in (text(impact_time=10 ** 400), text(recover_time=-(10 ** 400)), "[" * 2000 + "]" * 2000):
            with self.subTest(bad=bad[:30]), self.assertRaises(sa.NotifyTimesError):
                sa.parse_notify_times(bad, NAME)

    def test_large_file_is_rejected(self):
        with self.assertRaises(sa.NotifyTimesError):
            sa.parse_notify_times(text(extra="x" * 5000), NAME)


if __name__ == "__main__":
    unittest.main()
