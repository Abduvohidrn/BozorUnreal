"""Spark (Blender'da oq rangga bo'yalgan GLB) ni /Game/Bozor/Mashinalar/Spark ga import qiladi va bozor atrofidagi
yo'llar chetiga (Cobalt va Damas bilan birga) qo'yadi. Kuzov - Nanite, oynalar - alohida Nanite'siz shaffof mesh.

GLB: Scripts/spark_eksport.py (Blender) -> Saved/SparkSource/SM_Spark.glb, SM_Spark_Oyna.glb
Ishga tushirish (muharrir yopiq):
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/spark_import.py
BOZOR_SPARK: import - faqat import; sinov - joy tanlash, saqlanmaydi; hammasi (standart) - import + joylash + saqlash.
"""
import math
import os
import stat

import unreal as ue

V = ue.Vector
PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
GLB = os.path.join(PROJ, "Saved", "SparkSource", "SM_Spark.glb")
DEST = "/Game/Bozor/Mashinalar/Spark"
MESH = DEST + "/SM_Spark"
OYNA = DEST + "/SM_Spark_Oyna"
OYNA_MAT = "/Game/Bozor/Mashinalar/Cobalt/M_Cobalt_Oyna"   # to'q tusli avtomobil shishasi (tonirovka)
MAP = "/Game/Bozor/Maps/Bozor"
GURUH = "Bozor_Atrof"
SONI = 2
LOG = os.path.join(PROJ, "Saved", "spark_import.txt")
REJIM = os.environ.get("BOZOR_SPARK", "hammasi")
EAL = ue.EditorAssetLibrary
lines = []


