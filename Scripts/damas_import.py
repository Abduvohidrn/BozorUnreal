"""Damas (Blender'dan GLB) ni /Game/Bozor/Mashinalar/Damas ga import qiladi, oq rangga bo'yaydi (yon tomonda
qizil + to'q kulrang chiziq, old uchida ko'k uchburchak) va bozor atrofidagi yo'llarga Cobalt'lar orasiga qo'yadi.

GLB: Scripts/damas_eksport.py (Blender) -> Saved/DamasSource/SM_Damas.glb
Ishga tushirish (muharrir yopiq):
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/damas_import.py
BOZOR_DAMAS: import - faqat import va material; sinov - joy tanlash, saqlanmaydi; hammasi (standart) - import + joylash + saqlash;
belgi - faqat materiallarga ISM belgisi.
"""
import math
import os
import stat

import unreal as ue

V = ue.Vector
PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
GLB = os.path.join(PROJ, "Saved", "DamasSource", "SM_Damas.glb")
DEST = "/Game/Bozor/Mashinalar/Damas"
MESH = DEST + "/SM_Damas"
OYNA_MAT = "/Game/Bozor/Mashinalar/Cobalt/M_Cobalt_Oyna"
MAP = "/Game/Bozor/Maps/Bozor"
GURUH = "Bozor_Atrof"
SONI = 3   # nechta Damas
LOG = os.path.join(PROJ, "Saved", "damas_import.txt")
REJIM = os.environ.get("BOZOR_DAMAS", "hammasi")
EAL = ue.EditorAssetLibrary
lines = []

# Yon chiziq (sm, yerdan / orqa chetidan): orqa qanot va siljiydigan eshik bo'ylab
CHIZIQ_OXIRI = 196.0          # orqadan, siljiydigan eshikning old cheti oldida
UCHBURCHAK = 22.0             # ko'k uchburchak uzunligi (old tomonga qaragan)
QORA = (76.0, 86.0)           # to'q kulrang tasma
QIZIL = (86.0, 95.0)          # qizil tasma


