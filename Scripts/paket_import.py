"""Yangi paketlar (Assets/Paket): qora, oq, sariq - qo'lda ushlangan holatda (Blender cloth: tutqichlar kaftda yig'ilgan).

FBX'larni Assets/Paket/Export ga Blender chiqaradi (pivot - kaft ushlaydigan nuqta, tutqichlar yuqorida, paket pastga osiladi).
Bu skript ularni /Game/Bozor/Paket ga import qiladi, ikki tomonlama PE plyonka materiali (M_Paket + MI_Paket_<rang>)
va LOD'lar qo'yadi. NPC'lar va bola shu meshlarni ishlatadi (ABozorNpcManager - C++ da yuklanadi).

Ishga tushirish: UnrealEditor-Cmd.exe BozorUnreal.uproject -run=pythonscript "-script=D:/Unreal Projects/BozorUnreal/Scripts/paket_import.py"
"""
import os
import sys

import unreal

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import bozor_common as bc

SRC = r"D:\Unreal Projects\BozorUnreal\Assets\Paket\Export"
DEST = bc.ROOT + "/Paket"
# Blender'dagi PE plyonka rangi (sRGB emas, chiziqli) va g'adir-budurligi
RANGLAR = {
    "Qora": ((0.006, 0.007, 0.008), 0.32),
    "Oq": ((0.80, 0.785, 0.735), 0.45),
    "Sariq": ((0.80, 0.62, 0.16), 0.40),   # Blender render (Paket_Ranglar.png) kabi och sariq
}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def material():
    path = DEST + "/M_Paket"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    m = tools.create_asset("M_Paket", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)   # plyonka bir qavat: ichi ham ko'rinsin
    rang = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -400, 0)
    rang.set_editor_property("parameter_name", "Rang")
    gadir = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 200)
    gadir.set_editor_property("parameter_name", "Gadir")
    gadir.set_editor_property("default_value", 0.4)
    spek = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 300)
    spek.set_editor_property("r", 0.5)
    mel.connect_material_property(rang, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(gadir, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(spek, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def instansiya(asos, rang, rgb, gadir):
    path = DEST + "/MI_Paket_" + rang
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        "MI_Paket_" + rang, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, asos)
    mel.set_material_instance_vector_parameter_value(mi, "Rang", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Gadir", gadir)
    eal.save_loaded_asset(mi)
    return mi


def main():
    pipeline = bc.make_pipeline("P_Paket", static=True)
    p = eal.load_asset(pipeline)
    p.mesh_pipeline.set_editor_property("build_nanite", False)   # NPC qo'lida, harakatda - oddiy LOD'lar
    eal.save_loaded_asset(p)
    asos = material()
    natija = []
    for rang, (rgb, gadir) in RANGLAR.items():
        fbx = os.path.join(SRC, "SM_Paket_%s.fbx" % rang)
        bc.interchange_import(fbx, DEST, pipeline)
        sm = eal.load_asset("%s/SM_Paket_%s" % (DEST, rang))
        if not sm:
            bc.log("paket: import bo'lmadi", fbx)
            continue
        mi = instansiya(asos, rang, rgb, gadir)
        for i in range(len(sm.static_materials)):
            sm.set_material(i, mi)
        # LOD: 9k uchburchak -> 50% -> 20% (olomonda ko'p NPC paket ko'taradi)
        opt = unreal.EditorScriptingMeshReductionOptions()
        opt.set_editor_property("reduction_settings", [
            unreal.EditorScriptingMeshReductionSettings(1.0, 1.0),
            unreal.EditorScriptingMeshReductionSettings(0.5, 0.35),
            unreal.EditorScriptingMeshReductionSettings(0.2, 0.12),
        ])
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if sub:
            sub.set_lods_with_notification(sm, opt, True)
        else:   # commandlet'da muharrir subsystem'lari yo'q
            unreal.EditorStaticMeshLibrary.set_lods(sm, opt)
        eal.save_loaded_asset(sm)
        b = sm.get_bounding_box()
        natija.append("%s: min %s max %s, lod %d" % (rang, b.min, b.max, sm.get_num_lods()))
    bc.log("paket:", natija)
    print("PAKET_NATIJA", natija)


main()
