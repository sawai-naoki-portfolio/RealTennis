"""Production venues B–E. Geometry in court-local metres (+X along court)."""
from __future__ import annotations
from bisect import bisect_right
from dataclasses import dataclass
from functools import lru_cache
from math import ceil, cos, floor, hypot, pi, sin

from . import match_court
from .primitives import box, join, regular_polygon, straight_tube


@dataclass(frozen=True)
class Venue:
    capacity: int
    rows: int
    infield: tuple
    surround: tuple
    seats: tuple
    accent: tuple
    name: str


VENUES = {
    'B': Venue(10000, 26, (.45,.115,.049), (.39,.082,.032), (.24,.18,.125), (.85,.60,.31), 'Terracotta'),
    'C': Venue(15000, 34, (.077,.235,.074), (.067,.189,.072), (.041,.128,.075), (.64,.74,.55), 'Fern'),
    'D': Venue(8000, 22, (.034,.103,.251), (.049,.078,.128), (.09,.105,.169), (.31,.52,.72), 'Midnight'),
    'E': Venue(11000, 28, (.052,.202,.209), (.172,.178,.14), (.104,.165,.193), (.77,.425,.185), 'Summit'),
}
PRACTICE = Venue(0, 0, (.049,.159,.305), (.06,.199,.163), (), (), 'Practice')
BLOCKS = 24
TREAD, RISE, FIRST = .85, .42, 1.4


def palette(code):
    p = PRACTICE if code == 'Practice' else VENUES[code]
    colors = {'Playing': p.infield, 'Surround': p.surround, 'Alley': tuple(c*1.09 for c in p.infield),
              'Band': tuple(c*1.18 for c in p.infield), 'Line': (.89,.89,.82), 'Worn': (.246,.232,.113),
              'Stone': (.37,.34,.29), 'Metal': (.16,.18,.18), 'Timber': (.34,.189,.092),
              'Light': (.92,.93,.85), 'Rock': (.20,.24,.24), 'Ridge': (.28,.31,.30)}
    if code != 'Practice':
        colors.update({'Seat':p.seats,'Accent':p.accent})
    return {f'M_V{code}_{k}':v for k,v in colors.items()}


def deck(code):
    """Split grass into mowing stripes and wear islands; retain all exact painted edges."""
    groups = {}
    def face(k,x0,x1,y0,y1):
        groups.setdefault(k,[]).append(([(x0,y0,0),(x1,y0,0),(x1,y1,0),(x0,y1,0)],[(0,1,2,3)]))
    for slot,(verts,faces) in match_court.deck():
        mat={'M_A_Acrylic':'Playing','M_A_Alley':'Alley','M_A_Runoff':'Surround','M_A_Paint':'Line'}[slot]
        for f in faces:
            coords=[verts[i] for i in f]
            x0,x1=min(v[0] for v in coords),max(v[0] for v in coords)
            y0,y1=min(v[1] for v in coords),max(v[1] for v in coords)
            if code=='Practice':
                x0,x1=max(x0,-18.285),min(x1,18.285)
                y0,y1=max(y0,-9.145),min(y1,9.145)
                if x0>=x1 or y0>=y1: continue
            if code!='C' or mat not in ('Playing','Surround','Alley'):
                face(mat,x0,x1,y0,y1);continue
            cuts=sorted({x0,x1,*[i*1.485 for i in range(-14,15) if x0<i*1.485<x1],
                         *[i*.2 for i in range(-64,65) if 10.7<abs(i*.2)<12.8 and x0<i*.2<x1]})
            for lo,hi in zip(cuts,cuts[1:]):
                xx=(lo+hi)/2
                ys=sorted({y0,y1,*[i*.25 for i in range(-16,17) if 10.7<abs(xx)<12.8 and y0<i*.25<y1]})
                for a,b in zip(ys,ys[1:]):
                    yy=(a+b)/2
                    wear=((abs(xx)-11.85)/.54)**2+(yy/3.1)**2 < .72+.22*sin(yy*3.4+xx)+.17*cos(yy*6.1-xx)
                    k='Worn' if wear else ('Band' if mat=='Playing' and floor(xx/1.485)%2 else mat)
                    face(k,lo,hi,a,b)
    return [(f'M_V{code}_{k}',join(parts)) for k,parts in groups.items()]


