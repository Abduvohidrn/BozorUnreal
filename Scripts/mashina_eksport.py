"""creat_3d_asset dagi mashina GLB'laridan o'yin uchun ikki GLB (Blender, fon bilan): kuzov (Nanite) va oynalar
(Nanite'siz, shaffof). Har bir mashina MASHINALAR da: manba, real uzunlik (masshtab), rang (O'zbekistonda odatiy:
oq, qora, kumush) va qaysi materiallar shisha. Hamma qismlar bitta mesh, pivot - tagining markazi (z=0), metrda.
Saved/<Nom>Source/ ga SM_<Nom>.glb, SM_<Nom>_Oyna.glb, oyna.json (shisha/chiroq qopqoqlari) va boyoq.json
(slot -> rang, metallik, g'adir-budurlik, lak) yoziladi.

blender -b --python Scripts/mashina_eksport.py -- <Nom>
(read_factory_settings ishlatilmaydi - u Blender kengaytmalarining Python kutubxonalarini tozalab yuboradi)
"""
import json
import os
import re
import sys

import bpy
from mathutils import Matrix, Vector

MANBA = r"D:\Unreal Projects\creat_3d_asset"
LOYIHA = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

# (chiziqli rang, metallik, g'adir-budurlik, lak qatlami)
XROM = ((0.9, 0.9, 0.9), 1.0, 0.08, 0.0)
QORA_PLASTIK = ((0.015, 0.015, 0.016), 0.0, 0.5, 0.0)
SHINA = ((0.015, 0.015, 0.016), 0.0, 0.85, 0.0)
DISK = ((0.78, 0.79, 0.8), 0.7, 0.3, 0.3)   # kumush quyma disk (soyada ham och ko'rinsin)
MASHINALAR = {
    "Spark": {
        "fayl": "spark.glb",
        "uzunlik": None,   # model o'lchami real (3.65 m)
        "rang": {
            "carpaint": ((0.80, 0.81, 0.80), 0.0, 0.18, 1.0),      # toza oq yaltiroq lak
            "chrome": XROM,
            "glossymetal": ((0.75, 0.76, 0.77), 1.0, 0.25, 0.0),   # tom relslari, belgi, boltlar
            "material": DISK,                                      # kumush quyma disklar
            "black": ((0.012, 0.012, 0.013), 0.0, 0.45, 0.3),      # ustunlar, oyna atrofi, panjara
            "tire": SHINA,
            "interior": ((0.03, 0.03, 0.032), 0.0, 0.8, 0.0),
            "brakedisk": ((0.25, 0.25, 0.26), 1.0, 0.45, 0.0),
            "mirror": ((0.85, 0.87, 0.9), 1.0, 0.03, 0.0),
            "darkglass": ((0.01, 0.01, 0.012), 0.0, 0.1, 0.5),
        },
        "oyna": ["greenglass"],
        "chiroq": ["clearglass"],
    },
    "BMW": {
        # bmw-m5-the-carbon-storm.zip -> Saved/BmwSource/zip/source/Untitled3.glb (BMW M5 F90, oq, forged carbon kapot/tom)
        "fayl": os.path.join(LOYIHA, "Saved", "BmwSource", "zip", "source", "Untitled3.glb"),
        "uzunlik": None,   # model o'lchami real (5.1 m, balandligi 1.46 m)
        "rang": {
            "TEX.035": ((0.8, 0.8, 0.8), 0.0, 0.2, 1.0),             # oq yaltiroq lak (kuzov)
            "TEX.016": ((0.3, 0.01, 0.01), 0.0, 0.15, 0.5),          # stop-chiroq qopqog'i (shaffof edi -> to'q qizil)
        },
        "oyna": ["TEX.017"],
        "chiroq": ["TEX.014"],
    },
    "Malibu": {
        "fayl": "2019_chevrolet_malibu_premier.glb",
        "uzunlik": 4.92,   # Chevrolet Malibu 2019 - 4923 mm
        "rang": {
            "carpaint_mat": ((0.012, 0.013, 0.015), 0.6, 0.22, 1.0),   # qora metallik
            "material": DISK,
            "plastic_B": QORA_PLASTIK,
            "black_P": ((0.01, 0.01, 0.01), 0.0, 0.12, 0.5),          # yaltiroq qora qoplama
            "chrome_mat": XROM,
        },
        "oyna": ["glass", "black_glass1"],
        "chiroq": ["glass_norm"],
    },
    "Nexia": {
        "fayl": "nexia_2.glb",
        "uzunlik": None,   # model o'lchami real (balandligi 1.38 m)
        "rang": {
            "carpaint": ((0.3, 0.31, 0.33), 0.8, 0.3, 1.0),             # kumush metallik
            "chrome": XROM,
            "material": DISK,
            "black": QORA_PLASTIK,
            "tire": SHINA,
            "interior": ((0.04, 0.04, 0.042), 0.0, 0.8, 0.0),
            "mattemetal": ((0.6, 0.6, 0.62), 1.0, 0.45, 0.0),
            "mirror": ((0.85, 0.87, 0.9), 1.0, 0.03, 0.0),
            "brakedisk": ((0.3, 0.3, 0.31), 1.0, 0.45, 0.0),
            "orangeglass": ((0.74, 0.29, 0.0), 0.0, 0.2, 0.5),
            "redglass": ((0.43, 0.05, 0.05), 0.0, 0.2, 0.5),
        },
        "oyna": ["windowglass"],
        "chiroq": ["clearglass"],
    },
}

