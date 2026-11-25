"""Chevrolet_Cobalt.blend dan o'yin uchun GLB (kuzov + alohida oynalar) (Blender, fon bilan): studiya obyektlari tashlanadi,
hamma qismlar bitta mesh'ga birlashtiriladi, pivot - tagining markazi (old tomoni -X, metrda).
blender -b <Chevrolet_Cobalt.blend> --python Scripts/cobalt_eksport.py -- <chiqish.glb>
"""
import sys

import bpy
from mathutils import Vector

out = sys.argv[sys.argv.index("--") + 1]
vl = bpy.context.view_layer
for o in list(bpy.data.objects):
    keep = o.type == 'MESH' and o.name in vl.objects and o.visible_get() \
        and not any(k in o.name for k in ("Studio", "Cyclorama"))
    if not keep and o.type != 'EMPTY':
        bpy.data.objects.remove(o)
meshes = [o for o in vl.objects if o.type == 'MESH']
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
car.name = car.data.name = "Cobalt"
import re
for m in car.data.materials:   # Unreal asset nomlari uchun faqat ASCII: "Champagne • clearcoat" -> MI_Cobalt_Champagne_clearcoat
    if m and not m.name.startswith("MI_Cobalt_"):
        m.name = "MI_Cobalt_" + re.sub(r"[^A-Za-z0-9]+", "_", m.name).strip("_")
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
mn = Vector([min((car.matrix_world @ Vector(c))[i] for c in car.bound_box) for i in range(3)])
mx = Vector([max((car.matrix_world @ Vector(c))[i] for c in car.bound_box) for i in range(3)])
shift = Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))
car.data.transform(__import__("mathutils").Matrix.Translation(-shift))
print("COBALT_BBOX", tuple(mn - shift), tuple(mx - shift), "polys", len(car.data.polygons), "mats", len(car.data.materials))
# Oynalar alohida mesh: Nanite shaffof materialni qo'llamaydi, shuning uchun shisha Nanite'siz SM_Cobalt_Oyna bo'ladi
oyna = out.replace(".glb", "_Oyna.glb")
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='DESELECT')
for i, m in enumerate(car.data.materials):
    if m and "solar_glass" in m.name:
        car.active_material_index = i
        bpy.ops.object.material_slot_select()
bpy.ops.mesh.separate(type='SELECTED')
bpy.ops.object.mode_set(mode='OBJECT')
glass = [o for o in bpy.context.selected_objects if o != car][0]
glass.name = glass.data.name = "Cobalt_Oyna"
for o in (car, glass):   # bo'sh material slotlari eksportga tushmasin
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.material_slot_remove_unused()
print("COBALT_OYNA polys", len(glass.data.polygons), "kuzov polys", len(car.data.polygons))
for o, f in ((car, out), (glass, oyna)):
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=f, export_format='GLB', use_selection=True, export_apply=True,
                              export_yup=True, export_materials='EXPORT', export_cameras=False, export_lights=False)
