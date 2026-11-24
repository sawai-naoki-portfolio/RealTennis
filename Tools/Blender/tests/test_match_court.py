"""Centro court: painted surface area, access to rows, LOD slot stability."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tennis_blender import broadcast_camera, dims as d, layout, match_court as kit


class MatchCourtTest(unittest.TestCase):
    def test_inlaid_lines_preserve_regulation_area_and_do_not_overlap(self):
        areas={}
        for slot,(verts,faces) in kit.deck():
            areas[slot]=sum((verts[f[1]][0]-verts[f[0]][0])*(verts[f[2]][1]-verts[f[1]][1]) for f in faces)
            self.assertTrue(all(z==0 for _,_,z in verts))
        self.assertAlmostEqual(areas['M_A_Paint'],sum(r.size_x*r.size_y for r in layout.court_lines()))
        self.assertAlmostEqual(sum(areas.values()),4*d.STADIUM_HALF_X*d.STADIUM_HALF_Y)

    def test_each_seat_run_stays_clear_of_aisles(self):
        capacity=0
        for n,_,r,_,mid,count,_ in kit.banks():
            upper=(r-(d.STADIUM_HALF_Y if n[0]==0 else d.STADIUM_HALF_X))>kit.stadium.UPPER_OFFSET
            if n[0]==0:
                aisles=d.STADIUM_UPPER_AISLES_X if upper else d.STADIUM_LOWER_AISLES_X
            else:
                aisles=d.STADIUM_UPPER_AISLES_Y if upper else d.STADIUM_LOWER_AISLES_Y
            for c in aisles:
                self.assertGreaterEqual(abs(abs(mid)-c)-count*d.STADIUM_SEAT_WIDTH/2,d.STADIUM_AISLE_WIDTH/2-1e-8)
            capacity+=count
        self.assertEqual(capacity,14804)

    def test_prop_lods_have_stable_materials_and_fewer_triangles(self):
        for make in (kit.player_chair,kit.drinks_table,broadcast_camera.camera):
            base=make(0)
            names=[n for n,_ in base]
            last=None
            for lod in range(3):
                slots=make(lod)
                self.assertEqual([n for n,_ in slots],names)
                triangles=sum(len(f)-2 for _,(_,faces) in slots for f in faces)
                if last is not None:
                    self.assertLess(triangles,last)
                last=triangles