nom = sys.argv[sys.argv.index("--") + 1]
cfg = MASHINALAR[nom]
chiqish = os.path.join(LOYIHA, "Saved", nom + "Source")
os.makedirs(chiqish, exist_ok=True)
out = os.path.join(chiqish, "SM_%s.glb" % nom)

for o in list(bpy.data.objects):
    bpy.data.objects.remove(o)
bpy.ops.import_scene.gltf(filepath=os.path.join(MANBA, cfg["fayl"]))
vl = bpy.context.view_layer

for m in bpy.data.materials:
    b = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if m.node_tree else None
    if not b or m.name not in cfg["rang"]:
        continue
    c, met, rough, coat = cfg["rang"][m.name]
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
car.name = car.data.name = nom
asl = {}
for m in car.data.materials:   # Unreal asset nomlari: MI_<Nom>_<material>, faqat ASCII
    if m and not m.name.startswith("MI_"):
        yangi = "MI_%s_%s" % (nom, re.sub(r"[^A-Za-z0-9]+", "_", m.name).strip("_"))
        asl[yangi] = m.name
        m.name = yangi
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def bbox():   # uchlardan (bound_box mesh o'zgargach yangilanmaydi); transform qo'llangan - lokal = dunyo
    vs = [v.co for v in car.data.vertices]
    return (Vector([min(v[i] for v in vs) for i in range(3)]), Vector([max(v[i] for v in vs) for i in range(3)]))


mn, mx = bbox()
if cfg["uzunlik"]:
    k = cfg["uzunlik"] / max(mx.x - mn.x, mx.y - mn.y)
    car.data.transform(Matrix.Scale(k, 4))
    mn, mx = bbox()
    print("MASHINA masshtab", round(k, 4))
shift = Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))
car.data.transform(Matrix.Translation(-shift))
print("MASHINA_BBOX", nom, tuple(round(v, 3) for v in (mn - shift)), tuple(round(v, 3) for v in (mx - shift)),
      "polys", len(car.data.polygons))

shisha = set(cfg["oyna"]) | set(cfg["chiroq"])
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='DESELECT')
for i, m in enumerate(car.data.materials):
    if m and asl.get(m.name) in shisha:
        car.active_material_index = i
        bpy.ops.object.material_slot_select()
bpy.ops.mesh.separate(type='SELECTED')
bpy.ops.object.mode_set(mode='OBJECT')
glass = [o for o in bpy.context.selected_objects if o != car][0]
glass.name = glass.data.name = nom + "_Oyna"
for o in (car, glass):
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    vl.objects.active = o
    bpy.ops.object.material_slot_remove_unused()
oyna_slotlar = {m.name: ("chiroq" if asl.get(m.name) in cfg["chiroq"] else "oyna") for m in glass.data.materials if m}
print("MASHINA_OYNA", nom, len(glass.data.polygons), "kuzov", len(car.data.polygons), oyna_slotlar)
with open(os.path.join(chiqish, "oyna.json"), "w", encoding="utf-8") as f:
    json.dump(oyna_slotlar, f, ensure_ascii=False, indent=1)
# Unreal'da bu slotlarga M_Mashina_Boyoq nusxasi aniq qiymatlar bilan qo'yiladi (glTF materiali qayta importda yangilanmaydi)
boyoq = {m.name: [*cfg["rang"][asl[m.name]][0], *cfg["rang"][asl[m.name]][1:]]
         for m in car.data.materials if m and asl.get(m.name) in cfg["rang"]}
with open(os.path.join(chiqish, "boyoq.json"), "w", encoding="utf-8") as f:
    json.dump(boyoq, f, ensure_ascii=False, indent=1)
for o, fayl in ((car, out), (glass, out.replace(".glb", "_Oyna.glb"))):
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=fayl, export_format='GLB', use_selection=True, export_apply=True,
                              export_yup=True, export_materials='EXPORT', export_cameras=False, export_lights=False)
