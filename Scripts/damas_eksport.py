"""Damas_4_wheels_color.blend dan o'yin uchun bitta GLB (Blender, fon bilan): hamma qismlar bitta mesh'ga
birlashtiriladi, pivot - tagining markazi (g'ildirak pastki nuqtasi z=0), metrda. Teksturalar GLB ichiga kiradi.
blender -b <Damas_4_wheels_color.blend> --python Scripts/damas_eksport.py -- <chiqish.glb>
"""
import re
import sys

import bpy
from mathutils import Matrix, Vector

out = sys.argv[sys.argv.index("--") + 1]
vl = bpy.context.view_layer
meshes = [o for o in vl.objects if o.type == 'MESH' and o.visible_get()]
for o in list(bpy.data.objects):
    if o not in meshes and o.type != 'EMPTY':
        bpy.data.objects.remove(o)
for o in meshes:   # ota (empty) transformini qo'llab, ierarxiyadan chiqaradi
    mw = o.matrix_world.copy()
    o.parent = None
    o.matrix_world = mw
for o in list(bpy.data.objects):
    if o.type == 'EMPTY':
        bpy.data.objects.remove(o)
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
vl.objects.active = meshes[0]
bpy.ops.object.make_single_user(object=True, obdata=True)
bpy.ops.object.convert(target='MESH')
bpy.ops.object.join()
car = vl.objects.active
car.name = car.data.name = "Damas"
for m in car.data.materials:   # Unreal asset nomlari: "chassis.2" -> MI_Damas_chassis_2
    if m and not m.name.startswith("MI_Damas_"):
        m.name = "MI_Damas_" + re.sub(r"[^A-Za-z0-9]+", "_", m.name).strip("_")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
mn = Vector([min((car.matrix_world @ Vector(c))[i] for c in car.bound_box) for i in range(3)])
mx = Vector([max((car.matrix_world @ Vector(c))[i] for c in car.bound_box) for i in range(3)])
shift = Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))
car.data.transform(Matrix.Translation(-shift))
print("DAMAS_BBOX", tuple(mn - shift), tuple(mx - shift), "polys", len(car.data.polygons), "mats", len(car.data.materials))
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', use_selection=False, export_apply=True,
                          export_yup=True, export_materials='EXPORT', export_image_format='AUTO',
                          export_cameras=False, export_lights=False)
