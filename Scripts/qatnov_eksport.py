"""Qatnov (yuruvchi) mashinalar uchun Blender eksporti: kuzov g'ildiraklarsiz (SM_<Nom>_Qatnov) va 4 ta g'ildirak
(SM_<Nom>_G0..3, pivoti - g'ildirak o'qi markazi). Koordinatalar turgan mashinalardagi SM_<Nom> bilan bir xil
(o'sha pivot: butun mashina tagining markazi), shuning uchun Unreal'da materiallar SM_<Nom> slot nomlaridan ko'chiriladi
va oynalar uchun mavjud SM_<Nom>_Oyna ishlatiladi (oyna yuzalari bu yerda tashlanadi).

G'ildiraklar: shina materiali yuzalari 4 chorakka (old/orqa x chap/o'ng) bo'linadi -> har biri shina silindri;
silindr ichiga to'liq sig'adigan bog'lanmagan bo'laklar (shina, disk, boltlar, tormoz diski) o'sha g'ildirakka o'tadi.
Saved/<Nom>Source/qatnov.json: g'ildiraklar markazi (m, Blender) va radiusi.

blender -b --python Scripts/qatnov_eksport.py -- <Nom>      (Nom: Cobalt, Damas, Spark, Nexia, Malibu, BMW)
(read_factory_settings ishlatilmaydi - u Blender kengaytmalarining Python kutubxonalarini tozalab yuboradi)
"""
import json
import os
import re
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

MANBA = r"D:\Unreal Projects\creat_3d_asset"
LOYIHA = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
MASHINALAR = {
    "Cobalt": {"fayl": r"Vehicles\Cobalt\Chevrolet_Cobalt.blend", "oyna": ["Automotive solar glass"], "shina": ["Tire sidewall"]},
    "Damas": {"fayl": r"Vehicles\Damas\Damas_4_wheels_color.blend", "oyna": [], "shina": ["secondary"]},
    "Spark": {"fayl": "spark.glb", "oyna": ["greenglass", "clearglass"], "shina": ["tire"]},
    "Nexia": {"fayl": "nexia_2.glb", "oyna": ["windowglass", "clearglass"], "shina": ["tire"]},
    "BMW": {"fayl": os.path.join(LOYIHA, "Saved", "BmwSource", "zip", "source", "Untitled3.glb"),
            "oyna": ["TEX.017", "TEX.014"],
            "shina": ["Tire_Shader", "Tire_Shader.002", "Tire_Shader.003", "Tire_Shader.004"]},
    "Malibu": {"fayl": "2019_chevrolet_malibu_premier.glb", "uzunlik": 4.92,
               "oyna": ["glass", "black_glass1", "glass_norm"], "shina": ["tyre"]},
}

nom = sys.argv[sys.argv.index("--") + 1]
cfg = MASHINALAR[nom]
chiqish = os.path.join(LOYIHA, "Saved", nom + "Source")
os.makedirs(chiqish, exist_ok=True)
fayl = os.path.join(MANBA, cfg["fayl"])

# ---------------------------------------------------------------- yuklash va birlashtirish (eksport skriptlari kabi)
if fayl.endswith(".blend"):
    bpy.ops.wm.open_mainfile(filepath=fayl)
else:
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    bpy.ops.import_scene.gltf(filepath=fayl)
vl = bpy.context.view_layer
meshes = [o for o in vl.objects if o.type == 'MESH' and o.visible_get()
          and not any(k in o.name for k in ("Studio", "Cyclorama"))]
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
car.name = car.data.name = nom + "_Qatnov"
asl = {}
for m in car.data.materials:
    if m and not m.name.startswith("MI_"):
        yangi = "MI_%s_%s" % (nom, re.sub(r"[^A-Za-z0-9]+", "_", m.name).strip("_"))
        asl[yangi] = m.name
        m.name = yangi
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
me = car.data


def koordinatalar():
    co = np.empty(len(me.vertices) * 3, dtype=np.float64)
    me.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


co = koordinatalar()
if cfg.get("uzunlik"):
    k = cfg["uzunlik"] / max(np.ptp(co[:, 0]), np.ptp(co[:, 1]))
    me.transform(Matrix.Scale(k, 4))
    co = koordinatalar()
