"""Cobalt (Blender'dan GLB) ni /Game/Bozor/Mashinalar/Cobalt ga import qiladi va bozor atrofidagi
Damas/Labo mashinalarini (BozorMeshGroup ISM nusxalari) Cobalt bilan almashtiradi.

GLB: Scripts/cobalt_eksport.py (Blender) -> Saved/CobaltSource/SM_Cobalt.glb
Ishga tushirish (muharrir yopiq):
  UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/cobalt_import.py
BOZOR_COBALT=import - faqat import; sinov - almashtirish saqlanmasdan; hammasi (standart) - import qilingan Cobalt bilan almashtirib, sahnani saqlaydi;
oyna - qayta import (kuzov + oyna) va sahnaga oyna nusxalarini qo'shib saqlaydi; nanite - faqat materiallarga Nanite belgisi.
"""
import os
import stat

import unreal as ue

V = ue.Vector
PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
GLB = os.path.join(PROJ, "Saved", "CobaltSource", "SM_Cobalt.glb")
DEST = "/Game/Bozor/Mashinalar/Cobalt"
MESH = DEST + "/SM_Cobalt"
OYNA = DEST + "/SM_Cobalt_Oyna"
MAP = "/Game/Bozor/Maps/Bozor"
ESKI = ("SM_SM_Damas_LOD0", "SM_SM_Labo_LOD0")
LOG = os.path.join(PROJ, "Saved", "cobalt_import.txt")
REJIM = os.environ.get("BOZOR_COBALT", "hammasi")
EAL = ue.EditorAssetLibrary
lines = []