@lru_cache(maxsize=100)
def ring(offset):
    """Closed polyline, rounded rectangular plan. Distances are cumulative arc lengths."""
    x,y,r=24+offset,13.9+offset,3.7+offset*.38
    points=[]
    for cx,cy,start in [(x-r,y-r,0),(-x+r,y-r,pi/2),(-x+r,-y+r,pi),(x-r,-y+r,1.5*pi)]:
        for j in range(9):
            a=start+j*pi/16
            points.append((cx+r*cos(a),cy+r*sin(a)))
    points.append(points[0])
    s=[0.]
    for a,b in zip(points,points[1:]):
        s.append(s[-1]+hypot(b[0]-a[0],b[1]-a[1]))
    return points,s


def at(offset, fraction):
    points,dist=ring(offset)
    s=(fraction%1)*dist[-1]
    i=min(len(dist)-2,bisect_right(dist,s)-1)
    a,b=points[i:i+2]
    span=dist[i+1]-dist[i]
    tx,ty=(b[0]-a[0])/span,(b[1]-a[1])/span
    return a[0]+(s-dist[i])*tx,a[1]+(s-dist[i])*ty,tx,ty


def shell(offset0,z0,offset1,z1):
    a=ring(offset0)[0][:-1]; b=ring(offset1)[0][:-1]; n=len(a)
    return ([(x,y,z0) for x,y in a]+[(x,y,z1) for x,y in b],
            [(i,i+n,(i+1)%n+n,(i+1)%n) for i in range(n)])


