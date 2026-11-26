"""Qatnov mashinalari: Scripts/qatnov_eksport.py chiqargan g'ildiraksiz kuzov (SM_<Nom>_Qatnov) va 4 ta g'ildirakni
/Game/Bozor/Mashinalar/<Nom>/Qatnov ga import qiladi. Materiallar import qilinmaydi - turgan mashina SM_<Nom> dagi bir xil
nomli slotlardan ko'chiriladi (rang va shisha bir xil bo'ladi). Natija Saved/qatnov_import.txt ga yoziladi;
sahnadagi Bozor_Qatnov aktori Scripts/qatnov_joyla.py bilan yangilanadi.

UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/qatnov_import.py   (muhit: MASHINALAR=Cobalt,Damas,...)
"""
import json
import os
import stat

import unreal as ue

PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
EAL = ue.EditorAssetLibrary
NOMLAR = [n for n in os.environ.get("MASHINALAR", "Cobalt,Damas,Spark,Nexia,Malibu").split(",") if n]
NANITE = {"Damas": False}   # Damas kuzovida shaffof oyna bor - Nanite'siz
LOG = os.path.join(PROJ, "Saved", "qatnov_import.txt")
lines = []


def log(*a):
    s = " ".join(str(x) for x in a)
    lines.append(s)
    ue.log_warning("QATNOV " + s)
    with open(LOG, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def pipeline(dest, nanite):
    path = dest + "/PL_Qatnov"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    src = ue.load_asset("/Interchange/Pipelines/DefaultAssetsPipeline.DefaultAssetsPipeline")
    p = ue.AssetToolsHelpers.get_asset_tools().duplicate_asset("PL_Qatnov", dest, src)
    p.set_editor_property("asset_type_sub_folders", False)
    p.set_editor_property("scene_name_sub_folder", False)
    p.set_editor_property("reimport_strategy", ue.ReimportStrategyFlags.APPLY_PIPELINE_PROPERTIES)
    p.material_pipeline.set_editor_property("import_materials", False)
    p.material_pipeline.texture_pipeline.set_editor_property("import_textures", False)
    p.common_meshes_properties.set_editor_property("recompute_normals", False)
    p.common_meshes_properties.set_editor_property("import_lods", False)
    mp = p.mesh_pipeline
    mp.set_editor_property("import_skeletal_meshes", False)
    mp.set_editor_property("build_nanite", nanite)
    mp.set_editor_property("collision", False)
    EAL.save_loaded_asset(p)
    return path


def import_glb(fayl, dest, nom, nanite):
    yol = dest + "/" + nom
    old_mesh = ue.load_asset(yol) if EAL.does_asset_exist(yol) else None
    mesh_editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    nanite_settings = mesh_editor.get_nanite_settings(old_mesh) if old_mesh else None
    pl = pipeline(dest, nanite)
    mgr = ue.InterchangeManager.get_interchange_manager_scripted()
    params = ue.ImportAssetParameters()
    params.is_automated = True
    params.replace_existing = True
    params.override_pipelines.append(ue.SoftObjectPath(pl))
    oldin = set(p.split(".")[0] for p in EAL.list_assets(dest, recursive=False, include_folder=False))
    mgr.import_asset(dest, mgr.create_source_data(fayl), params)
    EAL.delete_asset(pl)
    if not EAL.does_asset_exist(yol):
        yangi = [p.split(".")[0] for p in EAL.list_assets(dest, recursive=False, include_folder=False)
                 if p.split(".")[0] not in oldin and EAL.find_asset_data(p).asset_class_path.asset_name == "StaticMesh"]
        EAL.rename_asset(yangi[0], yol)
    mesh = ue.load_asset(yol)
    if nanite_settings and mesh_editor.get_nanite_settings(mesh) != nanite_settings:
        mesh_editor.set_nanite_settings(mesh, nanite_settings, True)
    return mesh


def materiallar(manba, mesh):
    """manba (SM_<Nom>) slot nomi bo'yicha materiallarni ko'chiradi; topilmasa - log."""
    bor = {str(s.material_slot_name): s.material_interface for s in manba.static_materials}
    yoq = []
    for i, s in enumerate(mesh.static_materials):
        nom = str(s.material_slot_name)
        if nom in bor:
            mesh.set_material(i, bor[nom])
        else:
            yoq.append(nom)
    return yoq


def main(nomlar=NOMLAR):
    open(LOG, "w").close()
    hisobot = os.path.join(PROJ, "Saved", "qatnov_mashinalar.json")
    natija = {}
    if os.path.exists(hisobot):
        with open(hisobot, encoding="utf-8") as f:
            natija = json.load(f)
    for nom in nomlar:
        src = os.path.join(PROJ, "Saved", nom + "Source")
        info = json.load(open(os.path.join(src, "qatnov.json"), encoding="utf-8"))["gildiraklar"]
        dest = "/Game/Bozor/Mashinalar/%s/Qatnov" % nom
        papka = os.path.join(PROJ, "Content", "Bozor", "Mashinalar", nom, "Qatnov")
        for ildiz, _, fs in os.walk(papka):   # git LFS 'lockable' - yozish ruxsati
            for f in fs:
                os.chmod(os.path.join(ildiz, f), stat.S_IREAD | stat.S_IWRITE)
        manba = ue.load_asset("/Game/Bozor/Mashinalar/%s/SM_%s" % (nom, nom))
        nanite = NANITE.get(nom, True)
        kuzov_nomi = "SM_%s_Qatnov" % nom
        old_path = f"{dest}/{kuzov_nomi}"
        old_mesh = ue.load_asset(old_path) if EAL.does_asset_exist(old_path) else None
        old_body = old_mesh.get_editor_property("body_setup") if old_mesh else None
        old_shapes = old_body.get_editor_property("agg_geom") if old_body else None
        old_trace = old_body.get_editor_property("collision_trace_flag") if old_body else None
        kuzov = import_glb(os.path.join(src, kuzov_nomi + ".glb"), dest, kuzov_nomi, nanite)
        yoq = materiallar(manba, kuzov)
        if old_shapes is not None:
            body = kuzov.get_editor_property("body_setup")
            body.set_editor_property("agg_geom", old_shapes)
            body.set_editor_property("collision_trace_flag", old_trace)
        else:
            ue.EditorStaticMeshLibrary.remove_collisions(kuzov)
            ue.EditorStaticMeshLibrary.add_simple_collisions(kuzov, ue.ScriptCollisionShapeType.NDOP18)
        gil = []
        for g in info:
            m = import_glb(os.path.join(src, g["nom"] + ".glb"), dest, g["nom"], nanite)
            yoq += materiallar(manba, m)
            ue.EditorStaticMeshLibrary.remove_collisions(m)
            gil.append(m)
        for p in EAL.list_assets(dest, recursive=False, include_folder=False):
            EAL.save_asset(p.split(".")[0], only_if_is_dirty=False)
        b = kuzov.get_bounding_box()
        log(nom, "kuzov", kuzov.get_num_triangles(0), "uchburchak, o'lcham", round(b.max.x - b.min.x), round(b.max.y - b.min.y),
            round(b.max.z - b.min.z), "g'ildirak", len(gil), "slot topilmadi:", sorted(set(yoq)))
        # Blender (x, y, z) m -> Unreal (x, -y, z) sm (glTF importi Y ni teskari qiladi)
        natija[nom] = {"kuzov": kuzov.get_path_name().split(".")[0],
                       "gildiraklar": [m.get_path_name().split(".")[0] for m in gil],
                       "joylar": [[g["markaz"][0] * 100, -g["markaz"][1] * 100, g["markaz"][2] * 100] for g in info],
                       "radius": info[0]["radius"] * 100}
    with open(hisobot, "w", encoding="utf-8") as f:
        json.dump(natija, f, indent=1)


if __name__ == "__main__":
    main()
