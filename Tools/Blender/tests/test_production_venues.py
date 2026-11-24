"""Regulation markings, venue capacity, material slots and seat visibility."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tennis_blender import venues


class ProductionVenuesTest(unittest.TestCase):
    def test_decks_are_disjoint_and_cover_full_footprint(self):
        for code in ['Practice', *venues.VENUES]:
            width, length = (18.29, 36.57) if code == 'Practice' else (20.11, 40.23)
            area = 0
            for name, (vertices, faces) in venues.deck(code):
                self.assertIn(name, venues.palette(code))
                for face in faces:
                    a, b, c, d = (vertices[i] for i in face)
                    self.assertEqual((a[2], b[2], c[2], d[2]), (0, 0, 0, 0))
                    self.assertLess(a[0], b[0])
                    self.assertLess(b[1], c[1])
                    area += (b[0]-a[0]) * (c[1]-b[1])
            self.assertAlmostEqual(area, width*length, places=7)

    def test_seat_capacity_normals_and_lods(self):
        for code, venue in venues.VENUES.items():
            fine = venues.seats(code)
            coarse = venues.seats(code, 1)
            self.assertEqual(sum(len(v) for _, (v, _) in fine), venue.capacity*12)
            self.assertLess(sum(len(v) for _, (v, _) in coarse), venue.capacity*4)
            for name, (vertices, faces) in fine:
                self.assertIn(name, venues.palette(code))
                a,b,c = [vertices[i] for i in faces[0][:3]]
                self.assertGreater((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]), 0)