def log(*a):
    s = " ".join(str(x) for x in a)
    lines.append(s)
    ue.log_warning("DAMAS " + s)
    with open(LOG, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def pipeline():
    path = DEST + "/PL_Damas"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    src = ue.load_asset("/Interchange/Pipelines/DefaultAssetsPipeline.DefaultAssetsPipeline")
    p = ue.AssetToolsHelpers.get_asset_tools().duplicate_asset("PL_Damas", DEST, src)
    p.set_editor_property("asset_type_sub_folders", False)
    p.set_editor_property("scene_name_sub_folder", False)
    p.common_meshes_properties.set_editor_property("recompute_normals", False)
    p.common_meshes_properties.set_editor_property("import_lods", False)
    mp = p.mesh_pipeline
    mp.set_editor_property("import_skeletal_meshes", False)
    mp.set_editor_property("build_nanite", False)   # 97k uchburchak; shaffof oyna Nanite'siz ishlaydi
    mp.set_editor_property("collision", False)
    EAL.save_loaded_asset(p)
    return path


def oq_yon(sm):
    """Mesh lokal o'qlari: uzun o'q (0=X, 1=Y), old tomoni belgisi va bbox. Damas: old tomoni tepasi past (oyna qiya)."""
    b = sm.get_bounding_box()
    ax = 0 if (b.max.x - b.min.x) >= (b.max.y - b.min.y) else 1
    lo, hi = (b.min.x, b.max.x) if ax == 0 else (b.min.y, b.max.y)
    md = sm.get_static_mesh_description(0)
    top = {"bosh": -1e9, "oxir": -1e9}
    for i in range(md.get_vertex_count()):
        p = md.get_vertex_position(ue.VertexID(i))
        t = ((p.x if ax == 0 else p.y) - lo) / (hi - lo)
        if t < 0.1:
            top["bosh"] = max(top["bosh"], p.z)
        elif t > 0.9:
            top["oxir"] = max(top["oxir"], p.z)
    sign = 1 if top["oxir"] < top["bosh"] else -1
    return ax, sign, b, top


def boyoq_material(ax, sign, b):
    """Oq kuzov bo'yog'i, yon tomonida chiziq: rang lokal koordinatadan (ISM nusxasining o'z fazosi) hisoblanadi."""
    path = DEST + "/M_Damas_Boyoq"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mel = ue.MaterialEditingLibrary
    m = ue.AssetToolsHelpers.get_asset_tools().create_asset("M_Damas_Boyoq", DEST, ue.Material, ue.MaterialFactoryNew())
    P = ue.MaterialProperty
    along = ("P.x" if ax == 0 else "P.y")
    lat = ("P.y" if ax == 0 else "P.x")
    lo, hi = (b.min.x, b.max.x) if ax == 0 else (b.min.y, b.max.y)
    # d - orqa chetidan old tomonga masofa (sm)
    d = ("(%s - (%.2f))" % (along, lo)) if sign > 0 else ("((%.2f) - %s)" % (hi, along))
    yon = (min(-b.min.y, b.max.y) if ax == 0 else min(-b.min.x, b.max.x))
    kod = f"""
float d = {d};
float h = P.z;
float l = abs({lat});
float3 c = float3(0.82, 0.83, 0.82);
if (l > 55.0)
{{
    if (d > 0.0 && d < {CHIZIQ_OXIRI:.1f})
    {{
        if (h >= {QIZIL[0]:.1f} && h < {QIZIL[1]:.1f}) c = float3(0.42, 0.004, 0.008);
        else if (h >= {QORA[0]:.1f} && h < {QORA[1]:.1f}) c = float3(0.012, 0.013, 0.015);
    }}
    float u = d - {CHIZIQ_OXIRI:.1f};
    float yarim = 0.5 * ({QIZIL[1]:.1f} - {QORA[0]:.1f});
    float o = 0.5 * ({QIZIL[1]:.1f} + {QORA[0]:.1f});
    if (u >= 0.0 && u < {UCHBURCHAK:.1f} && abs(h - o) < yarim * (1.0 - u / {UCHBURCHAK:.1f})) c = float3(0.008, 0.04, 0.32);
}}
return c;
"""
    lp = mel.create_material_expression(m, ue.MaterialExpressionLocalPosition, -700, 0)
    cu = mel.create_material_expression(m, ue.MaterialExpressionCustom, -400, 0)
    cu.set_editor_property("code", kod)
    cu.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT3)
    cu.set_editor_property("description", "Damas bo'yog'i")
    ci = ue.CustomInput()
    ci.set_editor_property("input_name", "P")
    cu.set_editor_property("inputs", [ci])
    mel.connect_material_expressions(lp, "", cu, "P")
    mel.connect_material_property(cu, "", P.MP_BASE_COLOR)

    def const(v, y):
        e = mel.create_material_expression(m, ue.MaterialExpressionConstant, -400, y)
        e.set_editor_property("r", v)
        return e
    mel.connect_material_property(const(0.0, 200), "", P.MP_METALLIC)
    mel.connect_material_property(const(0.3, 260), "", P.MP_ROUGHNESS)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m)
    log("bo'yoq: uzun o'q", ax, "old belgisi", sign, "yon yarim kenglik", round(yon), "\n", kod)
    return m


def import_damas():
    pl = pipeline()
    mgr = ue.InterchangeManager.get_interchange_manager_scripted()
    params = ue.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines.append(ue.SoftObjectPath(pl))
    mgr.import_asset(DEST, mgr.create_source_data(GLB), params)
    EAL.delete_asset(pl)
    meshes = [p.split(".")[0] for p in EAL.list_assets(DEST, recursive=True, include_folder=False)
              if EAL.find_asset_data(p).asset_class_path.asset_name == "StaticMesh"]
    log("import qilingan mesh'lar:", meshes)
    if MESH not in meshes:
        EAL.rename_asset(meshes[0], MESH)
    sm = ue.load_asset(MESH)
    ue.EditorStaticMeshLibrary.remove_collisions(sm)
    ue.EditorStaticMeshLibrary.add_simple_collisions(sm, ue.ScriptCollisionShapeType.NDOP18)
    ax, sign, b, top = oq_yon(sm)
    log("bbox", b.min, b.max, "tepa", top)
    boyoq = boyoq_material(ax, sign, b)
    oyna = ue.load_asset(OYNA_MAT)
    for i, s in enumerate(sm.static_materials):
        nom = str(s.material_slot_name)
        if nom.endswith("_primary"):
            sm.set_material(i, boyoq)
        elif "_glass" in nom:
            sm.set_material(i, oyna)
    ism_belgisi()
    for p in EAL.list_assets(DEST, recursive=True, include_folder=False):
        EAL.save_asset(p.split(".")[0], only_if_is_dirty=False)
    return sm


