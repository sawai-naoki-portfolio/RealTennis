"""Build one venue in a clean Blender background process; existing scenes are never erased."""
import argparse
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tennis_blender import bpy_utils as bu, materials, venues as v
from tennis_blender import net_shape, dims
from tennis_blender.primitives import box, join, prism, regular_polygon
import export_all
import validate_dimensions


def light(scene, name, kind, position, power, size):
    data=bpy.data.lights.new(name,kind); data.energy=power
    if kind=='AREA': data.shape='DISK';data.size=size
    if kind=='SUN': data.angle=math.radians(6)
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=position
    return obj


def build(code,out,render):
    if bpy.data.filepath:
        raise RuntimeError('Use a clean background Blender instance')
    s=bpy.context.scene
    for obj in list(s.objects):
        if obj.name not in ('Cube','Light','Camera'):
            raise RuntimeError('Scene contains user objects')
    for obj in list(s.objects): bpy.data.objects.remove(obj,do_unlink=True)
    s.name='Venue_'+code
    s.unit_settings.system='METRIC';s.unit_settings.scale_length=1
    colors=v.palette(code)
    materials.PREVIEW_COLORS.update(colors)
    coll=bu.reset_collection('Tennis_Venue_'+code)
    deck='SM_Court_'+code+'_MatchDeck'
    names=[(deck,v.deck(code))]
    venue=v.VENUES.get(code)
    if venue:
        prefix='SM_Stadium_'+code+'_'
        names.extend([(prefix+'Bowl',v.bowl(code)),(prefix+'Bowl_LOD1',v.bowl(code,1)),
                      (prefix+'Seats',v.seats(code)),(prefix+'Seats_LOD1',v.seats(code,1)),
                      (prefix+'Architecture',v.architecture(code))])
    out.mkdir(parents=True,exist_ok=True)
    stats={}
    for name,parts in names:
        o=bu.slotted_object(name,parts,coll)
        bpy.context.view_layer.update()
        if name==deck:
            hx,hy=(18.285,9.145) if code=='Practice' else (20.115,10.055)
            passed,msg=validate_dimensions.check_bounds(name,(-hx,-hy,0),(hx,hy,0))
            if not passed:raise ValueError(msg)
        stats[name]=bu.triangle_count(o)
    for name,_ in names:
        o=bpy.data.objects[name]
        export_all._export(o,out/(name+'.fbx'))
    for name,_ in names:
        if '_LOD' in name:
            bpy.data.objects[name].hide_render=True
            bpy.data.objects[name].hide_set(True)
    for suffix in ('Playing','Alley','Surround','Worn','Band'):
        m=bpy.data.materials.get(f'M_V{code}_{suffix}')
        if not m:continue
        m.use_nodes=True; nt=m.node_tree
        bs=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED')
        bs.inputs['Roughness'].default_value=.84
        noise=nt.nodes.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value=190 if code=='B' else 330
        tc=nt.nodes.new('ShaderNodeTexCoord');nt.links.new(tc.outputs['Object'],noise.inputs['Vector'])
        bump=nt.nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.24;bump.inputs['Distance'].default_value=.0018
        nt.links.new(noise.outputs['Fac'],bump.inputs['Height']);nt.links.new(bump.outputs['Normal'],bs.inputs['Normal'])
    # A real cord silhouette in the modelling scene; UE uses the shared masked net.
    top=net_shape.top_height; yend=dims.POST_Y
    wire=[]
    for i in range(321):
        y=-yend+2*yend*i/320
        wire.append(box((-.0015,.0015),(y-.0015,y+.0015),(.03,top(y)-.045)))
    for i in range(1,24):
        for y0,y1 in zip((-yend,-dims.STICK_Y,0,dims.STICK_Y),(-dims.STICK_Y,0,dims.STICK_Y,yend)):
            z0,z1=[(top(y)-.05)*i/24 for y in (y0,y1)]
            wire.append(([(0,y0,z0-.002),(0,y1,z1-.002),(0,y1,z1+.002),(0,y0,z0+.002)],[(0,1,2,3)]))
    metal='M_V'+code+('_Metal' if venue else '_Surround')
    bu.slotted_object('PreviewNet',[(metal,join(wire)),('M_V'+code+'_Line',net_shape.band(64)),
        (metal,join([prism((0,y),regular_polygon(.1,16),0,dims.NET_HEIGHT_POST) for y in (-yend,yend)]))],coll)
    if code=='Practice':
        from tennis_blender import practice_court
        for name,parts in practice_court.parts().items():
            if name.endswith('Fence'):
                # The shared fence uses a masked material in UE. Show its posts and rails here.
                parts=parts[:2]
            bu.slotted_object(name,parts,coll)
    s.world=bpy.data.worlds.new('Venue ambience');s.world.use_nodes=True
    s.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.69,.78,.90,1)
    s.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.55
    sun=light(s,'Daylight','SUN',(0,0,50),3,0);sun.rotation_euler=(.61,-.44,-.32)
    if code=='D':
        for x in (-18,0,18):
            for y in (-12,12):light(s,'Hall light','AREA',(x,y,12),6500,9)
    cam=bpy.data.cameras.new('Venue review');co=bpy.data.objects.new('Venue review',cam)
    s.collection.objects.link(co);s.camera=co
    eye,target,lens=((33,-39,24),(0,4,3),31) if venue else ((23,-25,18),(0,0,0),33)
    if code=='D': eye,target,lens=(22,-17,5.4),(0,4,4),22
    if code=='E': eye,target,lens=(29,-34,21),(0,20,8),28
    co.location=eye;co.rotation_euler=(Vector(target)-Vector(eye)).to_track_quat('-Z','Y').to_euler()
    cam.lens=lens;cam.clip_end=420
    s.render.engine='CYCLES';s.cycles.samples=6;s.cycles.use_denoising=True
    s.render.resolution_percentage=100;s.render.resolution_x=1100;s.render.resolution_y=700
    path=out/'ProductionVenues'/code;path.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(path/('Venue'+code+'.blend')),compress=True)
    (path/'mesh-stats.json').write_text(json.dumps(stats,indent=2),encoding='utf8')
    if render:
        s.render.filepath=str(path/'venue.png');bpy.ops.render.render(write_still=True)


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('venue',choices=['Practice',*v.VENUES]);ap.add_argument('out',type=Path)
    ap.add_argument('--render',action='store_true')
    args=ap.parse_args(sys.argv[sys.argv.index('--')+1:]);build(args.venue,args.out,args.render)