mn, mx = co.min(0), co.max(0)
shift = Vector(((mn[0] + mx[0]) / 2, (mn[1] + mx[1]) / 2, mn[2]))
me.transform(Matrix.Translation(-shift))
co = koordinatalar()
print("QATNOV bbox", nom, np.round(co.min(0), 3), np.round(co.max(0), 3))

# ---------------------------------------------------------------- oyna yuzalarini tashlash (alohida SM_<Nom>_Oyna bor)
bm = bmesh.new()
bm.from_mesh(me)
oyna_idx = {i for i, m in enumerate(me.materials) if m and asl.get(m.name) in cfg["oyna"]}
if oyna_idx:
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index in oyna_idx], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(me)
bm.free()
me.update()
co = koordinatalar()

# ---------------------------------------------------------------- g'ildiraklar: shina yuzalari -> 4 chorak
nf = len(me.polygons)
mat = np.empty(nf, dtype=np.int32)
me.polygons.foreach_get("material_index", mat)
loop_start = np.empty(nf, dtype=np.int32)
me.polygons.foreach_get("loop_start", loop_start)
loop_total = np.empty(nf, dtype=np.int32)
me.polygons.foreach_get("loop_total", loop_total)
loop_v = np.empty(len(me.loops), dtype=np.int32)
me.loops.foreach_get("vertex_index", loop_v)
shina_idx = [i for i, m in enumerate(me.materials) if m and asl.get(m.name) in cfg["shina"]]
shina_v = np.unique(loop_v[np.isin(np.repeat(mat, loop_total), shina_idx)])
sp = co[shina_v]
# old/orqa o'qi - mashinaning uzun o'qi; yon - ikkinchisi
uzun = 0 if np.ptp(co[:, 0]) > np.ptp(co[:, 1]) else 1
yon = 1 - uzun
gildiraklar = []
for su in (1, -1):
    for sy in (1, -1):
        p = sp[(np.sign(sp[:, uzun]) == su) & (np.sign(sp[:, yon]) == sy)]
        lo, hi = p.min(0), p.max(0)
        c = (lo + hi) / 2
        # radius: g'ildirak yerga tegib turadi (o'q markazi balandligi) - "shina" materiali faqat qalpoq bo'lsa ham to'g'ri
        gildiraklar.append({"markaz": c, "radius": max((hi[2] - lo[2]) / 2, c[2]), "yon_lo": lo[yon], "yon_hi": hi[yon], "uzun": su, "tomon": sy})
        if nom == "Cobalt":
            # Shina geometriyasidan haqiqiy o'qni topamiz. Faqat yaw tuzatiladi:
            # vertikal radius, yerdagi kontakt va mashinadagi markaz o'zgarmaydi.
            axle = np.linalg.svd(p - p.mean(axis=0), full_matrices=False)[2][-1]
            if axle[yon] < 0:
                axle = -axle
            yaw = np.arctan2(-axle[0], axle[1])
            gildiraklar[-1]["tekisla_yaw"] = -yaw if abs(yaw) > np.deg2rad(1) else 0.0
        print("QATNOV shina", su, sy, "markaz", np.round(c, 3), "radius", round(max((hi[2] - lo[2]) / 2, c[2]), 3), "en", round(hi[yon] - lo[yon], 3))

# ---------------------------------------------------------------- bog'lanmagan bo'laklar (orollar)
ne = len(me.edges)
ev = np.empty(ne * 2, dtype=np.int64)
me.edges.foreach_get("vertices", ev)
a, b = ev[0::2], ev[1::2]
lab = np.arange(len(co), dtype=np.int64)
while True:
    m = np.minimum(lab[a], lab[b])
    old = lab.copy()
    np.minimum.at(lab, a, m)
    np.minimum.at(lab, b, m)
    lab = lab[lab]
    if np.array_equal(lab, old):
        break
f_lab = lab[loop_v[loop_start]]   # yuzaning oroli - birinchi uchi orqali
orollar = np.unique(lab)
# orol chegaralari
idx = np.argsort(lab)
sl = lab[idx]
chegara = np.flatnonzero(np.diff(sl)) + 1
boshi = np.concatenate(([0], chegara))
o_lo = np.minimum.reduceat(co[idx], boshi, axis=0)
o_hi = np.maximum.reduceat(co[idx], boshi, axis=0)
o_id = sl[boshi]
print("QATNOV orollar", len(o_id))