def row_offset(row,rows):
    return row*TREAD+(1.65 if row>=rows//2 else 0.)


def bowl(code,lod=0):
    rows=VENUES[code].rows
    end=row_offset(rows,rows)
    # Map each inner ring vertex onto the court's rectangular perimeter.
    outer=ring(-2.5)[0][:-1]
    inner=[]
    for x,y in outer:
        scale=min(20.115/max(abs(x),1e-9),10.055/max(abs(y),1e-9))
        inner.append((x*scale,y*scale,-.015))
    n=len(inner)
    apron=(inner+[(x,y,-.015) for x,y in outer],
           [(i,i+n,(i+1)%n+n,(i+1)%n) for i in range(n)])
    steps=[apron,shell(-2.5,0,-2.5,.93),shell(-2.5,.93,-2.28,.93),shell(-2.28,0,0,0),
           shell(0,0,0,FIRST),shell(end,FIRST+rows*RISE,end,0)]
    for i in range(0,rows,2 if lod else 1):
        j=min(rows,i+(2 if lod else 1))
        a,b=row_offset(i,rows),row_offset(j,rows)
        z=FIRST+i*RISE
        steps.extend([shell(a,z,b,z),shell(b,z,b,FIRST+j*RISE)])
    return [(f'M_V{code}_Stone',join(steps))]


def apportion(total,weights):
    values=[total*w/sum(weights) for w in weights]
    counts=[floor(x) for x in values]
    for i in sorted(range(len(weights)),key=lambda i:values[i]-counts[i],reverse=True)[:total-sum(counts)]:
        counts[i]+=1
    return counts


def seats(code,lod=0):
    v=VENUES[code]; by=[([],[]),([],[])]
    weights=[ring(row_offset(i,v.rows))[1][-1]-BLOCKS*1.3 for i in range(v.rows)]
    for i,count in enumerate(apportion(v.capacity,weights)):
        offset=row_offset(i,v.rows)+.37
        per=ring(offset)[1][-1]
        z=FIRST+i*RISE
        for block in range(BLOCKS):
            n=count//BLOCKS+(block<count%BLOCKS)
            width=per/BLOCKS-1.3
            # Preserve spaces for radial aisles; short distant banks follow the curve.
            repeats=ceil(n/5) if lod else n
            vs,fs=by[int(i in (3,v.rows//2+3) or (i>v.rows-4 and block%4==1))]
            for j in range(repeats):
                frac=(block+.5)/BLOCKS+((j+.5)/repeats-.5)*width/per
                x,y,tx,ty=at(offset,frac)
                half=(width/repeats-.045)/2 if lod else min(.24,width/n*.46)
                start=len(vs)
                profile=[(-.19,.27),(.135,.27),(.215,.66),(.25,.66),(.18,.205),(-.19,.205)]
                for d,h in profile:
                    for side in (-1,1):
                        w=side*half*(.93 if h>.6 and not lod else 1)
                        vs.append((x+tx*w+ty*d,y+ty*w-tx*d,z+h))
                for k in range(6):
                    j2=(k+1)%6
                    fs.append((start+2*j2,start+2*j2+1,start+2*k+1,start+2*k))
                fs.extend([tuple(start+2*k for k in range(6)),tuple(start+2*k+1 for k in reversed(range(6)))])
    return [(f'M_V{code}_{key}',m) for key,m in zip(('Seat','Accent'),by)]


def beam(a,b,d=.15): return straight_tube(a,b,regular_polygon(d,8))
def column(x,y,r,z0,z1):
    from .primitives import prism
    return prism((x,y),regular_polygon(2*r,12),z0,z1)


def architecture(code):
    """B: terracotta arcades; C: timber pavilion; D: indoor portal trusses; E: alpine horizon."""
    v=VENUES[code]; end=row_offset(v.rows,v.rows); top=FIRST+v.rows*RISE
    hx,hy=24+end,13.9+end
    g={}
    def add(key,part):g.setdefault(key,[]).append(part)
    if code=='B':
        for sign in (-1,1):
            y=sign*(hy+1.5)
            for bay in range(-7,8):
                x=bay*4
                add('Stone',box((x-.18,x+.18),(y-.26,y+.26),(top-1.1,top+2.1)))
                if bay==7: continue
                points=[(x+2+1.99*cos(pi-i*pi/12),y,top+1.9+1.3*sin(i*pi/12)) for i in range(13)]
                for a,b in zip(points,points[1:]):add('Stone',beam(a,b,.29))
            add('Accent',box((-29,29),(y-.95,y+.95),(top+3.25,top+3.45)))
    elif code=='C':
        y=hy+2.2
        for i in range(-5,6):
            x=i*4
            add('Timber',column(x,y,.16,top-.8,top+3.5))
            for sign in (-1,1):
                add('Timber',beam((x,y,top+2.4),(x+sign*1.0,y+.04,top+3.45),.16))
        for y0,y1,z0,z1 in [(y-4,y,top+3.4,top+4.8),(y,y+4,top+4.8,top+3.4)]:
            add('Seat',([(-22,y0,z0),(22,y0,z0),(22,y1,z1),(-22,y1,z1),
                        (-22,y0,z0-.14),(22,y0,z0-.14),(22,y1,z1-.14),(-22,y1,z1-.14)],
                        [(0,1,2,3),(7,6,5,4),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)]))
    elif code=='D':
        h=top+6; y=hy+1.5
        for s in (-1,1):
            add('Metal',box((-hx-1.5,hx+1.5),(s*y-.17,s*y+.17),(0,h)))
            add('Metal',box((s*(hx+1.5)-.17,s*(hx+1.5)+.17),(-y,y),(0,h)))
        for i in range(11):
            x=-hx+2*hx*i/10
            for sign in (-1,1):
                add('Metal',column(x,sign*(y-.4),.26,0,h))
                for off in (0,1.25):
                    add('Metal',beam((x,sign*y,h+off),(x,0,h+off+3),.22))
                for k in range(8):
                    y0,y1=sign*y*k/8,sign*y*(k+1)/8
                    z0=h+3-3*k/8;z1=h+3-3*(k+1)/8
                    add('Metal',beam((x,y0,z0+(1.25 if k%2 else 0)),(x,y1,z1+(0 if k%2 else 1.25)),.11))
            for sign in (-1,1):
                yc=sign*y*.56
                add('Light',box((x-1.4,x+1.4),(yc-.17,yc+.17),(h+.3,h+.4)))
        add('Metal',box((-hx-1.7,hx+1.7),(-y-.3,y+.3),(h+4.45,h+4.62)))
    else:
        for layer,key in enumerate(('Rock','Ridge')):
            verts=[];faces=[]
            for i in range(128):
                a=i*2*pi/128
                r=132+42*layer+9*sin(7*a)
                h=29+8*layer+9*sin(3*a+.8)+9*abs(sin(7*a+layer))+6*cos(13*a-1)+2*sin(31*a)
                for radius,height in [(r-31,-.7),(r-8,h*.53),(r,h),(r+18,h*.5)]:
                    verts.append((radius*cos(a),radius*sin(a),height))
            for i in range(128):
                j=(i+1)%128
                for k in range(3):
                    a,b,c,d=i*4+k,j*4+k,j*4+k+1,i*4+k+1
                    faces.extend([(a,b,d),(b,c,d)])
            add(key,(verts,faces))
    return [(f'M_V{code}_{k}',join(parts)) for k,parts in g.items()]