def log(*a):
    s = " ".join(str(x) for x in a)
    lines.append(s)
    ue.log_warning("SPARK " + s)
    with open(LOG, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def yozish_ruxsati(fayl=None):
    """git LFS 'lockable' fayllar checkout'dan keyin faqat o'qish uchun - saqlashdan oldin yozish ruxsati."""
    fayllar = [fayl] if fayl else [os.path.join(r, f) for r, _, fs in os.walk(os.path.join(PROJ, "Content", *DEST.split("/")[2:])) for f in fs]
    for f in fayllar:
        if os.path.exists(f):
            os.chmod(f, stat.S_IREAD | stat.S_IWRITE)


def pipeline(nanite):
    path = DEST + "/PL_Spark"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    src = ue.load_asset("/Interchange/Pipelines/DefaultAssetsPipeline.DefaultAssetsPipeline")
    p = ue.AssetToolsHelpers.get_asset_tools().duplicate_asset("PL_Spark", DEST, src)
    p.set_editor_property("asset_type_sub_folders", False)
    p.set_editor_property("scene_name_sub_folder", False)
    # qayta importda mavjud materiallar ham GLB dagi yangi ranglar bilan yangilansin (standart - o'zgartirmaydi)
    p.set_editor_property("reimport_strategy", ue.ReimportStrategyFlags.APPLY_PIPELINE_PROPERTIES)
    p.common_meshes_properties.set_editor_property("recompute_normals", False)
    p.common_meshes_properties.set_editor_property("import_lods", False)
    mp = p.mesh_pipeline
    mp.set_editor_property("import_skeletal_meshes", False)
    mp.set_editor_property("build_nanite", nanite)
    mp.set_editor_property("collision", False)
    EAL.save_loaded_asset(p)
    return path


def _glb(fayl, nanite, nom):
    pl = pipeline(nanite)
    mgr = ue.InterchangeManager.get_interchange_manager_scripted()
    params = ue.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines.append(ue.SoftObjectPath(pl))
    oldin = set(p.split(".")[0] for p in EAL.list_assets(DEST, recursive=True, include_folder=False))
    mgr.import_asset(DEST, mgr.create_source_data(fayl), params)
    EAL.delete_asset(pl)
    meshes = [p.split(".")[0] for p in EAL.list_assets(DEST, recursive=True, include_folder=False)
              if EAL.find_asset_data(p).asset_class_path.asset_name == "StaticMesh"]
    log("import", os.path.basename(fayl), "->", meshes)
    if nom not in meshes:
        yangi = [m for m in meshes if m not in oldin and m != nom]
        EAL.rename_asset(yangi[0], nom)
    return ue.load_asset(nom)


def chiroq_material():
    """Fara va chiroqlar ustidagi tiniq plastik: deyarli shaffof, yaltiroq."""
    path = DEST + "/M_Spark_Chiroq"
    if EAL.does_asset_exist(path):
        return ue.load_asset(path)
    mel = ue.MaterialEditingLibrary
    m = ue.AssetToolsHelpers.get_asset_tools().create_asset("M_Spark_Chiroq", DEST, ue.Material, ue.MaterialFactoryNew())
    m.set_editor_property("blend_mode", ue.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", ue.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    P = ue.MaterialProperty
    for v, prop, y in ((0.9, P.MP_BASE_COLOR, -100), (1.0, P.MP_SPECULAR, 0), (0.02, P.MP_ROUGHNESS, 60), (0.18, P.MP_OPACITY, 120)):
        e = mel.create_material_expression(m, ue.MaterialExpressionConstant, -300, y)
        e.set_editor_property("r", v)
        mel.connect_material_property(e, "", prop)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m)
    return m


def belgilar():
    """Materiallarga Nanite (kuzov) va ISM belgilari - aks holda o'yinda qo'pol zaxira mesh yoki umuman chizilmaydi."""
    for p in [p.split(".")[0] for p in EAL.list_assets(DEST, recursive=True, include_folder=False)] + [OYNA_MAT]:
        a = ue.load_asset(p)
        if not isinstance(a, ue.Material):
            continue
        shaffof = a.get_editor_property("blend_mode") == ue.BlendMode.BLEND_TRANSLUCENT
        kerak = [("used_with_instanced_static_meshes", True)] + ([] if shaffof else [("used_with_nanite", True)])
        if all(a.get_editor_property(k) for k, _ in kerak):
            continue
        yozish_ruxsati(os.path.join(PROJ, "Content", *p.split("/")[2:]) + ".uasset")
        for k, v in kerak:
            a.set_editor_property(k, v)
        ue.MaterialEditingLibrary.recompile_material(a)
        EAL.save_loaded_asset(a, False)
        log("belgi:", a.get_name())


def import_spark():
    yozish_ruxsati()
    sm = _glb(GLB, True, MESH)
    om = _glb(GLB.replace(".glb", "_Oyna.glb"), False, OYNA)
    ue.EditorStaticMeshLibrary.remove_collisions(sm)
    ue.EditorStaticMeshLibrary.add_simple_collisions(sm, ue.ScriptCollisionShapeType.NDOP18)
    ue.EditorStaticMeshLibrary.remove_collisions(om)
    oyna, chiroq = ue.load_asset(OYNA_MAT), chiroq_material()
    for i, s in enumerate(om.static_materials):
        om.set_material(i, chiroq if "clearglass" in str(s.material_slot_name) else oyna)
    for p in EAL.list_assets(DEST, recursive=True, include_folder=False):
        n = p.split(".")[0]
        if n.split("/")[-1] in ("MI_Spark_greenglass", "MI_Spark_clearglass") and not EAL.find_package_referencers_for_asset(n):
            EAL.delete_asset(n)
    belgilar()
    for p in EAL.list_assets(DEST, recursive=True, include_folder=False):
        EAL.save_asset(p.split(".")[0], only_if_is_dirty=False)
    return sm


# ------------------------------------------------------------------ joylash
def oq_yon(sm):
    """Uzun o'q (0=X, 1=Y), old tomoni belgisi (kapot past - old) va bbox."""
    b = sm.get_bounding_box()
    ax = 0 if (b.max.x - b.min.x) >= (b.max.y - b.min.y) else 1
    lo, hi = (b.min.x, b.max.x) if ax == 0 else (b.min.y, b.max.y)
    md = sm.get_static_mesh_description(0)
    top = {"bosh": -1e9, "oxir": -1e9}
    for i in range(0, md.get_vertex_count(), 3):
        p = md.get_vertex_position(ue.VertexID(i))
        t = ((p.x if ax == 0 else p.y) - lo) / (hi - lo)
        if t < 0.12:
            top["bosh"] = max(top["bosh"], p.z)
        elif t > 0.88:
            top["oxir"] = max(top["oxir"], p.z)
    return ax, (1 if top["oxir"] < top["bosh"] else -1), b, top


def _aabb(sm, t):
    b = sm.get_bounding_box()
    pts = [t.transform_location(V(x, y, z)) for x in (b.min.x, b.max.x) for y in (b.min.y, b.max.y) for z in (b.min.z, b.max.z)]
    return (min(p.x for p in pts), max(p.x for p in pts), min(p.y for p in pts), max(p.y for p in pts),
            min(p.z for p in pts), max(p.z for p in pts))


def _kesma(p, a, b):
    dx, dy = b.x - a.x, b.y - a.y
    t = 0.0 if dx == dy == 0 else max(0.0, min(1.0, ((p[0] - a.x) * dx + (p[1] - a.y) * dy) / (dx * dx + dy * dy)))
    return math.hypot(p[0] - (a.x + t * dx), p[1] - (a.y + t * dy))


def joylash(sm):
    ue.EditorLoadingAndSavingUtils.load_map(MAP)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    ax, sign, b, top = oq_yon(sm)
    log("Spark bbox", b.min, b.max, "uzun o'q", ax, "old", sign, "tepa", top)
    fyaw = {(0, 1): 0.0, (1, 1): 90.0, (0, -1): 180.0, (1, -1): -90.0}[(ax, sign)]
    yarim_uz = 0.5 * ((b.max.x - b.min.x) if ax == 0 else (b.max.y - b.min.y))
    yarim_en = 82.0
    to_siq, yol, mashinalar, guruh = [], [], [], None
    for a in actors:
        if a.get_actor_label() == GURUH:
            guruh = a
        for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
            m = c.static_mesh
            if not m or m.get_name() in ("SM_Spark", "SM_Spark_Oyna", "SM_Cobalt_Oyna"):
                continue
            n = m.get_name()
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                bb = _aabb(m, t)
                if "YOL_" in n:
                    yol.append(bb)
                elif bb[5] > 15.0:
                    to_siq.append((n, bb))
                if n in ("SM_Cobalt", "SM_Damas"):
                    mashinalar.append(t.translation)
    avt = []
    for a in actors:
        if a.get_class().get_name() == "BozorAvtobusIntro":
            at = a.get_actor_transform()
            for prop in ("kelish_nuqtalari", "ketish_nuqtalari"):
                pts = [at.transform_location(p) for p in a.get_editor_property(prop)]
                avt += list(zip(pts, pts[1:]))
    log("to'siqlar", len(to_siq), "yo'l", len(yol), "mashinalar", len(mashinalar), "avtobus kesmalari", len(avt))

    def bosh(x, y, yaw):
        r = math.radians(yaw)
        ex = abs(math.cos(r)) * yarim_uz + abs(math.sin(r)) * yarim_en
        ey = abs(math.sin(r)) * yarim_uz + abs(math.cos(r)) * yarim_en
        box = (x - ex - 60, x + ex + 60, y - ey - 15, y + ey + 15)   # oldi-orqasida ~0.6 m bo'sh
        for _, bb in to_siq:
            if bb[0] < box[1] and bb[1] > box[0] and bb[2] < box[3] and bb[3] > box[2] and bb[4] < 200:
                return False
        for px in (x - ex, x + ex):
            for py in (y - ey, y + ey):
                if not any(r_[0] <= px <= r_[1] and r_[2] <= py <= r_[3] for r_ in yol):
                    return False
        return all(_kesma((x, y), p, q) > 320 for p, q in avt)

    yollar = [
        ("shimol", [(x, 4530.0, 0.0) for x in range(-600, 8400, 50)]),
        ("janub", [(x, -4530.0, 180.0) for x in range(-600, 8400, 50)]),
        ("g'arb", [(-4230.0, y, 90.0) for y in range(-3900, 2800, 50)]),
    ]
    tanlangan = []
    for nom, nomzodlar in yollar:
        yaxshi = [c for c in nomzodlar if bosh(*c)]
        if not yaxshi:
            log(nom, "yo'lida bo'sh joy yo'q")
            continue

        def baho(c):   # boshqa mashinadan ~6 m da (bir qatorda), bozorga yaqinroq
            yaqin = min((math.hypot(c[0] - p.x, c[1] - p.y) for p in mashinalar), default=1e4)
            return abs(yaqin - 600.0) + 0.05 * math.hypot(c[0] - 2000, c[1])
        x, y, yaw = min(yaxshi, key=baho)
        tanlangan.append((nom, x, y, yaw))
        log(nom, "yo'li: nomzodlar", len(yaxshi), "-> Spark", round(x), round(y), "yaw", yaw)
    tlar = []
    mid = V((b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2, 0)
    for nom, x, y, yaw in tanlangan[:SONI]:
        r = ue.Rotator(roll=0, pitch=0, yaw=yaw - fyaw)
        loc = V(x, y, 0) - ue.Transform(V(0, 0, 0), r, V(1, 1, 1)).transform_location(mid)
        tlar.append(ue.Transform(loc, r, V(1, 1, 1)))
    for mesh, col, soya in ((sm, True, True), (ue.load_asset(OYNA), False, False)):
        bor = [c for c in guruh.get_components_by_class(ue.InstancedStaticMeshComponent)
               if c.static_mesh and c.static_mesh.get_name() == mesh.get_name()]
        if bor:
            bor[0].modify()
            bor[0].clear_instances()
            for t in tlar:
                bor[0].add_instance(t, True)
        else:
            guruh.add_instances(mesh, [], tlar, soya, col)
    log("Spark nusxalari:", len(tlar))
    if REJIM == "hammasi":
        yozish_ruxsati(os.path.join(PROJ, "Content", "Bozor", "Maps", "Bozor.umap"))
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
        log("sahna saqlandi:", ue.EditorLoadingAndSavingUtils.save_map(world, MAP))


def main():
    open(LOG, "w").close()
    sm = import_spark() if REJIM in ("import", "hammasi") else ue.load_asset(MESH)
    b = sm.get_bounding_box()
    log("SM_Spark o'lchami (sm):", round(b.max.x - b.min.x), round(b.max.y - b.min.y), round(b.max.z - b.min.z),
        "nanite:", sm.get_editor_property("nanite_settings").enabled)
    if REJIM in ("hammasi", "sinov"):
        joylash(sm)


main()
