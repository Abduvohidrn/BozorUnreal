"""BMW M5 lar uchun "01 S 100 SS" davlat raqami: tekstura (Scripts/bmw_raqam_chiz.ps1), material va old/orqa plita
mesh'i (Scripts/mashina_raqam_mesh.py -- BMW) ni import qiladi va sahnadagi har bir BMW nusxasiga plita qo'yadi.
Umumiy raqam atlasiga (32 joy, hammasi band) tegilmaydi - BMW uchun alohida tekstura va material.

UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/bmw_raqam_import.py   (muharrir yopiq)
"""
from pathlib import Path
import runpy
import stat

import unreal as ue

PROJECT = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
SOURCES = PROJECT / "Saved" / "MashinaRaqam"
ROOT = "/Game/Bozor/Mashinalar/Raqamlar"
MAP = "/Game/Bozor/Maps/Bozor"
EAL = ue.EditorAssetLibrary
MEL = ue.MaterialEditingLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def yozish(fayl):
    if fayl.exists():
        fayl.chmod(stat.S_IREAD | stat.S_IWRITE)   # git LFS 'lockable'


def tekstura():
    yozish(PROJECT / "Content" / "Bozor" / "Mashinalar" / "Raqamlar" / "T_Raqam_BMW.uasset")
    task = ue.AssetImportTask()
    task.set_editor_property("filename", str(SOURCES / "T_Raqam_BMW.png"))
    task.set_editor_property("destination_path", ROOT)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    TOOLS.import_asset_tasks([task])
    t = ue.load_asset(ROOT + "/T_Raqam_BMW")
    EAL.save_loaded_asset(t, False)
    return t


def material(t):
    """Plita UV (0..1) -> teksturadagi plita maydoni (16..496 x 12..116 piksel, 512x128)."""
    yol = ROOT + "/M_Raqam_BMW"
    if EAL.does_asset_exist(yol):
        return ue.load_asset(yol)
    m = TOOLS.create_asset("M_Raqam_BMW", ROOT, ue.Material, ue.MaterialFactoryNew())
    m.set_editor_property("used_with_instanced_static_meshes", True)
    uv = MEL.create_material_expression(m, ue.MaterialExpressionTextureCoordinate, -700, -200)
    cu = MEL.create_material_expression(m, ue.MaterialExpressionCustom, -480, -200)
    cu.set_editor_property("output_type", ue.CustomMaterialOutputType.CMOT_FLOAT2)
    cu.set_editor_property("description", "Plita maydoni")
    ci = ue.CustomInput()
    ci.set_editor_property("input_name", "UV")
    cu.set_editor_property("inputs", [ci])
    cu.set_editor_property("code", "return (UV * float2(480.0, 104.0) + float2(16.0, 12.0)) / float2(512.0, 128.0);")
    MEL.connect_material_expressions(uv, "", cu, "UV")
    s = MEL.create_material_expression(m, ue.MaterialExpressionTextureSample, -260, -200)
    s.set_editor_property("texture", t)
    MEL.connect_material_expressions(cu, "", s, "UVs")
    MEL.connect_material_property(s, "RGB", ue.MaterialProperty.MP_BASE_COLOR)
    r = MEL.create_material_expression(m, ue.MaterialExpressionConstant, -260, 60)
    r.set_editor_property("r", 0.58)   # umumiy raqamlar (M_Raqam_Shaxsiy) bilan bir xil
    MEL.connect_material_property(r, "", ue.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    return m


def main():
    mat = material(tekstura())
    yozish(PROJECT / "Content" / "Bozor" / "Mashinalar" / "Raqamlar" / "SM_Raqam_BMW.uasset")
    import_glb = runpy.run_path(str(PROJECT / "Scripts" / "qatnov_import.py"), run_name="kutubxona")["import_glb"]
    mesh = import_glb(str(SOURCES / "SM_Raqam_BMW.glb"), ROOT, "SM_Raqam_BMW", False)
    if not mesh or mesh.get_num_triangles(0) != 36:
        raise RuntimeError("SM_Raqam_BMW: old va orqa plita tekshiruvdan o'tmadi")
    mesh.set_material(0, mat)
    EAL.save_loaded_asset(mesh, False)

    ue.EditorLoadingAndSavingUtils.load_map(MAP)
    tlar, guruh, bor = [], None, None
    for a in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
        for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
            nom = c.static_mesh.get_name() if c.static_mesh else ""
            if nom == "SM_BMW":
                guruh = a
                tlar += [c.get_instance_transform(i, True) for i in range(c.get_instance_count())]
            elif nom == "SM_Raqam_BMW":
                bor = c
    if not tlar:
        raise RuntimeError("Sahnada BMW yo'q - avval Scripts/mashina_import.py (MASHINA=BMW)")
    if bor:   # qayta ishga tushirilganda - eski plitalar yangilanadi
        bor.modify()
        bor.clear_instances()
        for t in tlar:
            bor.add_instance(t, True)
    else:
        guruh.add_instances(mesh, [mat], tlar, False, False)
    yozish(PROJECT / "Content" / "Bozor" / "Maps" / "Bozor.umap")
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    ok = ue.EditorLoadingAndSavingUtils.save_map(world, MAP)
    ue.log_warning("BMWRAQAM %d ta BMW ga old va orqa raqam (01 S 100 SS), saqlandi: %s" % (len(tlar), ok))


main()
