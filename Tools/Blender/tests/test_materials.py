"""materials.py の単体テスト：すべてのスロットに確認用の色があり、ラインとコートが見分けられること。"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tennis_blender import materials as m  # noqa: E402

SLOTS = [m.COURT_OUT, m.COURT_IN, m.COURT_LINE, m.NET, m.NET_BAND, m.NET_POST, m.BALL,
         m.RACKET_FRAME, m.RACKET_GRIP, m.RACKET_STRINGS,
         m.FENCE_POST, m.FENCE_NET, m.BENCH, m.LIGHT_POLE, m.LIGHT_HEAD,
         m.PROP_FRAME, m.PROP_DARK, m.PROP_ACCENT, m.PROP_LIGHT, m.PROP_SCREEN,
         m.PLAYER_SKIN_A, m.PLAYER_SKIN_B, m.PLAYER_HAIR_A, m.PLAYER_HAIR_B, m.PLAYER_TOP_A, m.PLAYER_TOP_B,
         m.PLAYER_BAND_A, m.PLAYER_BAND_B, m.PLAYER_WHITE, m.PLAYER_SHOE, m.PLAYER_SOLE, m.PLAYER_LIPS,
         m.PLAYER_EYE, m.PLAYER_IRIS,
         m.STADIUM_CONCRETE, m.STADIUM_SEAT, m.STADIUM_AISLE, m.STADIUM_SIGNAGE]


def luminance(rgb):
    r, g, b = rgb
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


class MaterialsTest(unittest.TestCase):
    def test_every_slot_has_a_preview_color(self):
        self.assertEqual(sorted(m.PREVIEW_COLORS), sorted(SLOTS))

    def test_colors_are_in_unit_range(self):
        for name, rgb in m.PREVIEW_COLORS.items():
            with self.subTest(slot=name):
                self.assertEqual(len(rgb), 3)
                self.assertTrue(all(0.0 <= c <= 1.0 for c in rgb))

    def test_slot_names_follow_naming_rule(self):
        for name in SLOTS:
            with self.subTest(slot=name):
                self.assertTrue(name.startswith("M_"))

    def test_lines_stand_out_from_the_court(self):
        line = luminance(m.PREVIEW_COLORS[m.COURT_LINE])
        for surface in (m.COURT_IN, m.COURT_OUT):
            with self.subTest(surface=surface):
                self.assertGreater(line - luminance(m.PREVIEW_COLORS[surface]), 0.5)

    def test_racket_parts_stand_out(self):
        """ストリングが、フレームとグリップから見分けられる。"""
        strings = luminance(m.PREVIEW_COLORS[m.RACKET_STRINGS])
        for part in (m.RACKET_FRAME, m.RACKET_GRIP):
            with self.subTest(part=part):
                self.assertGreater(strings - luminance(m.PREVIEW_COLORS[part]), 0.5)

    def test_stadium_aisles_stand_out(self):
        """縦の通路の帯が、観客席の段と壁から見分けられる。"""
        aisle = luminance(m.PREVIEW_COLORS[m.STADIUM_AISLE])
        for part in (m.STADIUM_SEAT, m.STADIUM_CONCRETE):
            with self.subTest(part=part):
                self.assertGreater(aisle - luminance(m.PREVIEW_COLORS[part]), 0.25)


if __name__ == "__main__":
    unittest.main()
