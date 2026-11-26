"""Mashinalar: kolliziya va turgan mashinalar joyi.

1) Kolliziya: har bir mashina mesh'iga (SM_<Nom>, SM_<Nom>_Qatnov) ikkita quti - kuzov pasti va salon (oddiy kolliziya;
   NDOP18 Nanite mesh'larda yaratilmagan edi - o'yinchi mashina ichiga kirib ketardi).
2) Turgan mashinalar (BozorMeshGroup ISM nusxalari) yo'l chetiga, o'ng tomonlama harakat yo'nalishida qo'yiladi va
   qatnov bo'laklaridan tashqariga (x >= 3800) ko'chiriladi: shimoliy yo'lning janubiy yarmi - g'arbga qarab,
   janubiy yo'lning shimoliy yarmi - sharqqa qarab, g'arbiy yo'lning sharqiy yarmi - janubga qarab.
   Oynalar (SM_<Nom>_Oyna) bir xil transform bilan birga ko'chadi.

UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/mashina_tuzat.py   (muharrir yopiq)
"""
import math
import os
import stat

import unreal as ue

V = ue.Vector
PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
MAP = "/Game/Bozor/Maps/Bozor"
EAL = ue.EditorAssetLibrary
OLD_YAW = {"Cobalt": 180.0, "Damas": -90.0, "Spark": 90.0, "Nexia": 90.0, "Malibu": 90.0, "BMW": 90.0}
# (mashina, eski joyi taxminan (x, y) - qaysi nusxa ekanini topish uchun, yangi markaz (x, y), old tomoni yo'nalishi)
JOYLAR = [
    ("Cobalt", (732, 4700), (3800, 4530), 180.0),
    ("Damas", (1400, 4530), (4400, 4530), 180.0),
    ("Spark", (2000, 4530), (5000, 4530), 180.0),
    ("Nexia", (2600, 4530), (6200, 4530), 180.0),
    ("Damas", (850, -4530), (3800, -4530), 0.0),
    ("Spark", (1450, -4530), (4400, -4530), 0.0),
    ("Nexia", (2050, -4530), (5000, -4530), 0.0),
    ("Damas", (-4230, 2750), (-4230, 2750), -90.0),
    ("Malibu", (-4230, 2150), (-4230, 2150), -90.0),
]


def yozish(fayl):
    if os.path.exists(fayl):
        os.chmod(fayl, stat.S_IREAD | stat.S_IWRITE)   # git LFS 'lockable'