tanlov = {}   # orol -> g'ildirak
# Cobalt'ning salon ostidagi mayda bo'laklari shina qutisiga kiradi,
# ammo g'ildirak emas va kuzovda qolishi kerak.
kuzov_orollari = set()
if nom == "Cobalt":
    salon = [i for i, m in enumerate(me.materials) if m and "Cabin_graphite_fabric" in m.name]
    kuzov_orollari.update(f_lab[np.isin(mat, salon)])
for gi, g in enumerate(gildiraklar):
    c, r = g["markaz"], g["radius"]
    ichki = 0.18   # tormoz diski, gupchak - g'ildirakdan ichkariroq
    if g["tomon"] > 0:
        y_lo, y_hi = g["yon_lo"] - ichki, g["yon_hi"] + 0.08
    else:
        y_lo, y_hi = g["yon_lo"] - 0.08, g["yon_hi"] + ichki
    ok = ((o_lo[:, uzun] >= c[uzun] - r * 1.08) & (o_hi[:, uzun] <= c[uzun] + r * 1.08)
          & (o_lo[:, 2] >= c[2] - r * 1.08) & (o_hi[:, 2] <= c[2] + r * 1.08)
          & (o_lo[:, yon] >= y_lo) & (o_hi[:, yon] <= y_hi))
    for oid in o_id[ok]:
        if oid not in kuzov_orollari:
            tanlov.setdefault(int(oid), gi)
    print("QATNOV g'ildirak", gi, "bo'laklar", int(ok.sum()))

# ---------------------------------------------------------------- ajratish va eksport
info = []
hammasi = np.zeros(len(f_lab), dtype=bool)
for gi, g in enumerate(gildiraklar):
    ids = [oid for oid, w in tanlov.items() if w == gi]
    sel = np.isin(f_lab, ids)
    hammasi |= sel
    wm = me.copy()   # nusxada faqat shu g'ildirak yuzalari qoladi
    wm.name = "%s_G%d" % (nom, gi)
    bm = bmesh.new()
    bm.from_mesh(wm)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in np.flatnonzero(~sel)], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    c = Vector(g["markaz"])
    bmesh.ops.translate(bm, vec=-c, verts=bm.verts)
    if g.get("tekisla_yaw"):
        bmesh.ops.rotate(bm, cent=Vector(), matrix=Matrix.Rotation(g["tekisla_yaw"], 3, 'Z'), verts=bm.verts)
    bm.to_mesh(wm)
    bm.free()
    w = bpy.data.objects.new(wm.name, wm)
    bpy.context.scene.collection.objects.link(w)
    info.append({"nom": "SM_%s_G%d" % (nom, gi), "markaz": list(map(float, c)), "radius": float(g["radius"]),
                 "yuza": len(wm.polygons)})
bm = bmesh.new()   # kuzovdan g'ildiraklarni olib tashlash
bm.from_mesh(me)
bm.faces.ensure_lookup_table()
bmesh.ops.delete(bm, geom=[bm.faces[i] for i in np.flatnonzero(hammasi)], context='FACES')
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(me)
bm.free()
me.update()

print("QATNOV natija", nom, [(i["nom"], i["yuza"], round(i["radius"], 3)) for i in info], "kuzov", len(car.data.polygons))
with open(os.path.join(chiqish, "qatnov.json"), "w", encoding="utf-8") as f:
    json.dump({"gildiraklar": info}, f, indent=1)
for o in [car] + [bpy.data.objects["%s_G%d" % (nom, i)] for i in range(len(info))]:
    bpy.ops.object.select_all(action='DESELECT')
    o.select_set(True)
    vl.objects.active = o
    bpy.ops.object.material_slot_remove_unused()
    nomi = "SM_%s_Qatnov" % nom if o == car else "SM_" + o.name
    bpy.ops.export_scene.gltf(filepath=os.path.join(chiqish, nomi + ".glb"), export_format='GLB', use_selection=True,
                              export_apply=True, export_yup=True, export_materials='EXPORT', export_cameras=False,
                              export_lights=False)
