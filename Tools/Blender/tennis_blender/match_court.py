"""Center Court A: match deck, aisle-aware seat banks and player equipment (metres)."""
from __future__ import annotations

import math

from . import dims as d, layout, stadium_a as stadium
from .primitives import box, join, octagon, prism, regular_polygon

# Each entry is (slot, linear RGB). Keep the distant seat bands in the same slot order.
PALETTE = {
    "M_A_Acrylic": (.024, .135, .31), "M_A_Alley": (.035, .168, .34),
    "M_A_Runoff": (.037, .205, .19), "M_A_Paint": (.89, .92, .87),
    "M_A_DeepSea": (.025, .094, .138), "M_A_Lagoon": (.035, .20, .225),
    "M_A_Tide": (.075, .29, .30), "M_A_Gold": (.84, .54, .145),
    "M_A_Timber": (.48, .27, .12), "M_A_Frame": (.095, .125, .13),
    "M_A_Textile": (.76, .80, .72), "M_A_Bottle": (.14, .47, .42),
}
DECK = "SM_Court_A_MatchDeck"
SEATS = "SM_Stadium_A_SeatBanks"
CHAIR = "SM_Prop_A_PlayersChair"
TABLE = "SM_Prop_A_DrinksTable"


def deck():
    """Tile on exact line boundaries. One coplanar face per cell, no layered decals."""
    lines = layout.court_lines()
    hx, hy = d.STADIUM_HALF_X, d.STADIUM_HALF_Y
    xs = sorted({-hx, hx, *(v for r in lines for v in (r.x_min, r.x_max))})
    ys = sorted({-hy, hy, *(v for r in lines for v in (r.y_min, r.y_max))})
    pieces = {}
    for x0, x1 in zip(xs, xs[1:]):
        for y0, y1 in zip(ys, ys[1:]):
            x, y = (x0+x1)/2, (y0+y1)/2
            if any(r.x_min < x < r.x_max and r.y_min < y < r.y_max for r in lines):
                slot = "M_A_Paint"
            elif abs(x) > d.HALF_LENGTH or abs(y) > d.HALF_WIDTH_DOUBLES:
                slot = "M_A_Runoff"
            elif abs(y) > d.HALF_WIDTH_SINGLES:
                slot = "M_A_Alley"
            else:
                slot = "M_A_Acrylic"
            cell = ([(x0,y0,0), (x1,y0,0), (x1,y1,0), (x0,y1,0)], [(0,1,2,3)])
            pieces.setdefault(slot, []).append(cell)
    return [(slot, join(cells)) for slot, cells in pieces.items()]


def banks():
    """Yield each uninterrupted run, with equal margins at the two aisle edges."""
    tiers = [
        (stadium.LOWER_OFFSET, d.STADIUM_LOWER_FRONT, d.STADIUM_LOWER_ROWS, d.STADIUM_LOWER_RISER,
         d.STADIUM_CORNER_GAP, d.STADIUM_LOWER_AISLES_X, d.STADIUM_LOWER_AISLES_Y),
        (stadium.UPPER_OFFSET, stadium.WALKWAY_Z+d.STADIUM_UPPER_RISER, d.STADIUM_UPPER_ROWS,
         d.STADIUM_UPPER_RISER, d.STADIUM_AISLE_WIDTH, d.STADIUM_UPPER_AISLES_X, d.STADIUM_UPPER_AISLES_Y),
    ]
    for depth, first, rows, rise, cut, aisles_x, aisles_y in tiers:
        for row in range(rows):
            u = depth + (row+.5)*d.STADIUM_ROW_DEPTH
            for normal, tangent in stadium.SIDES:
                along_x = normal[0] == 0
                half = (d.STADIUM_HALF_X if along_x else d.STADIUM_HALF_Y) + u
                radius = (d.STADIUM_HALF_Y if along_x else d.STADIUM_HALF_X) + u
                centers = aisles_x if along_x else aisles_y
                edge = half - cut/math.sqrt(2) - .12
                cuts = sorted(c for a in centers for c in (a,-a))
                starts = [-edge] + [c+d.STADIUM_AISLE_WIDTH/2 for c in cuts]
                ends = [c-d.STADIUM_AISLE_WIDTH/2 for c in cuts] + [edge]
                for lo, hi in zip(starts, ends):
                    count = math.floor((hi-lo)/d.STADIUM_SEAT_WIDTH)
                    if count:
                        yield normal, tangent, radius, first+rise*row, (lo+hi)/2, count, row