def ism_belgisi():
    """Sahnada ISM nusxalari bo'lgani uchun materiallarga 'Instanced Static Meshes' belgisi (aks holda o'yinda chizilmaydi)."""
    yollar = [p.split(".")[0] for p in EAL.list_assets(DEST, recursive=True, include_folder=False)] + [OYNA_MAT]
    for p in yollar:
        a = ue.load_asset(p)
        if isinstance(a, ue.Material) and not a.get_editor_property("used_with_instanced_static_meshes"):
            fayl = os.path.join(PROJ, "Content", *p.split("/")[2:]) + ".uasset"
            os.chmod(fayl, stat.S_IREAD | stat.S_IWRITE)   # git LFS 'lockable'
            a.set_editor_property("used_with_instanced_static_meshes", True)
            ue.MaterialEditingLibrary.recompile_material(a)
            EAL.save_loaded_asset(a, False)
            log("ISM belgisi:", a.get_name())


# ------------------------------------------------------------------ joylash
def _aabb(sm, t):
    b = sm.get_bounding_box()
    pts = [t.transform_location(V(x, y, z)) for x in (b.min.x, b.max.x) for y in (b.min.y, b.max.y) for z in (b.min.z, b.max.z)]
    return (min(p.x for p in pts), max(p.x for p in pts), min(p.y for p in pts), max(p.y for p in pts),
            min(p.z for p in pts), max(p.z for p in pts))


def _kesma_masofa(p, a, b):
    ax, ay, bx, by = a.x, a.y, b.x, b.y
    dx, dy = bx - ax, by - ay
    t = 0.0 if dx == dy == 0 else max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(p[0] - (ax + t * dx), p[1] - (ay + t * dy))