def kolliziya(mesh, nom):
    """Kuzov pasti (g'ildirak tepasigacha, ko'zgusiz) va salon (oynalar va tom) - ikki quti."""
    md = mesh.get_static_mesh_description(0)
    n = md.get_vertex_count()
    qadam = max(1, n // 60000)
    pts = [md.get_vertex_position(ue.VertexID(i)) for i in range(0, n, qadam)]
    pts = [p for p in pts if p.z > -1e5]
    zmax = max(p.z for p in pts)
    ax = 0 if OLD_YAW[nom] in (0.0, 180.0) else 1   # uzun o'q

    def oraliq(vals, a=0.02, b=0.98):
        vals = sorted(vals)
        return vals[int(a * (len(vals) - 1))], vals[int(b * (len(vals) - 1))]

    pastki = [p for p in pts if p.z < zmax * 0.55]
    yuqori = [p for p in pts if p.z > zmax * 0.62]
    qutilar = []
    for qism, z0, z1, yon_kes in ((pastki, max(12.0, zmax * 0.12), zmax * 0.58, 0.01), (yuqori, zmax * 0.55, zmax, 0.05)):
        ux = oraliq([(p.x if ax == 0 else p.y) for p in qism], 0.005, 0.995)
        yx = oraliq([(p.y if ax == 0 else p.x) for p in qism], yon_kes, 1 - yon_kes)
        b = ue.KBoxElem()
        cx, cy = (ux[0] + ux[1]) / 2, (yx[0] + yx[1]) / 2
        sx, sy = ux[1] - ux[0], yx[1] - yx[0]
        if ax == 1:
            cx, cy, sx, sy = cy, cx, sy, sx
        b.set_editor_property("center", V(cx, cy, (z0 + z1) / 2))
        b.set_editor_property("x", sx)
        b.set_editor_property("y", sy)
        b.set_editor_property("z", z1 - z0)
        qutilar.append(b)
    bs = mesh.get_editor_property("body_setup")
    ag = bs.get_editor_property("agg_geom")
    ag.set_editor_property("box_elems", qutilar)
    ag.set_editor_property("convex_elems", [])
    bs.set_editor_property("agg_geom", ag)
    bs.set_editor_property("collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_DEFAULT)
    mesh.modify()
    yozish(os.path.join(PROJ, "Content", *mesh.get_path_name().split(".")[0].split("/")[2:]) + ".uasset")
    EAL.save_loaded_asset(mesh, False)
    ue.log_warning("TUZAT kolliziya %s: %s" % (mesh.get_name(), [(round(q.get_editor_property("x")), round(q.get_editor_property("y")), round(q.get_editor_property("z"))) for q in qutilar]))


def joylar():
    ue.EditorLoadingAndSavingUtils.load_map(MAP)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    comps = {}
    for a in actors:
        for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
            if c.static_mesh:
                comps.setdefault(c.static_mesh.get_name(), []).append(c)
    for nom, eski, yangi, old in JOYLAR:
        topildi = False
        for c in comps.get("SM_" + nom, []):
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                # eski joyida yoki allaqachon yangi joyida (skript qayta ishga tushirilganda)
                if min(math.hypot(t.translation.x - eski[0], t.translation.y - eski[1]),
                       math.hypot(t.translation.x - yangi[0], t.translation.y - yangi[1])) > 250:
                    continue
                yaw = old - OLD_YAW[nom]
                yt = ue.Transform(V(yangi[0], yangi[1], t.translation.z), ue.Rotator(roll=0, pitch=0, yaw=yaw), V(1, 1, 1))
                for oc in comps.get("SM_%s_Oyna" % nom, []):   # shu mashina oynasi
                    for j in range(oc.get_instance_count()):
                        if oc.get_instance_transform(j, True).translation.distance(t.translation) < 5:
                            oc.modify()
                            oc.update_instance_transform(j, yt, True, True, True)
                c.modify()
                c.update_instance_transform(i, yt, True, True, True)
                ue.log_warning("TUZAT %s (%d,%d) yaw %d -> (%d,%d) yaw %d" % (nom, t.translation.x, t.translation.y,
                                                                            t.rotation.rotator().yaw, yangi[0], yangi[1], yaw))
                topildi = True
                break
            if topildi:
                break
        if not topildi:
            ue.log_warning("TUZAT topilmadi: %s %s" % (nom, eski))
    yozish(os.path.join(PROJ, "Content", "Bozor", "Maps", "Bozor.umap"))
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    ue.log_warning("TUZAT saqlandi: %s" % ue.EditorLoadingAndSavingUtils.save_map(world, MAP))


def main():
    rejim = os.environ.get("BOZOR_TUZAT", "hammasi")
    if rejim in ("hammasi", "kolliziya"):
        tanlov = [n for n in os.environ.get("TUZAT_MASHINALAR", "").split(",") if n] or list(OLD_YAW)
        for nom in tanlov:
            for p in ("/Game/Bozor/Mashinalar/%s/SM_%s" % (nom, nom), "/Game/Bozor/Mashinalar/%s/Qatnov/SM_%s_Qatnov" % (nom, nom)):
                if EAL.does_asset_exist(p):   # BMW'da qatnov varianti yo'q
                    kolliziya(ue.load_asset(p), nom)
    if rejim in ("hammasi", "joy"):
        joylar()


main()
