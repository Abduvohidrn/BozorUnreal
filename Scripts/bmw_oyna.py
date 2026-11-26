"""BMW oynalari: to'q tonirovka (M_BMW_Oyna). BMW modelida salon to'liq yo'q (pol, oldingi panel) - asl modelda oynalar
qora edi; umumiy M_Cobalt_Oyna (yarim shaffof) bilan ichi bo'sh ko'rinardi. Turgan, qatnovdagi va haydaladigan BMW -
hammasi SM_BMW_Oyna ishlatadi, shuning uchun bitta o'zgarish yetarli.

UnrealEditor-Cmd BozorUnreal.uproject -run=pythonscript -script=Scripts/bmw_oyna.py   (muharrir yopiq)
"""
import os
import stat

import unreal as ue

PROJ = ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())
DEST = "/Game/Bozor/Mashinalar/BMW"
EAL = ue.EditorAssetLibrary
MEL = ue.MaterialEditingLibrary


def yozish(yol):
    f = os.path.join(PROJ, "Content", *yol.split("/")[2:]) + ".uasset"
    if os.path.exists(f):
        os.chmod(f, stat.S_IREAD | stat.S_IWRITE)   # git LFS 'lockable'


def material():
    yol = DEST + "/M_BMW_Oyna"
    if EAL.does_asset_exist(yol):
        EAL.delete_asset(yol)
    m = ue.AssetToolsHelpers.get_asset_tools().create_asset("M_BMW_Oyna", DEST, ue.Material, ue.MaterialFactoryNew())
    m.set_editor_property("blend_mode", ue.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("translucency_lighting_mode", ue.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    P = ue.MaterialProperty
    rang = MEL.create_material_expression(m, ue.MaterialExpressionConstant3Vector, -400, -200)
    rang.set_editor_property("constant", ue.LinearColor(0.004, 0.005, 0.006, 1))
    MEL.connect_material_property(rang, "", P.MP_BASE_COLOR)
    for qiymat, prop, y in ((0.0, P.MP_METALLIC, -60), (1.0, P.MP_SPECULAR, 0), (0.03, P.MP_ROUGHNESS, 60)):
        e = MEL.create_material_expression(m, ue.MaterialExpressionConstant, -400, y)
        e.set_editor_property("r", qiymat)
        MEL.connect_material_property(e, "", prop)
    # kuchli tonirovka: to'g'ri qaraganda 88%, chetdan deyarli to'liq qora - salon ichi bilinmaydi, aks yaxshi ko'rinadi
    fr = MEL.create_material_expression(m, ue.MaterialExpressionFresnel, -600, 200)
    fr.set_editor_property("exponent", 3.0)
    fr.set_editor_property("base_reflect_fraction", 0.0)
    lerp = MEL.create_material_expression(m, ue.MaterialExpressionLinearInterpolate, -400, 200)
    lerp.set_editor_property("const_a", 0.88)
    lerp.set_editor_property("const_b", 0.98)
    MEL.connect_material_expressions(fr, "", lerp, "Alpha")
    MEL.connect_material_property(lerp, "", P.MP_OPACITY)
    MEL.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    return m


def main():
    m = material()
    yol = DEST + "/SM_BMW_Oyna"
    yozish(yol)
    om = ue.load_asset(yol)
    for i, s in enumerate(om.static_materials):
        if str(s.material_slot_name) == "MI_BMW_TEX_017":   # oynalar (TEX_014 - fara qopqog'i, tiniq qoladi)
            om.set_material(i, m)
    EAL.save_loaded_asset(om, False)
    ue.log_warning("BMWOYNA %s" % [(str(s.material_slot_name), s.material_interface.get_name()) for s in om.static_materials])


main()