def log(*a):
    s = " ".join(str(x) for x in a)
    lines.append(s)
    ue.log_warning("COBALT " + s)
    with open(LOG, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def pipeline():
    path = DEST + "/PL_Cobalt"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    src = ue.load_asset("/Interchange/Pipelines/DefaultAssetsPipeline.DefaultAssetsPipeline")
    p = ue.AssetToolsHelpers.get_asset_tools().duplicate_asset("PL_Cobalt", DEST, src)
    p.set_editor_property("asset_type_sub_folders", False)
    p.set_editor_property("scene_name_sub_folder", False)
    p.common_meshes_properties.set_editor_property("recompute_normals", False)
    p.common_meshes_properties.set_editor_property("import_lods", False)
    mp = p.mesh_pipeline
    mp.set_editor_property("import_skeletal_meshes", False)
    mp.set_editor_property("build_nanite", True)
    mp.set_editor_property("collision", False)
    EAL.save_loaded_asset(p)
    return path


def _glb(fayl, pl):
    """GLB ni DEST ga import qiladi, yangi paydo bo'lgan (yoki yangilangan) StaticMesh yo'lini qaytaradi."""
    mgr = ue.InterchangeManager.get_interchange_manager_scripted()
    params = ue.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines.append(ue.SoftObjectPath(pl))
    oldin = set(EAL.list_assets(DEST, recursive=True, include_folder=False))
    mgr.import_asset(DEST, mgr.create_source_data(fayl), params)
    nom = "SM_" + os.path.splitext(os.path.basename(fayl))[0].replace("SM_", "")
    meshes = [p.split(".")[0] for p in EAL.list_assets(DEST, recursive=True, include_folder=False)
              if EAL.find_asset_data(p).asset_class_path.asset_name == "StaticMesh"]
    log("import", os.path.basename(fayl), "-> mesh'lar:", meshes)
    if DEST + "/" + nom in meshes:
        return DEST + "/" + nom
    yangi = [m for m in meshes if not any(o.startswith(m + ".") for o in oldin)]
    return yangi[0]


def oyna_material():
    """To'q tusli avtomobil shishasi: yarim shaffof (chetdan qaraganda Fresnel bo'yicha qalinroq), past roughness, aks ettiradi."""
    path = DEST + "/M_Cobalt_Oyna"
    if EAL.does_asset_exist(path):   # Damas ham ishlatadi - o'chirilmaydi, borini qoldiradi
        return ue.load_asset(path)
    mel = ue.MaterialEditingLibrary
    m = ue.AssetToolsHelpers.get_asset_tools().create_asset("M_Cobalt_Oyna", DEST, ue.Material, ue.MaterialFactoryNew())
    m.set_editor_property("blend_mode", ue.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", ue.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property("two_sided", True)
    P = ue.MaterialProperty

    def const(v, x, y):
        e = mel.create_material_expression(m, ue.MaterialExpressionConstant, x, y)
        e.set_editor_property("r", v)
        return e
    rang = mel.create_material_expression(m, ue.MaterialExpressionConstant3Vector, -400, -200)
    rang.set_editor_property("constant", ue.LinearColor(0.012, 0.016, 0.02, 1))
    mel.connect_material_property(rang, "", P.MP_BASE_COLOR)
    mel.connect_material_property(const(0.0, -400, -60), "", P.MP_METALLIC)
    mel.connect_material_property(const(1.0, -400, 0), "", P.MP_SPECULAR)
    mel.connect_material_property(const(0.03, -400, 60), "", P.MP_ROUGHNESS)
    fr = mel.create_material_expression(m, ue.MaterialExpressionFresnel, -600, 200)
    fr.set_editor_property("exponent", 4.0)
    fr.set_editor_property("base_reflect_fraction", 0.0)
    lerp = mel.create_material_expression(m, ue.MaterialExpressionLinearInterpolate, -400, 200)
    lerp.set_editor_property("const_a", 0.5)
    lerp.set_editor_property("const_b", 0.95)
    mel.connect_material_expressions(fr, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", P.MP_OPACITY)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m)
    return m


def import_cobalt():
    yozish_ruxsati()
    pl = pipeline()
    body = _glb(GLB, pl)
    oyna = _glb(GLB.replace(".glb", "_Oyna.glb"), pl)
    EAL.delete_asset(pl)
    if body != MESH:
        EAL.rename_asset(body, MESH)
    if oyna != OYNA:
        EAL.rename_asset(oyna, OYNA)
    sm = ue.load_asset(MESH)
    # kolliziya: 18-DOP (kuzov shakliga yaqin, oddiy) - o'yinchi/NPC mashina ichidan o'tmaydi
    ue.EditorStaticMeshLibrary.remove_collisions(sm)
    ue.EditorStaticMeshLibrary.add_simple_collisions(sm, ue.ScriptCollisionShapeType.NDOP18)
    om = ue.load_asset(OYNA)
    ns = om.get_editor_property("nanite_settings")
    ns.enabled = False   # shaffof material Nanite'da ishlamaydi
    om.set_editor_property("nanite_settings", ns)
    ue.EditorStaticMeshLibrary.remove_collisions(om)
    om.set_material(0, oyna_material())
    eski = DEST + "/MI_Cobalt_Automotive_solar_glass"   # glTF dan kelgan mat shisha - endi kerak emas
    if EAL.does_asset_exist(eski) and not EAL.find_package_referencers_for_asset(eski):
        EAL.delete_asset(eski)
    nanite_belgisi()
    for p in EAL.list_assets(DEST, recursive=True, include_folder=False):
        EAL.save_asset(p.split(".")[0], only_if_is_dirty=False)
    return sm


def yozish_ruxsati():
    """git LFS 'lockable' .uasset fayllari checkout'dan keyin faqat o'qish uchun - saqlashdan oldin yozish ruxsati."""
    papka = os.path.join(PROJ, "Content", *DEST.split("/")[2:])
    for ildiz, _, fayllar in os.walk(papka):
        for f in fayllar:
            os.chmod(os.path.join(ildiz, f), stat.S_IREAD | stat.S_IWRITE)


def nanite_belgisi():
    """glTF materiallari Nanite/ISM belgisisiz keladi: belgisiz bo'lsa o'yinda kuzov qo'pol zaxira mesh bilan chiziladi."""
    yozish_ruxsati()
    for p in EAL.list_assets(DEST, recursive=True, include_folder=False):
        a = ue.load_asset(p.split(".")[0])
        if isinstance(a, ue.Material) and a.get_name() != "M_Cobalt_Oyna" and not (
                a.get_editor_property("used_with_nanite") and a.get_editor_property("used_with_instanced_static_meshes")):
            a.set_editor_property("used_with_nanite", True)
            a.set_editor_property("used_with_instanced_static_meshes", True)
            ue.MaterialEditingLibrary.recompile_material(a)
            EAL.save_loaded_asset(a, False)
            log("Nanite belgisi:", a.get_name())


def uzun_oq(sm):
    """Mesh'ning uzun o'qi (0=X, 1=Y), yo'nalish belgisi (+1: old tomoni o'qning + tomonida) va bbox."""
    b = sm.get_bounding_box()
    ext = [b.max.x - b.min.x, b.max.y - b.min.y]
    ax = 0 if ext[0] >= ext[1] else 1
    md = sm.get_static_mesh_description(0)
    lo, hi = (b.min.x, b.max.x) if ax == 0 else (b.min.y, b.max.y)
    top = {"bosh": -1e9, "oxir": -1e9}
    for i in range(md.get_vertex_count()):
        p = md.get_vertex_position(ue.VertexID(i))
        t = ((p.x if ax == 0 else p.y) - lo) / (hi - lo)
        if t < 0.12:
            top["bosh"] = max(top["bosh"], p.z)
        elif t > 0.88:
            top["oxir"] = max(top["oxir"], p.z)
    return ax, b, top


def yaw_of(ax, sign):
    # old tomoni lokal +X (yaw 0), +Y (90), -X (180), -Y (-90)
    return {(0, 1): 0.0, (1, 1): 90.0, (0, -1): 180.0, (1, -1): -90.0}[(ax, sign)]


def almashtir(sm):
    ue.EditorLoadingAndSavingUtils.load_map(MAP)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    cax, cb, ctop = uzun_oq(sm)
    log("Cobalt bbox", cb.min, cb.max, "uzun o'q", cax, "tepa", ctop)
    # Cobalt: old tomoni kapot (past) - qaysi chekkada tepa past bo'lsa, o'sha old
    csign = (1 if ctop["oxir"] < ctop["bosh"] else -1)
    cyaw = yaw_of(cax, csign)
    cmid = V((cb.min.x + cb.max.x) / 2, (cb.min.y + cb.max.y) / 2, cb.min.z)
    jami = 0
    for a in actors:
        for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
            old = c.static_mesh
            if not old or old.get_name() not in ESKI:
                continue
            ax, b, top = uzun_oq(old)
            # Damas (mikroavtobus): old tomoni tumshug'i past; Labo (pikap): kabina old tomonda baland
            if "Damas" in old.get_name():
                sign = 1 if top["oxir"] < top["bosh"] else -1
            else:
                sign = 1 if top["oxir"] > top["bosh"] else -1
            oyaw = yaw_of(ax, sign)
            omid = V((b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2, b.min.z)
            log(a.get_actor_label(), old.get_name(), "bbox", b.min, b.max, "tepa", top, "old yaw", oyaw)
            yangi = []
            for i in range(c.get_instance_count()):
                t = c.get_instance_transform(i, True)
                rot = t.rotation.rotator()
                # eski mashina markazi (tagi) dunyoda; Cobalt old tomoni eski mashina old tomoniga qaraydi
                markaz = t.transform_location(omid)
                yaw = rot.yaw + oyaw - cyaw
                r = ue.Rotator(roll=rot.roll, pitch=rot.pitch, yaw=yaw)
                tt = ue.Transform(V(0, 0, 0), r, V(1, 1, 1))
                loc = markaz - tt.transform_location(cmid)
                yangi.append(ue.Transform(loc, r, V(1, 1, 1)))
                log("  nusxa", i, "eski", t.translation, "yaw", round(rot.yaw), "-> yangi", loc, "yaw", round(yaw))
            c.modify()
            c.clear_instances()
            c.set_static_mesh(sm)
            c.set_editor_property("override_materials", [])
            for nt in yangi:
                c.add_instance(nt, True)
            jami += len(yangi)
    log("almashtirildi:", jami)
    oynalar(actors)
    if REJIM in ("hammasi", "oyna"):
        # git LFS 'lockable' fayllari faqat o'qish uchun bo'ladi - saqlashdan oldin yozish ruxsati
        umap = os.path.join(PROJ, "Content", "Bozor", "Maps", "Bozor.umap")
        os.chmod(umap, stat.S_IREAD | stat.S_IWRITE)
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
        ok = ue.EditorLoadingAndSavingUtils.save_map(world, MAP)
        ue.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)   # tashqi aktorlar (OFPA) bo'lsa
        log("sahna saqlandi:", ok)


def oynalar(actors):
    """Har bir SM_Cobalt nusxasiga ayni transformli SM_Cobalt_Oyna nusxasi (shaffof, soya va kolliziyasiz)."""
    om = ue.load_asset(OYNA)
    jami = 0
    for a in actors:
        comps = a.get_components_by_class(ue.InstancedStaticMeshComponent)
        tlar = [c.get_instance_transform(i, True) for c in comps if c.static_mesh and c.static_mesh.get_name() == "SM_Cobalt"
                for i in range(c.get_instance_count())]
        bor = [c for c in comps if c.static_mesh and c.static_mesh.get_name() == "SM_Cobalt_Oyna"]
        if not tlar and not bor:
            continue
        if bor:   # qayta ishga tushirilganda - eski oyna nusxalari yangilanadi
            c = bor[0]
            c.modify()
            c.clear_instances()
            for t in tlar:
                c.add_instance(t, True)
        elif isinstance(a, ue.BozorMeshGroup):
            a.add_instances(om, [], tlar, False, False)
        jami += len(tlar)
    log("oyna nusxalari:", jami)


def main():
    open(LOG, "w").close()
    sm = import_cobalt() if REJIM in ("import", "oyna") else ue.load_asset(MESH)
    if REJIM == "nanite":
        nanite_belgisi()
    b = sm.get_bounding_box()
    log("SM_Cobalt o'lchami (sm):", round(b.max.x - b.min.x), round(b.max.y - b.min.y), round(b.max.z - b.min.z),
        "material slotlari:", [str(m.material_slot_name) + "=" + (m.material_interface.get_name() if m.material_interface else "-")
                               for m in sm.static_materials])
    log("nanite:", sm.get_editor_property("nanite_settings").enabled)
    if REJIM in ("hammasi", "sinov", "oyna"):
        almashtir(sm)


main()