def joylash(sm):
    ue.EditorLoadingAndSavingUtils.load_map(MAP)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    ax, sign, b, _ = oq_yon(sm)
    fyaw = {(0, 1): 0.0, (1, 1): 90.0, (0, -1): 180.0, (1, -1): -90.0}[(ax, sign)]
    yarim_uz = 0.5 * ((b.max.x - b.min.x) if ax == 0 else (b.max.y - b.min.y))
    yarim_en = 78.0   # kuzov (oynasiz) yarim kengligi + oz zahira
    to_siq, yol, cobaltlar, guruh = [], [], [], None
    for a in actors:
        if a.get_actor_label() == GURUH:
            guruh = a
        for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
            m = c.static_mesh
            if not m or m.get_name() in ("SM_Damas", "SM_Cobalt_Oyna"):
                continue
            n = m.get_name()
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                bb = _aabb(m, t)
                if "YOL_" in n:
                    yol.append(bb)
                elif bb[5] > 15.0:   # tekis (asfalt, maysa, dog') emas - to'siq
                    to_siq.append((n, bb))
                if n == "SM_Cobalt":
                    cobaltlar.append(t.translation)
    # avtobus yo'li: uning ustiga mashina qo'yilmaydi
    avt = []
    for a in actors:
        if a.get_class().get_name() == "BozorAvtobusIntro":
            at = a.get_actor_transform()
            for prop in ("kelish_nuqtalari", "ketish_nuqtalari"):
                pts = [at.transform_location(p) for p in a.get_editor_property(prop)]
                avt += list(zip(pts, pts[1:]))
                log("avtobus", prop, [(round(p.x), round(p.y)) for p in pts])
    log("to'siqlar", len(to_siq), "yo'l bo'laklari", len(yol), "cobalt", len(cobaltlar), "avtobus kesmalari", len(avt))

    def bosh_joy(x, y, yaw):
        r = math.radians(yaw)
        ex = abs(math.cos(r)) * yarim_uz + abs(math.sin(r)) * yarim_en
        ey = abs(math.sin(r)) * yarim_uz + abs(math.cos(r)) * yarim_en
        box = (x - ex - 25, x + ex + 25, y - ey - 15, y + ey + 15)
        for n, bb in to_siq:
            if bb[0] < box[1] and bb[1] > box[0] and bb[2] < box[3] and bb[3] > box[2] and bb[4] < 200:
                return False
        # to'rt burchagi ham yo'l ustida bo'lsin
        for px in (x - ex, x + ex):
            for py in (y - ey, y + ey):
                if not any(r_[0] <= px <= r_[1] and r_[2] <= py <= r_[3] for r_ in yol):
                    return False
        return all(_kesma_masofa((x, y), p, q) > 320 for p, q in avt)

    # o'ng tomonlama harakat: bozorga yaqin bo'lak, yo'l chetida (bordyurdan ~1.2 m)
    yollar = [
        ("janub", [(x, -4530.0, 180.0) for x in range(-600, 8400, 50)]),
        ("shimol", [(x, 4530.0, 0.0) for x in range(-600, 8400, 50)]),
        ("g'arb", [(-4230.0, y, 90.0) for y in range(-3900, 2800, 50)]),
    ]
    tanlangan = []
    for nom, nomzodlar in yollar:
        yaxshi = [(x, y, yaw) for x, y, yaw in nomzodlar if bosh_joy(x, y, yaw)]
        if not yaxshi:
            log(nom, "yo'lida bo'sh joy yo'q")
            continue

        def baho(c):   # biror Cobalt'dan ~7 m da (aralash qator), bozorga yaqinroq
            yaqin = min((math.hypot(c[0] - p.x, c[1] - p.y) for p in cobaltlar), default=1e4)
            return abs(yaqin - 700.0) + 0.05 * math.hypot(c[0] - 2000, c[1])
        x, y, yaw = min(yaxshi, key=baho)
        tanlangan.append((nom, x, y, yaw))
        log(nom, "yo'li: bo'sh nomzodlar", len(yaxshi), "-> Damas", round(x), round(y), "yaw", yaw)
    tanlangan = tanlangan[:SONI]
    tlar = []
    for nom, x, y, yaw in tanlangan:
        r = ue.Rotator(roll=0, pitch=0, yaw=yaw - fyaw)
        tt = ue.Transform(V(0, 0, 0), r, V(1, 1, 1))
        mid = V((b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2, 0)
        loc = V(x, y, 0) - tt.transform_location(mid)
        tlar.append(ue.Transform(loc, r, V(1, 1, 1)))
    bor = [c for c in guruh.get_components_by_class(ue.InstancedStaticMeshComponent)
           if c.static_mesh and c.static_mesh.get_name() == "SM_Damas"]
    if bor:
        bor[0].modify()
        bor[0].clear_instances()
        for t in tlar:
            bor[0].add_instance(t, True)
    else:
        guruh.add_instances(sm, [], tlar, True, True)
    log("Damas nusxalari:", len(tlar))
    if REJIM == "hammasi":
        umap = os.path.join(PROJ, "Content", "Bozor", "Maps", "Bozor.umap")
        os.chmod(umap, stat.S_IREAD | stat.S_IWRITE)   # git LFS 'lockable' - faqat o'qish uchun
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
        log("sahna saqlandi:", ue.EditorLoadingAndSavingUtils.save_map(world, MAP))


def main():
    open(LOG, "w").close()
    sm = import_damas() if REJIM in ("import", "hammasi") else ue.load_asset(MESH)
    if REJIM == "belgi":
        ism_belgisi()
    b = sm.get_bounding_box()
    log("SM_Damas o'lchami (sm):", round(b.max.x - b.min.x), round(b.max.y - b.min.y), round(b.max.z - b.min.z))
    if REJIM in ("hammasi", "sinov"):
        joylash(sm)


main()
