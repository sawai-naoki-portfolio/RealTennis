"""Center Court A. Run in a separate, empty Blender scene (ART-03)."""
import math
from pathlib import Path

import bpy
from mathutils import Vector

from tennis_blender import broadcast_camera, bpy_utils as bu, court_props, dims as d, match_court as kit
from tennis_blender import materials, net_shape, stadium_a
from tennis_blender.primitives import box, join, prism, regular_polygon
import export_all
import validate_dimensions


def stage_mesh(name, items, scene):
    return bu.slotted_object(scene.name+"_"+name, items, scene.collection)


def camera(scene, name, eye, target, lens):
    data = bpy.data.cameras.new(name)
    data.lens = lens
    data.clip_end = 400
    obj = bpy.data.objects.new(name, data)
    scene.collection.objects.link(obj)
    obj.location = eye
    obj.rotation_euler = (Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()
    return obj


def preview(scene, objects):
    """Place game assets for review; model origins and the UE export stay unchanged."""
    for name, pieces in stadium_a.parts().items():
        if 'LOD' not in name and not name.endswith('_Floor'):
            stage_mesh('Preview_'+name,pieces,scene)
    for name in ('SM_Prop_UmpireChair','SM_Prop_Scoreboard'):
        obj = stage_mesh('Preview_'+name,court_props.parts()[name],scene)
        obj.location = (0,7.299,0) if 'Umpire' in name else (-16.9,-9.94,0)
        obj.rotation_euler.z = -math.pi/2 if 'Umpire' in name else math.pi/2
    for name, x in ((kit.CHAIR,1.45),(kit.TABLE,2.14)):
        original = objects[name]
        for sign in (-1,1):
            if sign==-1:
                obj=original.copy()
                scene.collection.objects.link(obj)
            else:
                obj=original
            obj.location=(sign*x,7.7,0)
            obj.rotation_euler.z=-math.pi/2
    rig=objects[broadcast_camera.NAME]
    rig.location=(-4.1,8.6,0)
    rig.rotation_euler.z=-math.pi/2+.23
    # Real cords in the modelling scene stand in for the game's masked shader.
    net=[]
    for i in range(321):
        y = -d.POST_Y + 2*d.POST_Y*i/320
        net.append(box((-.0013,.0013),(y-.0013,y+.0013),(.02,net_shape.top_height(y)-.045)))
    for i in range(1,24):
        for y0,y1 in zip((-d.POST_Y,-d.STICK_Y,0,d.STICK_Y),(-d.STICK_Y,0,d.STICK_Y,d.POST_Y)):
            z0,z1=[(net_shape.top_height(y)-.05)*i/24 for y in (y0,y1)]
            net.append(([(0,y0,z0-.0014),(0,y1,z1-.0014),(0,y1,z1+.0014),(0,y0,z0+.0014)],[(0,1,2,3)]))
    stage_mesh('Preview_Net',[('M_A_Frame',join(net)),('M_A_Paint',net_shape.band(48)),
                ('M_A_Frame',join([prism((0,y),regular_polygon(.085,16),0,d.NET_HEIGHT_POST) for y in (-d.POST_Y,d.POST_Y)]))],scene)
    scene.camera=camera(scene,'Overview',(54,-69,53),(0,0,3),32)
    camera(scene,'Courtside',(5.6,2.7,2.1),(.65,7.55,1.05),36)
    camera(scene,'Court',(18,-18,11),(0,0,1),30)
    world=bpy.data.worlds.new('Centre A daylight')
    world.use_nodes=True
    world.node_tree.nodes.get('Background').inputs[0].default_value=(.68,.79,.94,1)
    world.node_tree.nodes.get('Background').inputs[1].default_value=.6
    scene.world=world
    sun=bpy.data.lights.new('Afternoon sunlight','SUN')
    sun.energy=3.4
    sun.angle=math.radians(7)
    light=bpy.data.objects.new('Afternoon sunlight',sun)
    scene.collection.objects.link(light)
    light.rotation_euler=(.54,-.48,-.2)
    for name in ('M_A_Acrylic','M_A_Alley','M_A_Runoff'):
        material=bpy.data.materials[name]
        nodes=material.node_tree.nodes
        links=material.node_tree.links
        bsdf=next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
        noise=nodes.new('ShaderNodeTexNoise')
        noise.inputs['Scale'].default_value=310
        coords=nodes.new('ShaderNodeTexCoord')
        links.new(coords.outputs['Object'],noise.inputs['Vector'])
        bump=nodes.new('ShaderNodeBump')
        bump.inputs['Distance'].default_value=.0015
        bump.inputs['Strength'].default_value=.23
        links.new(noise.outputs['Fac'],bump.inputs['Height'])
        links.new(bump.outputs['Normal'],bsdf.inputs['Normal'])
        bsdf.inputs['Roughness'].default_value=.82
    scene.render.engine='CYCLES'
    scene.cycles.samples=12
    scene.cycles.use_denoising=True
    scene.render.resolution_percentage=100
    scene.render.resolution_x,scene.render.resolution_y=1440,900


def build():
    scene=bpy.context.scene
    if list(scene.objects):
        raise RuntimeError('Open an empty working scene for Center Court A')
    scene.unit_settings.system='METRIC'
    scene.unit_settings.scale_length=1
    materials.PREVIEW_COLORS.update(kit.PALETTE)
    collection=bu.reset_collection('Tennis_CenterAMatch')
    objects={}
    for name,make in ((kit.DECK,kit.deck),(kit.SEATS,kit.seats),(kit.SEATS+'_LOD1',lambda:kit.seats(1)),
                      (kit.CHAIR,kit.player_chair),(kit.TABLE,kit.drinks_table)):
        objects[name]=bu.slotted_object(name,make(),collection)
    for lod in (1,2):
        for base,make in ((kit.CHAIR,kit.player_chair),(kit.TABLE,kit.drinks_table)):
            name=base+f'_LOD{lod}'
            objects[name]=bu.slotted_object(name,make(lod),collection)
    for lod in range(3):
        name=broadcast_camera.NAME+(f'_LOD{lod}' if lod else '')
        objects[name]=bu.slotted_object(name,broadcast_camera.camera(lod),collection)
    bpy.context.view_layer.update()
    results=[validate_dimensions.check_units(),
             validate_dimensions.check_bounds(kit.DECK,(-d.STADIUM_HALF_X,-d.STADIUM_HALF_Y,0),(d.STADIUM_HALF_X,d.STADIUM_HALF_Y,0)),
             validate_dimensions.check_bounds(kit.CHAIR,(-.27,-.29,.01),(.27,.29,.87)),
             validate_dimensions.check_bounds(kit.TABLE,(-.26,-.18,0),(.26,.18,.65))]
    if any(not ok for ok,_ in results):
        raise ValueError(results)
    print('validate_dimensions：OK',results)
    print('Asset triangles:',{name:bu.triangle_count(obj) for name,obj in objects.items()})
    export_all.main()
    out=export_all.export_dir()
    for name in objects:
        if not (out/(name+'.fbx')).is_file():
            raise RuntimeError(name+' FBX missing')
    for name,obj in objects.items():
        if '_LOD' in name:
            obj.hide_render=True
            obj.hide_set(True)
    preview(scene,objects)
    scene.camera.data.type='ORTHO'
    scene.camera.data.ortho_scale=146
    path=Path(out)/'CenterCourtA'/'CenterCourtA.blend'
    path.parent.mkdir(parents=True,exist_ok=True)
    bpy.data.libraries.write(str(path),{scene},fake_user=True,compress=True)
    print(path)


if __name__=='__main__':
    build()