def seats(lod=0):
    """Near: tapered bucket silhouette. Far: seat/back silhouette per uninterrupted run."""
    slots = ["M_A_DeepSea","M_A_Lagoon","M_A_Tide","M_A_Gold"]
    by_slot = {s:([],[]) for s in slots}
    for n,t,r,z,mid,count,row in banks():
        slot = slots[3 if row in (5,14) else (row//3)%3]
        verts, faces = by_slot[slot]
        repeat = 1 if lod else count
        half = (count*.5-.045)/2 if lod else .2275
        section = [(-.205,.23,.92),(-.205,.32,.92),(.12,.30,1),(.20,.68,.89),(.238,.67,.89),(.17,.23,1)]
        for i in range(repeat):
            at = mid + (i-(repeat-1)/2)*.5
            base = len(verts)
            for depth,height,width in section:
                for sign in (-1,1):
                    a = at+sign*half*(1 if lod else width)
                    verts.append((n[0]*(r+depth)+t[0]*a,n[1]*(r+depth)+t[1]*a,z+height))
            for j in range(len(section)):
                k=(j+1)%len(section)
                faces.append((base+2*j,base+2*j+1,base+2*k+1,base+2*k))
            faces.extend([tuple(base+2*j for j in range(len(section))),
                          tuple(base+2*j+1 for j in reversed(range(len(section))))])
    # A common beam and spaced pedestal feet support each run; aisles stay clear.
    supports=[]
    for n,t,r,z,mid,count,row in banks():
        def rail(a,b,w,h0,h1):
            x0,y0=(n[0]*r+t[0]*a,n[1]*r+t[1]*a)
            x1,y1=(n[0]*r+t[0]*b,n[1]*r+t[1]*b)
            return box((min(x0,x1)-abs(n[0])*w,max(x0,x1)+abs(n[0])*w),
                       (min(y0,y1)-abs(n[1])*w,max(y0,y1)+abs(n[1])*w),(z+h0,z+h1))
        span=count*.5-.09
        supports.append(rail(mid-span/2,mid+span/2,.072,.17,.235))
        feet=math.ceil(count/5)
        for i in range(feet):
            at=mid+span*((i+.5)/feet-.5)
            supports.append(rail(at-.045,at+.045,.086,0,.18))
    return list(by_slot.items()) + [("M_A_Frame",join(supports))]


def capsule(cx,cy,hx,hy,z0,z1,chamfer):
    return prism((cx,cy),octagon(hx,hy,chamfer),z0,z1)


def player_chair(lod=0):
    frame=[capsule(x,y,.021,.021,.012,.455,.006) for x in (-.233,.233) for y in (-.255,.255)]
    for y in (-.255,.255):
        frame.extend([capsule(0,y,.27,.025,.010,.044,.015),capsule(0,y,.24,.02,.411,.455,.014),
                      capsule(-.229,y,.018,.018,.44,.87,.006),capsule(.161,y,.014,.014,.45,.635,.005),
                      capsule(0,y,.247,.035,.625,.655,.018)])
    timber=[capsule(x,0,.031,.26,.446,.476,.012) for x in (-.21,-.14,-.07,0,.07,.14,.21)]
    timber += [capsule(-.234,0,.02,.264,z,z+.056,.013) for z in (.635,.715,.795)]
    if lod:
        frame = [box((x-.021,x+.021),(y-.02,y+.02),(.01,.455)) for x in (-.233,.233) for y in (-.255,.255)]
        frame += [box((-.25,-.21),(y-.02,y+.02),(.44,.87)) for y in (-.255,.255)]
        if lod == 1:
            frame += [box((-.247,.247),(y-.035,y+.035),(.625,.655)) for y in (-.255,.255)]
        timber = [box((-.24,.24),(-.26,.26),(.446,.476))]
        timber += ([box((-.254,-.214),(-.264,.264),(z,z+.056)) for z in (.635,.715,.795)]
                   if lod == 1 else [box((-.254,-.214),(-.264,.264),(.635,.851))])
    return [("M_A_Frame",join(frame)),("M_A_Timber",join(timber))]


def drinks_table(lod=0):
    frame=[capsule(x,y,.015,.015,0,.42,.005) for x in (-.225,.225) for y in (-.149,.149)]
    frame += [capsule(0,0,.25,.175,.085,.102,.02),capsule(0,0,.26,.18,.42,.443,.025)]
    towels=[capsule(-.09,.01,.115,.135,.443+i*.023,.465+i*.023,.017) for i in range(3)]
    if lod:
        frame=[box((x-.015,x+.015),(y-.015,y+.015),(0,.42)) for x in (-.225,.225) for y in (-.149,.149)]
        frame.append(box((-.26,.26),(-.18,.18),(.42,.443)))
        if lod == 2:
            towels=[box((-.205,.025),(-.125,.145),(.443,.511))]
    bodies=[]; lids=[]
    for y in (-.079,.079):
        bodies.append(prism((.14,y),regular_polygon(.072,(18,10,6)[lod]),.443,.605))
        bodies.append(prism((.14,y),regular_polygon(.040,(18,10,6)[lod]),.605,.627))
        lids.append(prism((.14,y),regular_polygon(.047,(18,10,6)[lod]),.627,.65))
    return [("M_A_Frame",join(frame)),("M_A_Textile",join(towels)),("M_A_Bottle",join(bodies)),("M_A_Gold",join(lids))]
