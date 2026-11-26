"""spark.glb (Chevrolet/Ravon Spark) dan o'yin uchun ikki GLB (Blender, fon bilan): kuzov (Nanite) va oynalar
(Nanite'siz, shaffof). Rasmdagidek bo'yaladi: oq yaltiroq lak, kumush disklar va tom relslari, xrom, qora
ustun/oyna atrofi. Hamma qismlar bitta mesh, pivot - tagining markazi (z=0), metrda.
blender -b --python Scripts/spark_eksport.py -- <spark.glb> <chiqish SM_Spark.glb>
"""
import sys

import bpy
from mathutils import Matrix, Vector

src, out = sys.argv[sys.argv.index("--") + 1:][:2]
# read_factory_settings ishlatilmaydi: u kengaytmalarni o'chiradi va ularning Python kutubxonalarini (wheels) tozalab yuboradi
for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
bpy.ops.import_scene.gltf(filepath=src)
vl = bpy.context.view_layer

# (rang sRGB emas - chiziqli, metallik, g'adir-budurlik, lak qatlami)
RANG = {
    "carpaint": ((0.80, 0.81, 0.80), 0.0, 0.18, 1.0),      # toza oq yaltiroq lak
    "chrome": ((0.90, 0.90, 0.90), 1.0, 0.08, 0.0),
    "glossymetal": ((0.75, 0.76, 0.77), 1.0, 0.25, 0.0),   # tom relslari, belgi, boltlar
    "material": ((0.86, 0.87, 0.88), 1.0, 0.42, 0.3),      # kumush quyma disklar (och, xira yaltiroq)
    "black": ((0.012, 0.012, 0.013), 0.0, 0.45, 0.3),      # ustunlar, oyna atrofi, panjara
    "tire": ((0.015, 0.015, 0.016), 0.0, 0.85, 0.0),
    "interior": ((0.03, 0.03, 0.032), 0.0, 0.8, 0.0),
    "brakedisk": ((0.25, 0.25, 0.26), 1.0, 0.45, 0.0),
    "mirror": ((0.85, 0.87, 0.9), 1.0, 0.03, 0.0),
    "darkglass": ((0.01, 0.01, 0.012), 0.0, 0.1, 0.5),
}
OYNA = ("greenglass", "clearglass")   # alohida, Nanite'siz mesh (Unreal'da shaffof material)

for m in bpy.data.materials:
    b = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if m.node_tree else None
    if not b or m.name not in RANG:
        continue
    c, met, rough, coat = RANG[m.name]
    for link in list(b.inputs['Base Color'].links):
        m.node_tree.links.remove(link)
    b.inputs['Base Color'].default_value = (*c, 1.0)
    b.inputs['Metallic'].default_value = met
    b.inputs['Roughness'].default_value = rough
    b.inputs['Coat Weight'].default_value = coat
    b.inputs['Alpha'].default_value = 1.0

meshes = [o for o in vl.objects if o.type == 'MESH']
for o in list(bpy.data.objects):
    if o not in meshes and o.type != 'EMPTY':
        bpy.data.objects.remove(o)
for o in meshes:
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
car.name = car.data.name = "Spark"
for m in car.data.materials:
    if m and not m.name.startswith("MI_Spark_"):
        m.name = "MI_Spark_" + m.name
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
mn = Vector([min((car.matrix_world @ Vector(c))[i] for c in car.bound_box) for i in range(3)])
mx = Vector([max((car.matrix_world @ Vector(c))[i] for c in car.bound_box) for i in range(3)])
shift = Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))
car.data.transform(Matrix.Translation(-shift))
print("SPARK_BBOX", tuple(mn - shift), tuple(mx - shift), "polys", len(car.data.polygons))

bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='DESELECT')
for i, m in enumerate(car.data.materials):
    if m and m.name.replace("MI_Spark_", "") in OYNA:
        car.active_material_index = i
        bpy.ops.object.material_slot_select()
bpy.ops.mesh.separate(type='SELECTED')
bpy.ops.object.mode_set(mode='OBJECT')
glass = [o for o in bpy.context.selected_objects if o != car][0]
glass.name = glass.data.name = "Spark_Oyna"
for o in (car, glass):
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    vl.objects.active = o
    bpy.ops.object.material_slot_remove_unused()
print("SPARK_OYNA polys", len(glass.data.polygons), "kuzov", len(car.data.polygons),
      [m.name for m in glass.data.materials])
for o, f in ((car, out), (glass, out.replace(".glb", "_Oyna.glb"))):
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=f, export_format='GLB', use_selection=True, export_apply=True,
                              export_yup=True, export_materials='EXPORT', export_cameras=False, export_lights=False)
