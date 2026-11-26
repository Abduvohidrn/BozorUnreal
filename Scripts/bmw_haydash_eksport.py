"""Haydaladigan BMW (Chaos Vehicle) uchun skelet mesh: Saved/BMWSource/SK_BMW_Haydash.glb

Manba: qatnov eksporti (Scripts/qatnov_eksport.py -- BMW): g'ildiraksiz kuzov SM_BMW_Qatnov.glb (oynasiz) va
4 ta g'ildirak SM_BMW_G0..3.glb + qatnov.json (g'ildirak markazlari). Chaos Vehicle talabi: old tomoni +X.
Blender'da old tomoni -Y edi -> hammasi Z atrofida +90 gradus buriladi (glTF->Unreal: Blender X = Unreal X).
Suyaklar: root (kuzov) va wheel_fl/wheel_fr/wheel_rl/wheel_rr (g'ildirak markazida, Chaos g'ildiraklar joyi).
Skelet mesh'da faqat kuzov (soddalashtirilgan, ~110 ming uchburchak, Nanite'siz); g'ildiraklar - alohida SM_BMW_G*.
Saved/BMWSource/haydash.json: g'ildiraklar (Unreal sm, +X old) va radius.

blender -b --python Scripts/bmw_haydash_eksport.py
"""
import json
import math
import os

import bpy
from mathutils import Matrix, Vector

LOYIHA = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(LOYIHA, "Saved", "BMWSource")
BURISH = Matrix.Rotation(math.radians(90.0), 4, 'Z')   # old -Y -> +X
K_NISBAT = 0.55     # kuzov

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
info = json.load(open(os.path.join(SRC, "qatnov.json"), encoding="utf-8"))["gildiraklar"]


def yukla(fayl):
    oldin = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=fayl)
    yangi = [o for o in bpy.data.objects if o not in oldin and o.type == 'MESH']
    for o in yangi:   # ota-ona transformini qo'llab, bitta mesh
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
    for o in [o for o in bpy.data.objects if o not in oldin and o.type != 'MESH']:
        bpy.data.objects.remove(o)
    bpy.ops.object.select_all(action='DESELECT')
    for o in yangi:
        o.select_set(True)
    bpy.context.view_layer.objects.active = yangi[0]
    if len(yangi) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return ob


def soddalashtir(ob, nisbat):
    m = ob.modifiers.new("Sodda", 'DECIMATE')
    m.ratio = nisbat
    m.use_collapse_triangulate = True
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.modifier_apply(modifier=m.name)


kuzov = yukla(os.path.join(SRC, "SM_BMW_Qatnov.glb"))
kuzov.name = "BMW_Kuzov"
soddalashtir(kuzov, K_NISBAT)
kuzov.data.transform(BURISH)

# suyaklar
arm_data = bpy.data.armatures.new("BMW_Skelet")
arm = bpy.data.objects.new("BMW_Skelet", arm_data)
bpy.context.scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
root = arm_data.edit_bones.new("root")
root.head, root.tail = Vector((0, 0, 0)), Vector((0, 0, 0.5))
gildiraklar = []
for g in info:
    c = BURISH @ Vector(g["markaz"])
    nom = "wheel_%s%s" % ("f" if c.x > 0 else "r", "l" if c.y > 0 else "r")   # Blender +Y = Unreal -Y (chap)
    b = arm_data.edit_bones.new(nom)
    b.head, b.tail = c, c + Vector((0, 0, 0.3))
    b.parent = root
    gildiraklar.append((nom, g, c))
bpy.ops.object.mode_set(mode='OBJECT')

# g'ildiraklar skelet mesh'ga kirmaydi: ular alohida mesh (SM_BMW_G0..3, Nanite) - C++ Chaos g'ildirak holatidan
# (aylanish, rul, osma) har kadr joylaydi (Animation Blueprint kerak bo'lmaydi). Suyaklar faqat g'ildirak joyi uchun.
qismlar = [(kuzov, "root")]

for ob, suyak in qismlar:   # har qism bitta suyakka to'liq biriktiriladi
    vg = ob.vertex_groups.new(name=suyak)
    vg.add(range(len(ob.data.vertices)), 1.0, 'REPLACE')
    ob.parent = arm
    mod = ob.modifiers.new("Skelet", 'ARMATURE')
    mod.object = arm

bpy.ops.object.select_all(action='DESELECT')
mesh = kuzov
mesh.name = mesh.data.name = "SK_BMW_Haydash"
print("HAYDASH uchburchak", sum(len(p.vertices) - 2 for p in mesh.data.polygons), "suyaklar", [b.name for b in arm_data.bones])

bpy.ops.object.select_all(action='DESELECT')
mesh.select_set(True)
arm.select_set(True)
bpy.ops.export_scene.gltf(filepath=os.path.join(SRC, "SK_BMW_Haydash.glb"), export_format='GLB', use_selection=True,
                          export_skins=True, export_animations=False, export_yup=True, export_materials='EXPORT',
                          export_apply=False)
# Unreal koordinatalari (sm): glTF importi Y ni teskari qiladi
natija = {"gildiraklar": {nom: [c.x * 100, -c.y * 100, c.z * 100] for nom, g, c in gildiraklar},
          "meshlar": {nom: "SM_BMW_G" + g["nom"][-1] for nom, g, c in gildiraklar},
          "radius": info[0]["radius"] * 100}
with open(os.path.join(SRC, "haydash.json"), "w", encoding="utf-8") as f:
    json.dump(natija, f, indent=1)
print("HAYDASH", natija)
