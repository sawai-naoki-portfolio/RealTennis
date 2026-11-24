"""Courtside broadcast camera on a fluid-head tripod. Origin on the ground, lens facing +X (m)."""
import math

from .primitives import box, join, octagon, prism, regular_polygon, straight_tube


NAME = "SM_Prop_A_BroadcastCamera"


def camera(lod=0):
    """The distant meshes keep the silhouette, lens centre and tripod footprint."""
    sides = (24, 12, 8)[lod]

    def rod(a, b, diameter):
        return straight_tube(a, b, regular_polygon(diameter, sides))

    def case(x, y, width, depth, z0, z1, cut):
        return prism((x, y), octagon(width / 2, depth / 2, cut), z0, z1)

    frame, shell, glass, markings = [], [], [], []
    for angle in (0, 120, 240):
        co, si = math.cos(math.radians(angle)), math.sin(math.radians(angle))

        def leg(radius, height, lateral=0):
            return (radius * co - lateral * si, radius * si + lateral * co, height)

        frame.append(rod(leg(.44, .023), leg(.23, .68), .044))
        for offset in ((-.022, .022) if lod == 0 else (0,)):
            frame.append(rod(leg(.25, .62, offset), leg(.08, 1.2, offset), .036))
        shell.extend([rod(leg(.446, .015), leg(.427, .082), .070),
                      rod(leg(.255, .61), leg(.23, .69), .080)])
        if lod < 2:
            frame.append(rod(leg(.025, .26), leg(.34, .32), .017))
    shell.extend([prism((0, 0), regular_polygon(.20, sides), 1.15, 1.26),
                  case(0, 0, .24, .19, 1.25, 1.38, .033),
                  case(-.055, 0, .46, .225, 1.39, 1.65, .025),
                  case(-.32, 0, .082, .205, 1.41, 1.61, .012)])
    frame.append(case(-.01, 0, .38, .145, 1.37, 1.40, .012))
    # Zoom barrel, focus rings and recessed front glass.
    shell.append(rod((.16, 0, 1.525), (.47, 0, 1.525), .158))
    for x in ((.21, .235, .29, .32, .42) if lod == 0 else (.24, .42)):
        frame.append(rod((x, 0, 1.525), (x+.018, 0, 1.525), .183))
    glass.append(rod((.47, 0, 1.525), (.478, 0, 1.525), .14))
    # Open matte box: four tapered leaves, leaving the optics visible from the court.
    for theta in range(4):
        co, si = math.cos(theta*math.pi/2), math.sin(theta*math.pi/2)
        verts = [(x, y*co-h*si, 1.525+y*si+h*co) for x, y, h in
                 [(.44,-.09,.09),(.44,.09,.09),(.57,.127,.127),(.57,-.127,.127),
                  (.44,-.09,.096),(.44,.09,.096),(.57,.127,.133),(.57,-.127,.133)]]
        shell.append((verts, [(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]))
    # Rear-facing viewfinder on a yoke and operator's pan handle.
    frame.append(rod((-.12, 0, 1.64),(-.17, 0, 1.72), .042))
    shell.append(case(-.17, 0, .065, .245, 1.70, 1.85, .009))
    glass.append(box((-.205,-.202),(-.113,.113),(1.712,1.838)))
    frame.append(rod((-.01,.08,1.30),(-.43,.22,1.15), .022))
    shell.append(rod((-.32,.185,1.19),(-.46,.23,1.14), .04))
    markings.append(case(.105,0,.038,.079,1.65,1.67,.006))
    if lod == 0:
        for y in (-.114,.114):
            for x in (-.18,-.16,-.14,-.12,-.10,-.08):
                shell.append(box((x,x+.009),(y-.003,y+.003),(1.43,1.52)))
            markings.append(box((-.03,.085),(y-.002,y+.002),(1.56,1.573)))
    return [(name,join(parts)) for name,parts in
            (("M_A_Frame",frame),("M_A_DeepSea",shell),("M_A_Lagoon",glass),("M_A_Gold",markings))]
